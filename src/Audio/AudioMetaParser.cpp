#include "../../Header/AudioPart/AudioMetaParser.h"
#include "../../Global/Global.h"

bool AudioMetaParser::s_ffmpegInitialized = false;

AudioMetaParser* AudioMetaParser::s_instance = nullptr;
QMutex AudioMetaParser::s_mutex;


AudioMetaParser* AudioMetaParser::getInstance() {
    if (!s_instance) {
        s_instance = new AudioMetaParser();
    }
    return s_instance;
}

AudioMetaParser::~AudioMetaParser()
{
    // FFmpeg 清理（可选）
}

void AudioMetaParser::initFFmpeg()
{
    if (!s_ffmpegInitialized) {
        avformat_network_init();
        av_log_set_level(AV_LOG_QUIET);  // 减少控制台输出
        s_ffmpegInitialized = true;
        qDebug() << "FFmpeg initialized, version:" << av_version_info();
    }
}

// 修改后的 parse 函数
bool AudioMetaParser::parse(const QString& filePath, AudioUtil::AudioMeta& meta, QString& error)
{
    // meta = AudioUtil::AudioMeta();
    meta.filePath = filePath;

    // 检查文件是否存在
    if (!QFile::exists(filePath)) {
        error = QString("文件不存在: %1").arg(filePath);
        return false;
    }

    // 1. 打开文件
    AVFormatContext* fmtCtx = nullptr;
    int ret = avformat_open_input(&fmtCtx, filePath.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        error = QString("无法打开文件: %1 (错误: %2)")
                    .arg(filePath)
                    .arg(errorCodeToString(ret));
        return false;
    }

    // 设置自定义读取超时（可选）
    fmtCtx->flags |= AVFMT_FLAG_NONBLOCK;

    // 2. 获取流信息
    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0) {
        error = QString("无法获取流信息: %1").arg(errorCodeToString(ret));
        avformat_close_input(&fmtCtx);
        return false;
    }

    // 3. 查找音频流
    int audioStreamIdx = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIdx = i;
            break;
        }
    }

    if (audioStreamIdx == -1) {
        error = "文件中没有找到音频流";
        avformat_close_input(&fmtCtx);
        return false;
    }

    // 4. 提取信息
    extractMetadata(fmtCtx, meta);
    extractFormatInfo(fmtCtx, audioStreamIdx, meta);

    // 5. 尝试提取封面（多种方法）
    // 首先检查是否已有封面文件
    QString coverDir = "../images/MusicCover";
    QString coverName = QString::fromStdString(filterFilename(meta.title.toStdString())) + ".png";
    QString coverPath = coverDir + "/" + coverName;

    meta.cover = loadExistingCover(coverPath);
    // meta.album = coverPath;

    if (meta.cover.isNull()) {
        // 如果没有，尝试从音频文件提取
        extractCover(fmtCtx, meta);
    }

    // 6. 释放资源
    avformat_close_input(&fmtCtx);

    error = "解析成功";
    return true;
}

void AudioMetaParser::extractMetadata(AVFormatContext* fmtCtx, AudioUtil::AudioMeta& meta)
{
    // 1. 从全局元数据读取
    AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_get(fmtCtx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        QString key = QString::fromUtf8(tag->key).toLower();
        QString value = QString::fromUtf8(tag->value).trimmed();

        if (key == "title") meta.title = value;
        else if (key == "artist" || key == "album_artist") meta.artist = value;
        else if (key == "album") meta.album = value;
        else if (key == "genre") meta.genre = value;
        else if (key == "year" || key == "date") meta.year = value;
        else if (key == "comment") meta.comment = value;
        else if (key == "track" || key == "tracknumber") {
            // 处理轨道号格式，如 "1/10"
            QString trackStr = value.split('/')[0];
            bool ok;
            int track = trackStr.toInt(&ok);
            if (ok) meta.track = track;
        }
        else if (key == "disc" || key == "discnumber") {
            // 可选：提取光盘号
        }
    }

    // 2. 如果没有标题，使用文件名
    if (meta.title.isEmpty()) {
        meta.title = QFileInfo(meta.filePath).completeBaseName();
    }

    // 3. 如果没有艺术家，尝试从文件名解析
    if (meta.artist.isEmpty()) {
        // 常见的命名格式：艺术家 - 标题
        QString fileName = QFileInfo(meta.filePath).completeBaseName();
        if (fileName.contains(" - ")) {
            QStringList parts = fileName.split(" - ");
            if (parts.size() >= 2) {
                meta.artist = parts[0].trimmed();
                if (meta.title == fileName) { // 如果标题还是文件名
                    meta.title = parts[1].trimmed();
                }
            }
        }
    }
}

