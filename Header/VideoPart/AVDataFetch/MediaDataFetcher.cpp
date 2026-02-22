//
// Created by lsy on 2026/1/17.
//

#include "MediaDataFetcher.h"
#include <QDebug>
#include <QTime>

MediaDataFetcher::MediaDataFetcher(QObject *parent)
    : AVDataFetcher(parent)
{
    avformat_network_init();

    audio_queue_ = std::make_shared<AVDataQueue>();
    video_queue_ = std::make_shared<AVDataQueue>();

    fmt_ctx_ = nullptr;
    video_codec_ctx_ = nullptr;
    audio_codec_ctx_ = nullptr;
    video_stream_idx_ = -1;
    audio_stream_idx_ = -1;
    video_time_base_ = {0, 0};
    audio_time_base_ = {0, 0};
    video_codecpar_ = nullptr;
    audio_codecpar_ = nullptr;

    should_stop_ = true;
    is_paused_ = false;
    is_eos_ = false;

    file_info_.videoStreamIndex = -1;
    file_info_.audioStreamIndex = -1;
    file_info_.videoWidth = 0;
    file_info_.videoHeight = 0;
    file_info_.fps = 0.0;
    file_info_.durationMs = 0;
    file_info_.bitrate = 0;

    // 音视频专用环形缓冲区，容量 400
    audio_ring_buffer_ = std::make_unique<RingBuffer<PacketWrapper>>(RING_BUFFER_CAPACITY);
    video_ring_buffer_ = std::make_unique<RingBuffer<PacketWrapper>>(RING_BUFFER_CAPACITY);

    qDebug() << "[MediaDataFetcher] 构造函数完成，音频/视频缓冲区容量:" << RING_BUFFER_CAPACITY;
}

MediaDataFetcher::~MediaDataFetcher()
{
    cleanup();
}

