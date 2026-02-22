//
// Created by lsy on 2026/1/17.
//

#include "../../../Header/VideoPart/AVDataFetch/LocalFileDataFetcher.h"
#include <QFileInfo>
#include <QDebug>

LocalFileDataFetcher::LocalFileDataFetcher(QObject * parent) : AVDataFetcher(parent)
{
    avformat_network_init();

    // ========== 新增：音视频独立队列初始化 ==========
    audio_queue_ = std::make_shared<AVDataQueue>();
    video_queue_ = std::make_shared<AVDataQueue>();

    // 原有初始化逻辑（完全保留）
    format_ctx_ = nullptr;
    video_codec_ = nullptr;
    audio_codec_ = nullptr;
    video_codec_ctx_ = nullptr;
    audio_codec_ctx_ = nullptr;

    should_stop_ = true;
    is_paused_ = false;
    is_eos_ = false;
    file_path_ = "";

    file_info_.videoStreamIndex = -1;
    file_info_.audioStreamIndex = -1;
    file_info_.videoWidth = 0;
    file_info_.videoHeight = 0;
    file_info_.fps = 0.0;
    file_info_.durationMs = 0;
    file_info_.bitrate = 0;

    ring_buffer_ = std::make_unique<RingBuffer<PacketWrapper>>(RING_BUFFER_CAPACITY);

    qDebug() << "[LocalFileDataFetcher] 构造函数完成，环形缓冲区容量:" << RING_BUFFER_CAPACITY;
}

LocalFileDataFetcher::~LocalFileDataFetcher()
{
    cleanup();
}

