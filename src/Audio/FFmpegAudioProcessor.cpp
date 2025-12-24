//
// Created by lsy on 2025/9/29.
//

#ifdef __STDC_CONSTANT_MACROS
#warning "__STDC_CONSTANT_MACROS is defined"
#else
#error "__STDC_CONSTANT_MACROS is NOT defined "
#endif


#include "../../Header/AudioPart/FFmpegAudioProcessor.h"

#include <QFileInfo>

FFmpegAudioProcessor::FFmpegAudioProcessor()
{
    initFFmpeg();
}

FFmpegAudioProcessor::~FFmpegAudioProcessor()
{

}

void FFmpegAudioProcessor::initFFmpeg()
{
    static bool initialized = false;
    if (!initialized)
    {
        avformat_network_init(); // 替代avformat_network_init()，初始化网络和格式

        initialized = true;
    }
}

bool FFmpegAudioProcessor::parseAudioInfo(const QString& filePath, AudioMeta& info, QString& error)
{
    info = AudioMeta();
    info.filePath = filePath;

    /*AVFormatContext主要起到了管理和存储媒体文件相关信息的作用。
     *它是一个比较重要的结构体，在FFmpeg中用于表示媒体文件的格式上下文，其中包含了已经打开的媒体文件的详细信息
    包括媒体文件的格式、媒体流的信息、各个媒体流的编码格式、时长、码率等。*/
    AVFormatContext* fmtCtx  = nullptr;
    int ret = 0;

    // 打开文件

    // 貌似ret是记录打开文件的log或者是debug,正常情况下应该是为0的
    // 好了，现在是这样的 ret 貌似为流信息，如果错误的话是error的错误信息，需要用avErrorToString主动去处理
    ret = avformat_open_input(&fmtCtx,filePath.toUtf8().constData(),nullptr,nullptr);
    if (ret != 0)
    {
        error = QString("无法打开文件:%1 (错误:%2)").arg(filePath).arg(avErrorToString(ret));
    }

    // 获取流信息
    ret = avformat_find_stream_info(fmtCtx,nullptr);
    if (ret < 0)
    {
        error = QString("解析Stream信息失败:%1").arg(avErrorToString(ret));
        avformat_close_input(&fmtCtx);
        return false;
    }

    // 查找音频Stream
    int audioStreamIndex = -1;

    /*在使用 FFmpeg 进行媒体文件处理时，nb_streams 是一个非常重要的字段。
     *它表示媒体文件中包含的流的数量。流是媒体文件的基本组成部分，可以包含音频、视频、字幕等。*/
    for (unsigned int i = 0; i < fmtCtx->nb_streams; i++)
    {
        // 现在难道是进行逐个读取Stream？
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audioStreamIndex = i;
            break; //遇到 AVMEDIA_TYPE_AUDIO直接break出循环，这是为什么呢？
        }
    }
    if (audioStreamIndex == -1)
    {
        error = QString("未识别到音频Stream");
        avformat_close_input(&fmtCtx);
        return false;
    }

    // 解析Meta和Info
    parseMetadata(fmtCtx,info);
    parseFormatInfo(fmtCtx,audioStreamIndex,info);

    //资源释放
    avformat_close_input(&fmtCtx);
    return true;

}