// ------------------------------------------------------------
// doInit() : 打开网络流，获取流信息，打开解码器
// ------------------------------------------------------------
bool MediaDataFetcher::doInit()
{
    qDebug() << "[MediaDataFetcher::doInit] === 开始 ===";
    qDebug() << "  source_info_.isValid =" << source_info_.isValid;
    qDebug() << "  source_info_.mode =" << static_cast<int>(source_info_.mode);
    qDebug() << "  source_info_.url =" << source_info_.url;

    if (!source_info_.isValid || source_info_.url.isEmpty()) {
        qDebug() << "  错误：SourceInfo 无效或 URL 为空";
        return false;
    }

    url_ = source_info_.url;
    qDebug() << "  目标 URL:" << url_;

    fmt_ctx_ = avformat_alloc_context();
    if (!fmt_ctx_) {
        qDebug() << "  错误：无法分配 AVFormatContext";
        return false;
    }

    // 网络选项：user_agent + 1MB 缓冲 + HTTP Keep-Alive（关键！）
    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "user_agent", "RinzePlayer/1.0 (MediaFetcher)", 0);
    av_dict_set(&opts, "buffer_size", "1024000", 0);
    av_dict_set(&opts, "multiple_requests", "1", 0);   // 复用连接，大幅降低开销

    int ret = avformat_open_input(&fmt_ctx_, url_.toUtf8().constData(), nullptr, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        char err[256];
        av_strerror(ret, err, sizeof(err));
        qDebug() << "  错误：无法打开网络流，错误码:" << ret << ", 信息:" << err;
        avformat_free_context(fmt_ctx_);
        fmt_ctx_ = nullptr;
        return false;
    }

    ret = avformat_find_stream_info(fmt_ctx_, nullptr);
    if (ret < 0) {
        qDebug() << "  错误：无法获取流信息";
        avformat_close_input(&fmt_ctx_);
        fmt_ctx_ = nullptr;
        return false;
    }

    // 遍历流，打开解码器，记录信息
    file_info_.videoStreamIndex = -1;
    file_info_.audioStreamIndex = -1;

    for (unsigned int i = 0; i < fmt_ctx_->nb_streams; ++i) {
        AVStream *stream = fmt_ctx_->streams[i];
        AVCodecParameters *par = stream->codecpar;
        const AVCodec *codec = avcodec_find_decoder(par->codec_id);
        if (!codec) continue;

        if (par->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx_ < 0) {
            video_codec_ctx_ = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(video_codec_ctx_, par);
            if (avcodec_open2(video_codec_ctx_, codec, nullptr) >= 0) {
                video_stream_idx_ = i;
                video_time_base_ = stream->time_base;
                video_codecpar_ = par;

                file_info_.videoStreamIndex = i;
                file_info_.videoWidth = par->width;
                file_info_.videoHeight = par->height;
                file_info_.videoCodec = QString(codec->name);
                file_info_.fps = av_q2d(stream->avg_frame_rate);

                qDebug() << "    视频流: 索引=" << i
                         << ", 分辨率=" << par->width << "x" << par->height
                         << ", FPS=" << file_info_.fps
                         << ", 时间基=" << video_time_base_.num << "/" << video_time_base_.den;
            } else {
                avcodec_free_context(&video_codec_ctx_);
            }
        }
        else if (par->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_idx_ < 0) {
            audio_codec_ctx_ = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(audio_codec_ctx_, par);
            if (avcodec_open2(audio_codec_ctx_, codec, nullptr) >= 0) {
                audio_stream_idx_ = i;
                audio_time_base_ = stream->time_base;
                audio_codecpar_ = par;

                file_info_.audioStreamIndex = i;
                file_info_.audioCodec = QString(codec->name);

                qDebug() << "    音频流: 索引=" << i
                         << ", 编码器=" << codec->name
                         << ", 时间基=" << audio_time_base_.num << "/" << audio_time_base_.den;
            } else {
                avcodec_free_context(&audio_codec_ctx_);
            }
        }
    }

    if (video_stream_idx_ < 0 && audio_stream_idx_ < 0) {
        qDebug() << "  错误：没有找到任何可播放的音视频流";
        cleanup();
        return false;
    }

    if (fmt_ctx_->duration != AV_NOPTS_VALUE) {
        file_info_.durationMs = fmt_ctx_->duration / (AV_TIME_BASE / 1000);
    }
    if (fmt_ctx_->bit_rate > 0) {
        file_info_.bitrate = fmt_ctx_->bit_rate / 1000;
    }

    qDebug() << "  媒体信息: 时长=" << file_info_.durationMs << "ms, 码率=" << file_info_.bitrate << "kbps";
    qDebug() << "[MediaDataFetcher::doInit] === 初始化成功 ===";
    return true;
}

// ------------------------------------------------------------
// doStart() : 启动读取线程和处理线程
// ------------------------------------------------------------
bool MediaDataFetcher::doStart()
{
    qDebug() << "[MediaDataFetcher::doStart] === 开始 ===";

    if (!fmt_ctx_) {
        qDebug() << "  错误：fmt_ctx_ 为空，请先调用 init()";
        return false;
    }
    if (read_thread_.joinable() || process_thread_.joinable()) {
        qDebug() << "  警告：线程已在运行";
        return false;
    }

    // 重置状态
    should_stop_ = false;
    is_paused_ = false;
    is_eos_ = false;
    video_ring_buffer_->clear();
    audio_ring_buffer_->clear();
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        audio_queue_->clear();
        video_queue_->clear();
    }

    // 启动双线程
    read_thread_ = std::thread([this]() { readNetworkThread(); });
    process_thread_ = std::thread([this]() { processBufferThread(); });

    qDebug() << "[MediaDataFetcher::doStart] === 启动成功 ===";
    return true;
}