void AudioMetaParser::extractFormatInfo(AVFormatContext* fmtCtx, int audioStreamIdx, AudioUtil::AudioMeta& meta)
{
    AVStream* audioStream = fmtCtx->streams[audioStreamIdx];
    AVCodecParameters* codecPar = audioStream->codecpar;

    // 容器格式
    if (fmtCtx->iformat->long_name) {
        meta.format = QString::fromUtf8(fmtCtx->iformat->long_name);
    } else if (fmtCtx->iformat->name) {
        meta.format = QString::fromUtf8(fmtCtx->iformat->name);
    }

    // 编码器
    const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    if (codec) {
        meta.codec = codec->long_name ? QString::fromUtf8(codec->long_name)
                                      : QString::fromUtf8(codec->name);
    } else {
        meta.codec = "未知编码";
    }

    // 时长
    if (fmtCtx->duration != AV_NOPTS_VALUE) {
        meta.durationMs = static_cast<int>(fmtCtx->duration / 1000); // 微秒→毫秒
    } else if (audioStream->duration != AV_NOPTS_VALUE) {
        // 尝试从流获取时长
        AVRational time_base = audioStream->time_base;
        meta.durationMs = static_cast<int>(audioStream->duration * 1000 * time_base.num / time_base.den);
    }

    // 技术参数
    meta.sampleRate = codecPar->sample_rate;
    meta.channels = codecPar->channels;
    meta.bitRate = codecPar->bit_rate > 0 ? static_cast<int>(codecPar->bit_rate / 1000) : 0;

    // 计算比特率（如果元数据中没有）
    if (meta.bitRate == 0 && meta.durationMs > 0) {
        QFileInfo fileInfo(meta.filePath);
        qint64 fileSize = fileInfo.size(); // 字节
        meta.bitRate = static_cast<int>((fileSize * 8) / (meta.durationMs / 1000.0) / 1000);
    }
}

bool AudioMetaParser::extractCover(AVFormatContext* fmtCtx, AudioUtil::AudioMeta& meta)
{
    // 方法1：尝试从APIC标签提取（ID3v2等）
    if (extractAPICCover(fmtCtx, meta)) {
        return true;
    }

    // 方法2：尝试从视频流提取
    if (extractEmbeddedCover(fmtCtx, meta)) {
        return true;
    }

    return false;
}