bool FFmpegAudioProcessor::extractCover(const QString& filePath, QImage& cover, QString& error)
{
    cover = QImage();

    AVFormatContext* fmtCtx  = nullptr;
    int ret = avformat_open_input(&fmtCtx,filePath.toUtf8().constData(),nullptr,nullptr);
    if (ret != 0)
    {
        error = QString("打开文件失败：%1").arg(avErrorToString(ret));
        return false;
    }

    avformat_find_stream_info(fmtCtx,nullptr);

    // 查找封面的Stream
    int coverStreamIndex = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; i++)
    {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            // 封面流通常是单帧，且宽度/高度较小（排除真正的视频流）
            if (fmtCtx->streams[i]->nb_frames == 1) {
                coverStreamIndex = i;
                break;
            }
        }
    }
    if (coverStreamIndex == -1)
    {
        error = QString("Stream里并未包含封面");
        avformat_close_input(&fmtCtx);
        return false;
    }
    // 初始化解码器
    AVCodecParameters* codeParam = fmtCtx->streams[coverStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codeParam->codec_id); //Try to find a DECODER

    if (!codec)
    {
        error = QString("不支持的封面解码器");
        avformat_close_input(&fmtCtx);
        return false;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codeParam);
    ret = avcodec_open2(codecCtx, codec, nullptr);
    if (ret != 0) {
        error = QString("解码器打开失败：%1").arg(avErrorToString(ret));
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return false;
    }

    // 读取封面帧
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    bool gotFrame = false;

    while (av_read_frame(fmtCtx, pkt) >= 0) {
        if (pkt->stream_index != coverStreamIndex) {
            av_packet_unref(pkt);
            continue;
        }

        ret = avcodec_send_packet(codecCtx, pkt);
        if (ret < 0) break;

        ret = avcodec_receive_frame(codecCtx, frame);
        if (ret == 0) {
            gotFrame = true;
            break;
        }

        av_packet_unref(pkt);
    }

    // 转换为QImage（YUV→RGB32）
    if (gotFrame) {
        SwsContext* swsCtx = sws_getContext(
            frame->width, frame->height, (AVPixelFormat)frame->format,
            frame->width, frame->height, AV_PIX_FMT_RGB32,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        if (swsCtx) {
            // 分配RGB32缓冲区
            uint8_t* rgbData[1] = { nullptr };
            int rgbLinesize[1] = { 0 };
            av_image_alloc(rgbData, rgbLinesize, frame->width, frame->height, AV_PIX_FMT_RGB32, 1);

            // 转换像素格式
            sws_scale(
                swsCtx, frame->data, frame->linesize, 0, frame->height,
                rgbData, rgbLinesize
            );

            // 创建QImage（注意数据指针的生命周期）
            cover = QImage(
                rgbData[0], frame->width, frame->height,
                rgbLinesize[0], QImage::Format_RGB32
            ).copy(); // 深拷贝，避免数据释放后失效

            // 释放资源
            av_freep(&rgbData[0]);
            sws_freeContext(swsCtx);
        }
    }

    // 清理
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);

    if (cover.isNull()) {
        error = "封面解码失败";
        return false;
    }
    return true;
}