// ========== 核心：原有init逻辑 → 搬家到doInit ==========
bool LocalFileDataFetcher::doInit()
{
    qDebug() << "[LocalFileDataFetcher::doInit] === 开始 ===";
    qDebug() << "  source_info_.isValid =" << source_info_.isValid;
    qDebug() << "  source_info_.mode =" << static_cast<int>(source_info_.mode);
    qDebug() << "  source_info_.url =" << source_info_.url;
    qDebug() << "  source_info_.nativePath =" << source_info_.nativePath;

    if (!source_info_.isValid) {
        qDebug() << "  错误：SourceInfo 无效";
        return false;
    }

    // 获取正确的文件路径（原有逻辑保留）
    QString filePath;
    if (!source_info_.nativePath.isEmpty()) {
        filePath = source_info_.nativePath;
        qDebug() << "  使用 nativePath:" << filePath;
    } else if (!source_info_.url.isEmpty()) {
        filePath = source_info_.url;
        if (filePath.startsWith("file://")) {
            filePath = filePath.mid(7);
            qDebug() << "  移除 file:// 前缀，新路径:" << filePath;
        } else {
            qDebug() << "  使用 url:" << filePath;
        }
    } else {
        qDebug() << "  错误：没有文件路径";
        return false;
    }
    file_path_ = filePath;

    // 文件存在性检查（原有逻辑保留）
    QFileInfo fileInfo(file_path_);
    qDebug() << "  检查文件是否存在:";
    qDebug() << "    路径:" << file_path_;
    qDebug() << "    存在:" << fileInfo.exists();
    qDebug() << "    是文件:" << fileInfo.isFile();
    qDebug() << "    大小:" << fileInfo.size() << "字节";
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qDebug() << "  错误：文件不存在或不是普通文件";
        return false;
    }

    // FFmpeg上下文初始化（原有逻辑保留）
    format_ctx_ = avformat_alloc_context();
    if (!format_ctx_) {
        qDebug() << "  错误：无法分配format context";
        return false;
    }
    QByteArray file_path_utf8 = file_path_.toUtf8();
    const char* file_path_cstr = file_path_utf8.constData();
    int ret = avformat_open_input(&format_ctx_, file_path_cstr, nullptr, nullptr);
    if (ret != 0) {
        char error_buffer[256];
        av_strerror(ret, error_buffer, sizeof(error_buffer));
        qDebug() << "  错误：无法打开输入文件，错误码:" << ret << ", 错误信息:" << error_buffer;
        avformat_free_context(format_ctx_);
        format_ctx_ = nullptr;
        return false;
    }
    if (avformat_find_stream_info(format_ctx_, nullptr) < 0) {
        qDebug() << "  错误：无法获取流信息";
        avformat_close_input(&format_ctx_);
        format_ctx_ = nullptr;
        return false;
    }

    // 解析流信息（仅新增音视频时间基）
    file_info_.videoStreamIndex = -1;
    file_info_.audioStreamIndex = -1;
    for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
        AVStream* stream = format_ctx_->streams[i];
        AVCodecParameters* codecpar = stream->codecpar;

        // 视频流（新增时间基赋值）
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO && file_info_.videoStreamIndex < 0) {
            file_info_.videoStreamIndex = i;
            video_codec_ = avcodec_find_decoder(codecpar->codec_id);
            if (video_codec_) {
                video_codec_ctx_ = avcodec_alloc_context3(video_codec_);
                avcodec_parameters_to_context(video_codec_ctx_, codecpar);
                if (avcodec_open2(video_codec_ctx_, video_codec_, nullptr) >= 0) {
                    file_info_.videoWidth = codecpar->width;
                    file_info_.videoHeight = codecpar->height;
                    file_info_.videoCodec = QString(video_codec_->name);
                    file_info_.fps = av_q2d(stream->avg_frame_rate);
                    video_time_base_ = stream->time_base; // 新增：视频时间基
                    qDebug() << "    视频流: 索引=" << i << ", 分辨率=" << file_info_.videoWidth << "x" << file_info_.videoHeight << ", FPS=" << file_info_.fps << ", 时间基=" << video_time_base_.num << "/" << video_time_base_.den;
                }
            }
        }
        // 音频流（新增时间基赋值）
        else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO && file_info_.audioStreamIndex < 0) {
            file_info_.audioStreamIndex = i;
            audio_codec_ = avcodec_find_decoder(codecpar->codec_id);
            if (audio_codec_) {
                audio_codec_ctx_ = avcodec_alloc_context3(audio_codec_);
                avcodec_parameters_to_context(audio_codec_ctx_, codecpar);
                if (avcodec_open2(audio_codec_ctx_, audio_codec_, nullptr) >= 0) {
                    file_info_.audioCodec = QString(audio_codec_->name);
                    audio_time_base_ = stream->time_base; // 新增：音频时间基
                    qDebug() << "    音频流: 索引=" << i << ", 编码器=" << file_info_.audioCodec << ", 时间基=" << audio_time_base_.num << "/" << audio_time_base_.den;
                }
            }
        }
    }

    // 文件信息打印（原有逻辑保留）
    if (format_ctx_->duration != AV_NOPTS_VALUE) {
        file_info_.durationMs = format_ctx_->duration / (AV_TIME_BASE / 1000);
    } else {
        file_info_.durationMs = 0;
    }
    if (format_ctx_->bit_rate > 0) {
        file_info_.bitrate = format_ctx_->bit_rate / 1000;
    } else {
        file_info_.bitrate = 0;
    }
    qDebug() << "  文件信息: 时长=" << file_info_.durationMs << "ms, 码率=" << file_info_.bitrate << "kbps";
    qDebug() << "[LocalFileDataFetcher::doInit] === 初始化成功 ===";
    return true;
}
// ========== 原有start逻辑 → 搬家到doStart ==========

bool LocalFileDataFetcher::doStart()
{
    qDebug() << "[LocalFileDataFetcher::doStart] === 开始 ===";

    if (!format_ctx_) {
        qDebug() << "[LocalFileDataFetcher::doStart] 错误：format_ctx_ 为空，请先调用 init()";
        return false;
    }
    if (read_thread_.joinable() || process_thread_.joinable()) {
        qDebug() << "[LocalFileDataFetcher::doStart] 警告：线程已经在运行";
        return false;
    }

    // 重置状态（新增清空音视频队列）
    should_stop_ = false;
    is_paused_ = false;
    is_eos_ = false;
    ring_buffer_->clear();
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        audio_queue_->clear();  // 新增：清空音频队列
        video_queue_->clear();  // 新增：清空视频队列
    }

    // 启动线程（原有逻辑保留）
    read_thread_ = std::thread([this]() { readFileThread(); });
    process_thread_ = std::thread([this]() { processBufferThread(); });

    qDebug() << "[LocalFileDataFetcher::doStart] === 启动成功（双线程+音视频分流）===";
    return true;
}