// ------------------------------------------------------------
// readNetworkThread() : 生产者，从网络流读取 AVPacket，压入双缓冲区
// ------------------------------------------------------------
void MediaDataFetcher::readNetworkThread()
{
    qDebug() << "[readNetworkThread] 开始：从网络流读取数据（双缓冲区）";
    QTime cuT1 = QTime::currentTime();

    AVPacket *packet = av_packet_alloc();
    int packet_count = 0;
    int dropped_count = 0;
    int video_packet_count = 0;
    int audio_packet_count = 0;

    while (!should_stop_) {
        while (is_paused_ && !should_stop_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (should_stop_) break;

        int ret = av_read_frame(fmt_ctx_, packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                qDebug() << "[readNetworkThread] 网络流结束 (EOF)";
                is_eos_ = true;
                break;
            } else {
                char err[256];
                av_strerror(ret, err, sizeof(err));
                qDebug() << "[readNetworkThread] 读取错误，停止:" << err;
                is_eos_ = true;
                break;
            }
        }

        packet_count++;

        // 创建包装器（深拷贝）
        PacketWrapper wrapper(packet, packet->stream_index);

        if (wrapper.stream_index == video_stream_idx_ && wrapper.packet) {
            wrapper.is_key_frame = (wrapper.packet->flags & AV_PKT_FLAG_KEY) != 0;
        }

        // ========== 双缓冲区分流 ==========
        if (wrapper.stream_index == video_stream_idx_) {
            pushVideoPacketToTempQueue(std::move(wrapper));
            video_packet_count++;
            // 优化：每 5 个视频包刷一次（原为10），减少临时队列积压
            if (video_packet_count % 5 == 0) {
                flushTempQueueToRingBuffer();
            }
        } else if (wrapper.stream_index == audio_stream_idx_) {
            // 音频包直接入 audio_ring_buffer_，满则自旋等待（绝不丢弃）
            while (!audio_ring_buffer_->push(std::move(wrapper)) && !should_stop_) {
                std::this_thread::sleep_for(std::chrono::microseconds(50)); // 原200，减到50
            }
            audio_packet_count++;
        } else {
            av_packet_unref(packet);
            continue;
        }

        av_packet_unref(packet);
    }

    // 读取结束，刷空视频临时队列
    flushTempQueueToRingBuffer();
    av_packet_free(&packet);

    qDebug() << "[readNetworkThread] 读取结束";
    qDebug() << "  总包数:" << packet_count
             << "  视频包:" << video_packet_count
             << "  音频包:" << audio_packet_count
             << "  音频缓冲区剩余:" << audio_ring_buffer_->size()
             << "  视频缓冲区剩余:" << video_ring_buffer_->size();

    // 等待处理线程消化剩余数据（最长2秒）—— 修复条件判断
    int retry = 0;
    while ((audio_ring_buffer_->size() > 0 || video_ring_buffer_->size() > 0) && retry < 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        retry++;
    }

    // 发送结束信号
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        audio_queue_->push(AVData::createEOS());
        video_queue_->push(AVData::createEOS());
    }
    emit dataFinished();

    QTime cuT2 = QTime::currentTime();
    quint64 diff = cuT1.msecsTo(cuT2);
    qDebug() << "[readNetworkThread] 耗时" << diff / 1000 << "秒" << diff % 1000 << "毫秒";
}