bool FFmpegAudioProcessor::extractAudioFromVideo(const QString& videoPath, const QString& outputAudioPath, QString& error) {
    AVFormatContext* inFmtCtx = nullptr;
    int ret = avformat_open_input(&inFmtCtx, videoPath.toUtf8().constData(), nullptr, nullptr);
    if (ret != 0) {
        error = QString("无法打开视频文件：%1").arg(avErrorToString(ret));
        return false;
    }
    avformat_find_stream_info(inFmtCtx, nullptr);

    // 查找音频流
    int audioStreamIdx = -1;
    for (unsigned int i = 0; i < inFmtCtx->nb_streams; ++i) {
        if (inFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIdx = i;
            break;
        }
    }
    if (audioStreamIdx == -1) {
        error = "视频中未找到音频流";
        avformat_close_input(&inFmtCtx);
        return false;
    }

    // 创建输出文件上下文
    AVFormatContext* outFmtCtx = nullptr;
    ret = avformat_alloc_output_context2(&outFmtCtx, nullptr, nullptr, outputAudioPath.toUtf8().constData());
    if (ret < 0) {
        error = QString("无法创建输出文件上下文：%1").arg(avErrorToString(ret));
        avformat_close_input(&inFmtCtx);
        return false;
    }

    // 复制音频流参数到输出
    AVStream* inStream = inFmtCtx->streams[audioStreamIdx];
    AVStream* outStream = avformat_new_stream(outFmtCtx, nullptr);
    if (!outStream) {
        error = "创建输出流失败";
        avformat_free_context(outFmtCtx);
        avformat_close_input(&inFmtCtx);
        return false;
    }
    ret = avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
    if (ret < 0) {
        error = QString("复制流参数失败：%1").arg(avErrorToString(ret));
        avformat_free_context(outFmtCtx);
        avformat_close_input(&inFmtCtx);
        return false;
    }
    outStream->codecpar->codec_tag = 0;

    // 打开输出文件
    if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&outFmtCtx->pb, outputAudioPath.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            error = QString("无法打开输出文件：%1").arg(avErrorToString(ret));
            avformat_free_context(outFmtCtx);
            avformat_close_input(&inFmtCtx);
            return false;
        }
    }

    // 写入文件头
    ret = avformat_write_header(outFmtCtx, nullptr);
    if (ret < 0) {
        error = QString("写入文件头失败：%1").arg(avErrorToString(ret));
        avio_closep(&outFmtCtx->pb);
        avformat_free_context(outFmtCtx);
        avformat_close_input(&inFmtCtx);
        return false;
    }

    // 读取并写入音频数据包
    AVPacket* pkt = av_packet_alloc();
    while (av_read_frame(inFmtCtx, pkt) >= 0) {
        if (pkt->stream_index == audioStreamIdx) {
            // 调整时间戳和流索引
            av_packet_rescale_ts(pkt, inStream->time_base, outStream->time_base);
            pkt->stream_index = 0;
            pkt->pos = -1; // 避免错误

            ret = av_interleaved_write_frame(outFmtCtx, pkt);
            if (ret < 0) {
                qWarning() << "写入数据包失败：" << avErrorToString(ret);
                break;
            }
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    // 写入文件尾
    av_write_trailer(outFmtCtx);

    // 清理资源
    if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&outFmtCtx->pb);
    }
    avformat_free_context(outFmtCtx);
    avformat_close_input(&inFmtCtx);

    return true;
}

void FFmpegAudioProcessor::parseMetadata(AVFormatContext* fmtCtx, AudioMeta& info) {

    // 从AVDictionary提取元数据（标签）
    AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_get(fmtCtx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        QString key = QString::fromUtf8(tag->key).toLower();
        QString value = QString::fromUtf8(tag->value);

        if (key == "title") info.title = value;
        else if (key == "artist") info.artist = value;
        else if (key == "album") info.album = value;
        else if (key == "genre") info.genre = value;
        else if (key == "year") info.year = value;
        else if (key == "comment") info.comment = value;
        else if (key == "track") info.track = value.toInt();
    }

    // 若标题为空，用文件名代替
    if (info.title.isEmpty()) {
        info.title = QFileInfo(info.filePath).fileName();
    }
}

void FFmpegAudioProcessor::parseFormatInfo(AVFormatContext* fmtCtx, int audioStreamIdx, AudioMeta& info) {
    AVStream* audioStream = fmtCtx->streams[audioStreamIdx];
    AVCodecParameters* codecPar = audioStream->codecpar;

    // 容器格式
    info.format = QString::fromUtf8(fmtCtx->iformat->long_name ?: fmtCtx->iformat->name);

    // 音频编码
    const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    if (codec) {
        info.codec = QString::fromUtf8(codec->long_name ?: codec->name);
    }

    // 时长（毫秒）
    if (fmtCtx->duration != AV_NOPTS_VALUE) {
        info.durationMs = static_cast<int>(fmtCtx->duration / AV_TIME_BASE * 1000);
    }

    // 采样率、声道数、比特率
    info.sampleRate = codecPar->sample_rate;
    info.channels = codecPar->channels;
    info.bitRate = codecPar->bit_rate / 1000; // 转换为kbps

    // 采样格式
    info.sampleFmt = static_cast<AVSampleFormat>(codecPar->format);
}

QString FFmpegAudioProcessor::avErrorToString(int errCode) {
    char errBuf[1024] = {0};
    av_strerror(errCode, errBuf, sizeof(errBuf));
    return QString::fromUtf8(errBuf);
}