bool AudioMetaParser::extractAPICCover(AVFormatContext* fmtCtx, AudioUtil::AudioMeta& meta)
{
    // 遍历所有流，查找附件流（通常包含封面）
    for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i) {
        AVStream* stream = fmtCtx->streams[i];
        AVCodecParameters* codecPar = stream->codecpar;

        // 检查是否是附件流或包含封面
        if (codecPar->codec_type == AVMEDIA_TYPE_ATTACHMENT ||
            (codecPar->codec_type == AVMEDIA_TYPE_VIDEO &&
             (codecPar->codec_id == AV_CODEC_ID_PNG ||
              codecPar->codec_id == AV_CODEC_ID_MJPEG ||
              codecPar->codec_id == AV_CODEC_ID_BMP))) {

            // 读取附件数据
            AVPacket packet;
            av_init_packet(&packet);

            while (av_read_frame(fmtCtx, &packet) >= 0) {
                if (packet.stream_index == i) {
                    // 尝试解码为图像
                    AVCodec* codec = const_cast<AVCodec*>(avcodec_find_decoder(codecPar->codec_id));
                    if (!codec) {
                        av_packet_unref(&packet);
                        continue;
                    }

                    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
                    if (!codecCtx) {
                        av_packet_unref(&packet);
                        continue;
                    }

                    if (avcodec_parameters_to_context(codecCtx, codecPar) < 0 ||
                        avcodec_open2(codecCtx, codec, nullptr) < 0) {
                        avcodec_free_context(&codecCtx);
                        av_packet_unref(&packet);
                        continue;
                    }

                    AVFrame* frame = av_frame_alloc();
                    if (!frame) {
                        avcodec_free_context(&codecCtx);
                        av_packet_unref(&packet);
                        continue;
                    }

                    int ret = avcodec_send_packet(codecCtx, &packet);
                    if (ret >= 0) {
                        ret = avcodec_receive_frame(codecCtx, frame);
                        if (ret >= 0) {
                            // 成功解码，保存封面
                            QString coverDir = "../images/MusicCover";
                            QString coverName = QString::fromStdString(filterFilename(meta.title.toStdString())) + ".png";
                            QString coverPath = coverDir + "/" + coverName;

                            ensureCoverDirectory(coverDir);

                            if (encodePNG(coverPath, frame)) {
                                meta.cover = QPixmap(coverPath);
                                av_frame_free(&frame);
                                avcodec_free_context(&codecCtx);
                                av_packet_unref(&packet);
                                return true;
                            }
                        }
                    }

                    av_frame_free(&frame);
                    avcodec_free_context(&codecCtx);
                }
                av_packet_unref(&packet);
            }
        }
    }

    return false;
}

bool AudioMetaParser::extractEmbeddedCover(AVFormatContext* fmtCtx, AudioUtil::AudioMeta& meta)
{
    // 查找视频流
    int videoStreamIdx = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIdx = i;
            break;
        }
    }

    if (videoStreamIdx == -1) {
        return false;
    }

    AVStream* videoStream = fmtCtx->streams[videoStreamIdx];
    AVCodecParameters* codecPar = videoStream->codecpar;

    // 查找解码器
    const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        return false;
    }

    // 创建解码上下文
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        return false;
    }

    if (avcodec_parameters_to_context(codecCtx, codecPar) < 0) {
        avcodec_free_context(&codecCtx);
        return false;
    }

    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        avcodec_free_context(&codecCtx);
        return false;
    }

    // 读取并解码视频帧
    AVPacket packet;
    AVFrame* frame = av_frame_alloc();
    bool coverSaved = false;

    av_init_packet(&packet);

    while (av_read_frame(fmtCtx, &packet) >= 0 && !coverSaved) {
        if (packet.stream_index == videoStreamIdx) {
            if (avcodec_send_packet(codecCtx, &packet) >= 0) {
                while (avcodec_receive_frame(codecCtx, frame) >= 0) {
                    // 保存第一帧作为封面
                    QString coverDir = "../images/MusicCover";
                    QString coverName = QString::fromStdString(filterFilename(meta.title.toStdString())) + ".png";
                    QString coverPath = coverDir + "/" + coverName;

                    ensureCoverDirectory(coverDir);

                    if (encodePNG(coverPath, frame)) {
                        meta.cover = QPixmap(coverPath);
                        coverSaved = true;
                        break;
                    }
                }
            }
        }
        av_packet_unref(&packet);
    }

    // 清理
    av_frame_free(&frame);
    avcodec_free_context(&codecCtx);

    return coverSaved;
}