// ------------------------------------------------------------
// processBufferThread() : 消费者，音频批量优先 + 非阻塞轮询
// ------------------------------------------------------------
void MediaDataFetcher::processBufferThread()
{
    qDebug() << "[processBufferThread] 开始：音频批量优先模式（非阻塞）";
    int processed_count = 0;
    int audio_batch = 0, video_batch = 0;

    const int POP_TIMEOUT_MS = 0;  // 非阻塞，立即返回

    while (!should_stop_ && (!is_eos_ ||
                             audio_ring_buffer_->size() > 0 ||
                             video_ring_buffer_->size() > 0))
    {
        // ===== 1. 疯狂处理音频包，直到音频缓冲区清空 =====
        bool audio_processed = false;
        while (!should_stop_) {
            PacketWrapper wrapper;
            if (!audio_ring_buffer_->pop(wrapper, POP_TIMEOUT_MS))
                break;  // 音频缓冲区空了

            audio_processed = true;

            // ---------- 时间基转换（微秒）----------
            int64_t dts_us = AV_NOPTS_VALUE;
            int64_t pts_us = AV_NOPTS_VALUE;
            if (audio_time_base_.den > 0 && wrapper.dts != AV_NOPTS_VALUE) {
                dts_us = av_rescale_q_rnd(wrapper.dts, audio_time_base_, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF);
            }
            if (audio_time_base_.den > 0 && wrapper.pts != AV_NOPTS_VALUE) {
                pts_us = av_rescale_q_rnd(wrapper.pts, audio_time_base_, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF);
            }

            // 克隆 AVPacket（深拷贝）
            AVPacket* pkt = av_packet_clone(wrapper.packet);
            if (!pkt) {
                qWarning() << "[processBufferThread] av_packet_clone 失败（音频）";
                av_packet_free(&wrapper.packet);
                wrapper.packet = nullptr;
                continue;
            }

            // 构造 AVData
            auto avdata = std::make_shared<AVData>(AVDataType::PACKET);
            avdata->setPacket(pkt);
            avdata->setStreamIndex(wrapper.stream_index);
            avdata->setPts(pts_us);
            avdata->setDts(dts_us);
            avdata->setKeyFrame(false);  // 音频无关键帧概念

            // 入音频队列
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                audio_queue_->push(avdata);
            }

            audio_batch++;
            processed_count++;

            // 诊断：每处理 10 个音频包打印一次队列状态
            if (audio_batch % 10 == 0) {
                qDebug() << "[process] 音频队列大小:" << audio_queue_->size()
                         << "音频缓冲区剩余:" << audio_ring_buffer_->size();
            }

            // 释放 wrapper 中的 packet
            av_packet_free(&wrapper.packet);
            wrapper.packet = nullptr;
        }

        // ===== 2. 音频清空后，最多处理一个视频包（插空）=====
        if (!should_stop_) {
            PacketWrapper wrapper;
            if (video_ring_buffer_->pop(wrapper, POP_TIMEOUT_MS)) {
                // ---------- 时间基转换 ----------
                int64_t dts_us = AV_NOPTS_VALUE;
                int64_t pts_us = AV_NOPTS_VALUE;
                if (video_time_base_.den > 0 && wrapper.dts != AV_NOPTS_VALUE) {
                    dts_us = av_rescale_q_rnd(wrapper.dts, video_time_base_, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF);
                }
                if (video_time_base_.den > 0 && wrapper.pts != AV_NOPTS_VALUE) {
                    pts_us = av_rescale_q_rnd(wrapper.pts, video_time_base_, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF);
                }

                AVPacket* pkt = av_packet_clone(wrapper.packet);
                if (pkt) {
                    auto avdata = std::make_shared<AVData>(AVDataType::PACKET);
                    avdata->setPacket(pkt);
                    avdata->setStreamIndex(wrapper.stream_index);
                    avdata->setPts(pts_us);
                    avdata->setDts(dts_us);
                    avdata->setKeyFrame(wrapper.is_key_frame);

                    {
                        std::lock_guard<std::mutex> lock(queue_mutex_);
                        video_queue_->push(avdata);
                    }

                    video_batch++;
                    processed_count++;
                } else {
                    qWarning() << "[processBufferThread] av_packet_clone 失败（视频）";
                }

                // 释放 wrapper 中的 packet
                av_packet_free(&wrapper.packet);
                wrapper.packet = nullptr;
            }
        }

        // ===== 3. 双缓冲都空时，短暂让出 CPU =====
        if (!audio_processed && video_ring_buffer_->size() == 0) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }

    if (processed_count > 0) {
        emit dataAvailable();
    }

    qDebug() << "[processBufferThread] 结束，共处理" << processed_count << "个数据包"
             << "(音频:" << audio_batch << ", 视频:" << video_batch << ")";
}