// ========== 原有私有函数，完全不变 ==========
void LocalFileDataFetcher::readFileThread()
{
    qDebug() << "[readFileThread] 开始：快速读取到环形缓冲区（新增DTS排序）";
    QTime cuT1 = QTime::currentTime();

    AVPacket* packet = av_packet_alloc();
    int packet_count = 0;
    int dropped_count = 0;
    int video_packet_count = 0;
    int audio_packet_count = 0;

    while (!should_stop_ && av_read_frame(format_ctx_, packet) >= 0)
    {
        // 处理暂停
        while (is_paused_ && !should_stop_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (should_stop_) { break; }

        packet_count++;

        // 创建包装器（确保PacketWrapper正确克隆包+赋值DTS/PTS/关键帧）
        PacketWrapper wrapper(packet, packet->stream_index);

        if (wrapper.stream_index == file_info_.videoStreamIndex && wrapper.packet) {
            bool is_key = (wrapper.packet->flags & AV_PKT_FLAG_KEY) != 0;
            wrapper.is_key_frame = is_key; // 强制赋值
        }

        // ========== 核心修改：区分音视频包 ==========
        if (wrapper.stream_index == file_info_.videoStreamIndex) {
            // 视频包：先入DTS临时排序队列
            pushVideoPacketToTempQueue(std::move(wrapper));
            video_packet_count++;

            // 每攒10个视频包，批量刷入ring_buffer_（避免临时队列过大）
            if (video_packet_count % 10 == 0) {
                flushTempQueueToRingBuffer();
            }
        } else if (wrapper.stream_index == file_info_.audioStreamIndex) {
            // 音频包：直接入环形缓冲区
            if (!ring_buffer_->emplace(5, std::move(wrapper))) {
                dropped_count++;
                if (dropped_count % 50 == 0) {
                    qDebug() << "[readFileThread] 警告：环形缓冲区满，已丢弃" << dropped_count << "个音频包";
                }
            }
            audio_packet_count++;
        } else {
            // 其他流（字幕等）：直接丢弃
            continue;
        }

    }

    // ========== 关键补充：文件读取完成后，刷空临时队列 ==========
    flushTempQueueToRingBuffer();

    av_packet_free(&packet);

    // 标记文件结束
    is_eos_ = true;
    qDebug() << "[readFileThread] 文件读取完成";
    qDebug() << "  总包数:" << packet_count
             << "  视频包数:" << video_packet_count
             << "  音频包数:" << audio_packet_count
             << "  丢弃包数:" << dropped_count
             << "  缓冲区剩余:" << ring_buffer_->size();

    // 等待处理线程处理完剩余数据
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    int retry = 0;
    while (ring_buffer_->size() > 0 && retry < 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        retry++;
    }

    // 发送结束信号到队列
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        audio_queue_->push(AVData::createEOS());
        video_queue_->push(AVData::createEOS());
    }
    emit dataFinished();

    QTime cuT2 = QTime::currentTime();
    quint64 diff = cuT1.msecsTo(cuT2);
    qDebug() << "[readFileThread] 耗时" << diff / 1000 << "秒" << diff % 1000 << "毫秒";
}

std::vector<std::shared_ptr<AVData>> LocalFileDataFetcher::decodePacket(AVPacket* packet)
{
    std::vector<std::shared_ptr<AVData>> result;
    // 后续补解码逻辑即可
    return result;
}

void LocalFileDataFetcher::cleanup()
{
    if (video_codec_ctx_) {
        avcodec_close(video_codec_ctx_);
        avcodec_free_context(&video_codec_ctx_);
    }
    if (audio_codec_ctx_) {
        avcodec_close(audio_codec_ctx_);
        avcodec_free_context(&audio_codec_ctx_);
    }
    if (format_ctx_) {
        avformat_close_input(&format_ctx_);
        avformat_free_context(format_ctx_);
    }

}