bool AudioMetaParser::encodePNG(const QString& outputPath, AVFrame* frame)
{
    if (!frame || frame->width <= 0 || frame->height <= 0) {
        return false;
    }

    // 查找 PNG 编码器
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!codec) {
        qWarning() << "未找到 PNG 编码器";
        return false;
    }

    // 创建编码上下文
    AVCodecContext* encCtx = avcodec_alloc_context3(codec);
    if (!encCtx) {
        return false;
    }

    // 配置编码参数
    encCtx->width = frame->width;
    encCtx->height = frame->height;
    encCtx->pix_fmt = AV_PIX_FMT_RGBA;
    encCtx->time_base = {1, 1};

    // 设置压缩级别（0-9，0=无压缩，9=最大压缩）
    av_opt_set_int(encCtx->priv_data, "compression_level", 3, 0);

    // 打开编码器
    if (avcodec_open2(encCtx, codec, nullptr) < 0) {
        avcodec_free_context(&encCtx);
        return false;
    }

    // 如果需要，转换帧格式到 RGBA
    AVFrame* rgbaFrame = nullptr;
    SwsContext* swsCtx = nullptr;

    if (frame->format != AV_PIX_FMT_RGBA) {
        swsCtx = sws_getContext(
            frame->width, frame->height, (AVPixelFormat)frame->format,
            frame->width, frame->height, AV_PIX_FMT_RGBA,
            SWS_BICUBIC, nullptr, nullptr, nullptr
        );

        if (!swsCtx) {
            avcodec_free_context(&encCtx);
            return false;
        }

        rgbaFrame = av_frame_alloc();
        rgbaFrame->width = frame->width;
        rgbaFrame->height = frame->height;
        rgbaFrame->format = AV_PIX_FMT_RGBA;

        if (av_frame_get_buffer(rgbaFrame, 0) < 0) {
            sws_freeContext(swsCtx);
            av_frame_free(&rgbaFrame);
            avcodec_free_context(&encCtx);
            return false;
        }

        sws_scale(swsCtx,
                  frame->data, frame->linesize, 0, frame->height,
                  rgbaFrame->data, rgbaFrame->linesize);
    } else {
        rgbaFrame = frame;
    }

    // 创建输出文件
    FILE* file = fopen(outputPath.toUtf8().constData(), "wb");
    if (!file) {
        if (swsCtx) sws_freeContext(swsCtx);
        if (rgbaFrame != frame) av_frame_free(&rgbaFrame);
        avcodec_free_context(&encCtx);
        return false;
    }

    // 编码并写入文件
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = nullptr;
    pkt.size = 0;

    int ret = avcodec_send_frame(encCtx, rgbaFrame);
    if (ret < 0) {
        fclose(file);
        if (swsCtx) sws_freeContext(swsCtx);
        if (rgbaFrame != frame) av_frame_free(&rgbaFrame);
        avcodec_free_context(&encCtx);
        return false;
    }

    bool success = false;
    while (ret >= 0) {
        ret = avcodec_receive_packet(encCtx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            break;
        }

        fwrite(pkt.data, 1, pkt.size, file);
        av_packet_unref(&pkt);
        success = true;
    }

    // 清理资源
    fclose(file);
    if (swsCtx) sws_freeContext(swsCtx);
    if (rgbaFrame != frame) av_frame_free(&rgbaFrame);
    avcodec_free_context(&encCtx);

    return success;
}

QPixmap AudioMetaParser::loadExistingCover(const QString& filePath)
{
    if (QFile::exists(filePath)) {
        QPixmap pixmap(filePath);
        if (!pixmap.isNull()) {
            return pixmap;
        }
    }

    // 尝试其他格式
    QString basePath = filePath.left(filePath.lastIndexOf('.'));
    QStringList extensions = {"jpg", "jpeg", "png", "bmp", "gif"};

    for (const QString& ext : extensions) {
        QString path = basePath + "." + ext;
        if (QFile::exists(path)) {
            QPixmap pixmap(path);
            if (!pixmap.isNull()) {
                return pixmap;
            }
        }
    }

    return QPixmap();
}

void AudioMetaParser::ensureCoverDirectory(const QString& dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

std::string AudioMetaParser::filterFilename(const std::string& filename)
{
    std::string filtered = filename;
    const std::string invalid = "/\\:*?\"<>|";

    for (char c : invalid) {
        size_t pos = 0;
        while ((pos = filtered.find(c, pos)) != std::string::npos) {
            filtered.replace(pos, 1, "_");
            pos += 1;
        }
    }

    // 限制长度
    if (filtered.length() > 100) {
        filtered = filtered.substr(0, 100);
    }

    return filtered;
}

QString AudioMetaParser::errorCodeToString(int errCode)
{
    char errBuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errCode, errBuf, sizeof(errBuf));
    return QString::fromUtf8(errBuf);
}

QPixmap AudioMetaParser::parseAudioFile(const QString& filePath, AudioUtil::AudioMeta& meta)
{
    QString err;
    parse(filePath,meta,err);
    return meta.cover;
}