// ------------------------------------------------------------
// 辅助函数：视频包推入临时队列并排序
// ------------------------------------------------------------
void MediaDataFetcher::pushVideoPacketToTempQueue(PacketWrapper&& wrapper)
{
    std::lock_guard<std::mutex> lock(video_temp_mutex_);
    if (wrapper.packet) {
        wrapper.is_key_frame = (wrapper.packet->flags & AV_PKT_FLAG_KEY) != 0;
    }
    video_temp_queue_.push_back(std::move(wrapper));
    std::sort(video_temp_queue_.begin(), video_temp_queue_.end(), VideoPacketCompare());
}

// ------------------------------------------------------------
// 将排序后的视频包批量刷入 video_ring_buffer_（自旋等待直到成功）
// ------------------------------------------------------------
void MediaDataFetcher::flushTempQueueToRingBuffer()
{
    std::lock_guard<std::mutex> lock(video_temp_mutex_);
    if (video_temp_queue_.empty()) return;

    for (auto& wrapper : video_temp_queue_) {
        // 自旋等待时间降至 50us，减少对读取线程的占用
        while (!video_ring_buffer_->push(std::move(wrapper)) && !should_stop_) {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }
    video_temp_queue_.clear();
}

// ------------------------------------------------------------
// 资源清理
// ------------------------------------------------------------
void MediaDataFetcher::cleanup()
{
    should_stop_ = true;
    if (read_thread_.joinable()) read_thread_.join();
    if (process_thread_.joinable()) process_thread_.join();

    if (video_codec_ctx_) {
        avcodec_free_context(&video_codec_ctx_);
    }
    if (audio_codec_ctx_) {
        avcodec_free_context(&audio_codec_ctx_);
    }
    if (fmt_ctx_) {
        avformat_close_input(&fmt_ctx_);
    }

    // 清空双缓冲区
    if (audio_ring_buffer_) audio_ring_buffer_->clear();
    if (video_ring_buffer_) video_ring_buffer_->clear();

    {
        std::lock_guard<std::mutex> lock(video_temp_mutex_);
        video_temp_queue_.clear();
    }

    video_stream_idx_ = -1;
    audio_stream_idx_ = -1;
    video_codecpar_ = nullptr;
    audio_codecpar_ = nullptr;
    video_time_base_ = {0, 0};
    audio_time_base_ = {0, 0};

    file_info_ = FileInfo();
    is_eos_ = false;
    is_paused_ = false;
}

// ------------------------------------------------------------
// 编解码参数获取
// ------------------------------------------------------------
AVCodecParameters* MediaDataFetcher::getVideoCodecParams() const
{
    if (fmt_ctx_ && video_stream_idx_ >= 0) {
        return fmt_ctx_->streams[video_stream_idx_]->codecpar;
    }
    return nullptr;
}

AVCodecParameters* MediaDataFetcher::getAudioCodecParams() const
{
    if (fmt_ctx_ && audio_stream_idx_ >= 0) {
        return fmt_ctx_->streams[audio_stream_idx_]->codecpar;
    }
    return nullptr;
}

int MediaDataFetcher::getVideoStreamIndex() const
{
    return video_stream_idx_;
}

int MediaDataFetcher::getAudioStreamIndex() const
{
    return audio_stream_idx_;
}

const uint8_t* MediaDataFetcher::getVideoExtradata() const
{
    auto par = getVideoCodecParams();
    return par ? par->extradata : nullptr;
}

int MediaDataFetcher::getVideoExtradataSize() const
{
    auto par = getVideoCodecParams();
    return par ? par->extradata_size : 0;
}

const uint8_t* MediaDataFetcher::getAudioExtradata() const
{
    auto par = getAudioCodecParams();
    return par ? par->extradata : nullptr;
}

int MediaDataFetcher::getAudioExtradataSize() const
{
    auto par = getAudioCodecParams();
    return par ? par->extradata_size : 0;
}