void LocalFileDataFetcher::processBufferThread()
{
    qDebug() << "[processBufferThread] 开始：从环形缓冲区处理数据（已DTS排序）";
    int processed_count = 0;
    int audio_batch = 0, video_batch = 0;

    while (!should_stop_ && (!is_eos_ || ring_buffer_->size() > 0))
    {
        PacketWrapper wrapper;
        if (ring_buffer_->pop(wrapper, 10)) {
            processed_count++;

            // 时间基转换：仅保留基础转换，删掉校准（避免内存错误）
            int64_t dts_us = AV_NOPTS_VALUE;
            int64_t pts_us = AV_NOPTS_VALUE;
            AVRational* time_base = nullptr;
            if (wrapper.stream_index == file_info_.videoStreamIndex) {
                time_base = &video_time_base_;
            } else if (wrapper.stream_index == file_info_.audioStreamIndex) {
                time_base = &audio_time_base_;
            }
            if (time_base && wrapper.dts != AV_NOPTS_VALUE) {
                dts_us = av_rescale_q_rnd(wrapper.dts, *time_base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF);
            }
            if (time_base && wrapper.pts != AV_NOPTS_VALUE) {
                pts_us = av_rescale_q_rnd(wrapper.pts, *time_base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF);
            }

            // ========== 核心修复1：AVPacket生命周期（避免重复释放） ==========
            AVPacket* pkt = av_packet_clone(wrapper.packet); // 克隆packet，避免原指针被释放
            if (!pkt) {
                qWarning() << "[ProcessBuffer] 克隆AVPacket失败";
                continue;
            }

            // 封装AVData
            auto avdata = std::make_shared<AVData>(AVDataType::PACKET);
            avdata->setPacket(pkt); // 用克隆后的packet
            avdata->setStreamIndex(wrapper.stream_index);
            avdata->setPts(pts_us);
            avdata->setDts(dts_us);

            // 核心修复2：仅视频包设置关键帧，音频包强制false
            avdata->setKeyFrame(wrapper.stream_index == file_info_.videoStreamIndex ? wrapper.is_key_frame : false);

            // 音视频分流入队+修复3：打印错位
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (wrapper.stream_index == file_info_.audioStreamIndex) {
                audio_queue_->push(avdata);
                audio_batch++;
                if (audio_batch % 10 == 0) {
                    audio_batch = 0;
                }
            } else if (wrapper.stream_index == file_info_.videoStreamIndex) {
                video_queue_->push(avdata);
                video_batch++;
                if (video_batch % 10 == 0) {
                    video_batch = 0;
                }
            }

            // ========== 核心修复：释放原wrapper的packet，避免内存泄漏 ==========
            av_packet_free(&wrapper.packet);
            wrapper.packet = nullptr;
        }
    }

    if (processed_count - audio_batch - video_batch > 0) emit dataAvailable();

    qDebug() << "[processBufferThread] 结束，共处理" << processed_count << "个数据包";
}

// LocalFileDataFetcher.cpp
AVCodecParameters* LocalFileDataFetcher::getVideoCodecParams() const {
    if (format_ctx_ && file_info_.videoStreamIndex >= 0) {
        return format_ctx_->streams[file_info_.videoStreamIndex]->codecpar;
    }
    return nullptr;
}

AVCodecParameters* LocalFileDataFetcher::getAudioCodecParams() const {
    if (format_ctx_ && file_info_.audioStreamIndex >= 0) {
        return format_ctx_->streams[file_info_.audioStreamIndex]->codecpar;
    }
    return nullptr;
}

int LocalFileDataFetcher::getVideoStreamIndex() const {
    return file_info_.videoStreamIndex;
}

int LocalFileDataFetcher::getAudioStreamIndex() const {
    return file_info_.audioStreamIndex;
}

const uint8_t* LocalFileDataFetcher::getVideoExtradata() const {
    if (!format_ctx_ || file_info_.videoStreamIndex < 0) {
        return nullptr;
    }
    AVCodecParameters* codecpar = format_ctx_->streams[file_info_.videoStreamIndex]->codecpar;
    return codecpar->extradata;
}

int LocalFileDataFetcher::getVideoExtradataSize() const {
    if (!format_ctx_ || file_info_.videoStreamIndex < 0) {
        return 0;
    }
    AVCodecParameters* codecpar = format_ctx_->streams[file_info_.videoStreamIndex]->codecpar;
    return codecpar->extradata_size;
}

const uint8_t* LocalFileDataFetcher::getAudioExtradata() const
{
    if (!format_ctx_ || file_info_.audioStreamIndex < 0) { // 改为audioStreamIndex
        return nullptr;
    }
    AVCodecParameters* codecpar = format_ctx_->streams[file_info_.audioStreamIndex]->codecpar;
    return codecpar->extradata;
}

int LocalFileDataFetcher::getAudioExtradataSize() const {
    if (!format_ctx_ || file_info_.audioStreamIndex < 0) { // 改为audioStreamIndex
        return 0;
    }
    AVCodecParameters* codecpar = format_ctx_->streams[file_info_.audioStreamIndex]->codecpar;
    return codecpar->extradata_size;
}