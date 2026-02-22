#include "AVDecoder.h"
#include <QDebug>
#include <chrono>
#include <stdexcept>
#include <cmath>

static QString ffmpegErrorToString(int errNum) {
    char errBuf[1024] = {0};
    av_strerror(errNum, errBuf, sizeof(errBuf));
    return QString::fromUtf8(errBuf);
}

static double convertPtsToMs(int64_t pts, AVRational time_base) {
    if (pts == AV_NOPTS_VALUE) return -1.0;
    return av_rescale_q(pts, time_base, av_make_q(1, 1000));
}

static double convertAudioPtsToMs(int64_t pts, AVRational audio_time_base) {
    if (pts == AV_NOPTS_VALUE) return -1.0;
    return av_rescale_q(pts, audio_time_base, av_make_q(1, 1000));
}

AVDecoder::AVDecoder(QObject* parent)
    : QObject(parent)
{
    qDebug() << "AVDecoder: Created";
    av_log_set_level(AV_LOG_WARNING);
}

AVDecoder::~AVDecoder()
{
    stop();
    cleanup();
    qDebug() << "AVDecoder: Destroyed";
}

bool AVDecoder::initVideoDecoder(AVCodecParameters* params, const uint8_t* extradata, int extradata_size) {
    if (!params) {
        qWarning() << "[解码器] 流参数params为空";
        return false;
    }

    const AVCodec* codec = avcodec_find_decoder(params->codec_id);
    if (!codec) {
        qWarning() << "[解码器] 找不到H264解码器";
        return false;
    }

    video_codec_ctx_ = avcodec_alloc_context3(codec);
    if (!video_codec_ctx_) {
        qWarning() << "[解码器] 分配上下文失败";
        return false;
    }

    if (avcodec_parameters_to_context(video_codec_ctx_, params) < 0) {
        qWarning() << "[解码器] 复制流参数失败";
        avcodec_free_context(&video_codec_ctx_);
        return false;
    }

    if (params->codec_id == AV_CODEC_ID_H264) {
        const uint8_t* sps_pps_data = nullptr;
        int sps_pps_size = 0;

        if (extradata && extradata_size > 0) {
            sps_pps_data = extradata;
            sps_pps_size = extradata_size;
        } else if (params->extradata && params->extradata_size > 0) {
            sps_pps_data = params->extradata;
            sps_pps_size = params->extradata_size;
        }

        if (sps_pps_data && sps_pps_size > 0) {
            if (video_codec_ctx_->extradata) av_freep(&video_codec_ctx_->extradata);
            video_codec_ctx_->extradata = (uint8_t*)av_mallocz(sps_pps_size + AV_INPUT_BUFFER_PADDING_SIZE);
            memcpy(video_codec_ctx_->extradata, sps_pps_data, sps_pps_size);
            video_codec_ctx_->extradata_size = sps_pps_size;
        }

        video_codec_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
        video_codec_ctx_->max_b_frames = 0;
        video_codec_ctx_->refs = 1;
        video_codec_ctx_->thread_count = 1;
        video_codec_ctx_->thread_type = FF_THREAD_FRAME;
    }

    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "flags", "low_delay", 0);
    av_dict_set(&opts, "err_detect", "ignore_err", 0);

    int ret = avcodec_open2(video_codec_ctx_, codec, &opts);
    av_dict_free(&opts);

    if (ret < 0) {
        qWarning() << "[解码器] 打开失败：" << ffmpegErrorToString(ret);
        avcodec_free_context(&video_codec_ctx_);
        return false;
    }

    if (video_time_base_.num == 0 || video_time_base_.den == 0) {
        video_time_base_ = av_make_q(1, 90000);
    }

    return true;
}

bool AVDecoder::initAudioDecoder(AVCodecParameters* params, const uint8_t* extradata, int extradata_size)
{
    if (!params) {
        qCritical() << "initAudioDecoder: Invalid parameters";
        return false;
    }

    if (audio_codec_ctx_) {
        avcodec_free_context(&audio_codec_ctx_);
    }

    const AVCodec* codec = avcodec_find_decoder(params->codec_id);
    if (!codec) {
        qCritical() << "initAudioDecoder: Codec not found";
        return false;
    }

    audio_codec_ctx_ = avcodec_alloc_context3(codec);
    if (!audio_codec_ctx_) {
        qCritical() << "initAudioDecoder: Failed to allocate context";
        return false;
    }

    int ret = avcodec_parameters_to_context(audio_codec_ctx_, params);
    if (ret < 0) {
        qCritical() << "initAudioDecoder: Failed to copy parameters:" << ffmpegErrorToString(ret);
        avcodec_free_context(&audio_codec_ctx_);
        return false;
    }

    // 保存音频比特率信息
    int64_t audio_bitrate = params->bit_rate;
    bool is_high_bitrate_aac = false;

    // 检查是否为高比特率AAC-LC（可能引起卡顿的格式）
    if (params->codec_id == AV_CODEC_ID_AAC && audio_bitrate > 160000) {
        qWarning() << "initAudioDecoder: 检测到高比特率AAC-LC ("
                   << (audio_bitrate/1000) << "kbps)，启用优化模式";
        is_high_bitrate_aac = true;
    }

    if (extradata && extradata_size > 0) {
        if (audio_codec_ctx_->extradata) {
            av_freep(&audio_codec_ctx_->extradata);
        }

        audio_codec_ctx_->extradata = (uint8_t*)av_mallocz(extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
        if (!audio_codec_ctx_->extradata) {
            qCritical() << "initAudioDecoder: Failed to allocate extradata";
            avcodec_free_context(&audio_codec_ctx_);
            return false;
        }

        memcpy(audio_codec_ctx_->extradata, extradata, extradata_size);
        audio_codec_ctx_->extradata_size = extradata_size;
    }

    // 设置解码器参数
    AVDictionary* opts = nullptr;

    // 通用参数
    av_dict_set(&opts, "strict", "experimental", 0);
    av_dict_set(&opts, "err_detect", "ignore_err", 0);  // 忽略错误继续解码

    // 针对高比特率AAC-LC的优化
    if (is_high_bitrate_aac) {
        // 启用快速解码模式
        av_dict_set(&opts, "fast", "1", 0);
        av_dict_set(&opts, "aac_coder", "fast", 0);

        // 禁用耗电功能
        av_dict_set(&opts, "drc_scale", "0", 0);      // 禁用动态范围压缩
        av_dict_set(&opts, "drc_cut", "0", 0);        // 禁用DRC削减

        // 增加线程数以提高解码性能
        av_dict_set_int(&opts, "threads", 4, 0);

        // 禁用一些高级功能以提升速度
        av_dict_set(&opts, "lowres", "0", 0);         // 不使用低分辨率
        av_dict_set(&opts, "skip_frame", "none", 0);  // 不跳过帧
        av_dict_set(&opts, "skip_idct", "none", 0);   // 不跳过IDCT
        av_dict_set(&opts, "skip_loop_filter", "none", 0);  // 不跳过环路滤波

        // 记录状态，供后续调整
        qDebug() << "initAudioDecoder: 已启用高比特率AAC优化模式 (threads=4)";
    } else {
        // 正常解码模式
        av_dict_set(&opts, "fast", "0", 0);
        av_dict_set(&opts, "aac_coder", "twoloop", 0);
        // 正常模式也使用多线程
        av_dict_set_int(&opts, "threads", 2, 0);
    }

    ret = avcodec_open2(audio_codec_ctx_, codec, &opts);
    av_dict_free(&opts);

    if (ret < 0) {
        qCritical() << "initAudioDecoder: Failed to open codec:" << ffmpegErrorToString(ret);
        avcodec_free_context(&audio_codec_ctx_);
        return false;
    }

    if (audio_time_base_.num == 0 || audio_time_base_.den == 0) {
        if (audio_codec_ctx_->framerate.num > 0 && audio_codec_ctx_->framerate.den > 0) {
            audio_time_base_ = av_make_q(1, audio_codec_ctx_->sample_rate);
            qDebug() << "initAudioDecoder: Time base from framerate:"
                     << audio_time_base_.num << "/" << audio_time_base_.den;
        } else {
            audio_time_base_ = av_make_q(1, 44100);
            qDebug() << "initAudioDecoder: Time base fallback to 44100Hz:"
                     << audio_time_base_.num << "/" << audio_time_base_.den;
        }
    }

    qDebug() << "initAudioDecoder: Success, codec:" << codec->name
             << "sample_rate:" << audio_codec_ctx_->sample_rate
             << "channels:" << audio_codec_ctx_->channels
             << "format:" << av_get_sample_fmt_name((AVSampleFormat)audio_codec_ctx_->sample_fmt)
             << "bitrate:" << (audio_bitrate/1000) << "kbps"
             << (is_high_bitrate_aac ? "(high bitrate optimized)" : "");

    return true;
}

bool AVDecoder::initVideoDecoder(AVCodecParameters* params)
{
    return initVideoDecoder(params, params ? params->extradata : nullptr, params ? params->extradata_size : 0);
}

bool AVDecoder::initAudioDecoder(AVCodecParameters* params)
{
    return initAudioDecoder(params, params ? params->extradata : nullptr, params ? params->extradata_size : 0);
}

void AVDecoder::start()
{
    if (state_ != State::Stopped) {
        qWarning() << "AVDecoder: Already started";
        return;
    }

    stop_requested_ = false;
    pause_fetch_ = false;
    state_ = State::Decoding;

    if (video_fetcher_ && audio_fetcher_) {
        fetch_thread_ = std::thread(&AVDecoder::fetchThread, this);
        qDebug() << "Fetch thread started";
    }

    if (video_codec_ctx_) {
        video_decode_thread_ = std::thread(&AVDecoder::decodeVideoThread, this);
        qDebug() << "Video decode thread started";
    }

    if (audio_codec_ctx_) {
        audio_decode_thread_ = std::thread(&AVDecoder::decodeAudioThread, this);
    }

    emit stateChanged(state_);
    qDebug() << "AVDecoder: Started";
}

void AVDecoder::pause()
{
    if (state_ != State::Decoding) {
        return;
    }

    state_ = State::Paused;
    pause_fetch_ = true;

    {
        QMutexLocker locker(&mutex_);
        fetch_cond_.wakeAll();
        output_video_cond_.wakeAll();
        output_audio_cond_.wakeAll();
    }

    emit stateChanged(state_);
    qDebug() << "AVDecoder: Paused";
}

void AVDecoder::resume()
{
    if (state_ != State::Paused) {
        return;
    }

    state_ = State::Decoding;
    pause_fetch_ = false;

    {
        QMutexLocker locker(&mutex_);
        fetch_cond_.wakeAll();
        output_video_cond_.wakeAll();
        output_audio_cond_.wakeAll();
    }

    emit stateChanged(state_);
    qDebug() << "AVDecoder: Resumed";
}

void AVDecoder::stop()
{
    if (state_ == State::Stopped) {
        return;
    }

    stop_requested_ = true;
    state_ = State::Stopped;

    {
        QMutexLocker locker(&mutex_);
        video_cond_.wakeAll();
        audio_cond_.wakeAll();
        fetch_cond_.wakeAll();
        output_video_cond_.wakeAll();
        output_audio_cond_.wakeAll();
    }

    if (fetch_thread_.joinable()) {
        fetch_thread_.join();
    }
    if (video_decode_thread_.joinable()) {
        video_decode_thread_.join();
    }
    if (audio_decode_thread_.joinable()) {
        audio_decode_thread_.join();
    }

    video_packet_queue_.clear();
    audio_packet_queue_.clear();
    video_frame_queue_.clear();
    audio_frame_queue_.clear();

    video_packets_fetched_ = 0;
    audio_packets_fetched_ = 0;
    video_frames_decoded_ = 0;
    audio_frames_decoded_ = 0;
    video_frames_dropped_ = 0;
    audio_frames_dropped_ = 0;
    video_frames_output_ = 0;
    audio_frames_output_ = 0;
    video_callback_count_ = 0;
    audio_callback_count_ = 0;

    emit stateChanged(state_);
    qDebug() << "AVDecoder: Stopped";
}

void AVDecoder::flush() {
    std::lock_guard<std::mutex> lock(m_codecMutex);
    m_isDecodePaused = true;

    try {
        if (video_codec_ctx_) {
            avcodec_flush_buffers(video_codec_ctx_);
            qDebug() << "[AVDecoder] 安全刷新视频解码器";
        }
        if (audio_codec_ctx_) {
            avcodec_flush_buffers(audio_codec_ctx_);
            qDebug() << "[AVDecoder] 安全刷新音频解码器";
        }

        video_packet_queue_.clear();
        audio_packet_queue_.clear();
        video_frame_queue_.clear();
        audio_frame_queue_.clear();

        qDebug() << "AVDecoder: Flushed（线程安全版）";
    } catch (...) {
        qCritical() << "[AVDecoder] flush 过程发生异常，避免崩溃";
    }

    m_isDecodePaused = false;
}

void AVDecoder::seek(int64_t timestamp_ms)
{
    if (state_ == State::Stopped) {
        return;
    }

    qDebug() << "AVDecoder: Seeking to" << timestamp_ms << "ms";

    pause();
    flush();
    resume();
}

void AVDecoder::fetchThread()
{
    QTime startTime = QTime::currentTime();
    qDebug() << "Fetch thread running...";

    bool video_eos_received = false;
    bool audio_eos_received = false;

    int video_queue_warning_threshold = 300;
    int audio_queue_warning_threshold = 500;

    int fetch_interval_ms = 5;

    while (!stop_requested_) {
        if (pause_fetch_ || state_ != State::Decoding) {
            QMutexLocker locker(&mutex_);
            fetch_cond_.wait(&mutex_, 100);
            continue;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(fetch_interval_ms));

        float video_queue_ratio = (float)video_packet_queue_.size() / video_queue_warning_threshold;
        float audio_queue_ratio = (float)audio_packet_queue_.size() / audio_queue_warning_threshold;
        float max_queue_ratio = qMax(video_queue_ratio, audio_queue_ratio);

        if (max_queue_ratio >= 0.8) {
            fetch_interval_ms = 20;
        } else if (max_queue_ratio >= 0.7) {
            fetch_interval_ms = 15;
        } else if (max_queue_ratio >= 0.6) {
            fetch_interval_ms = 10;
        } else {
            fetch_interval_ms = 5;
        }

        if (audio_fetcher_ && !audio_eos_received && audio_codec_ctx_) {
            if (audio_packet_queue_.size() < audio_queue_warning_threshold) {
                std::shared_ptr<AVData> audio_packet = audio_fetcher_();
                if (audio_packet) {
                    if (audio_packet->isEOS()) {
                        qDebug() << "Fetch: Audio EOS received";
                        audio_packet_queue_.push(audio_packet, 10);
                        audio_eos_received = true;
                    } else if (audio_packet->isPacket()) {
                        audio_packets_fetched_++;
                        bool success = audio_packet_queue_.push(audio_packet, 50);
                        if (!success) {
                            qWarning() << "Fetch: Audio queue full, dropped packet";
                        }
                    }
                }
            } else {
                qDebug() << "Fetch: Audio queue full, skipping fetch";
            }
        }

        if (video_fetcher_ && !video_eos_received && video_codec_ctx_) {
            if (video_packet_queue_.size() < video_queue_warning_threshold) {
                std::shared_ptr<AVData> video_packet = video_fetcher_();
                if (video_packet) {
                    if (video_packet->isEOS()) {
                        qDebug() << "Fetch: Video EOS received";
                        video_packet_queue_.push(video_packet, 20);
                        video_eos_received = true;
                    } else if (video_packet->isPacket()) {
                        video_packets_fetched_++;
                        bool success = video_packet_queue_.push(video_packet, 200);
                        if (!success) {
                            qWarning() << "Fetch: Video queue full, dropped packet";
                        }
                    }
                }
            } else {
                qDebug() << "Fetch: Video queue full, skipping fetch";
            }
        }

        bool video_done = !video_codec_ctx_ || video_eos_received;
        bool audio_done = !audio_codec_ctx_ || audio_eos_received;

        if (video_done && audio_done) {
            qDebug() << "Fetch: All streams ended, stopping fetch thread";
            qDebug() << "Fetch statistics - Video packets:" << video_packets_fetched_
                     << ", Audio packets:" << audio_packets_fetched_;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(fetch_interval_ms));
    }

    QTime endTime = QTime::currentTime();
    int diffMs = startTime.msecsTo(endTime);
    qDebug() << "Fetch thread stopped " << qAbs(diffMs) << "ms used";
}

void AVDecoder::decodeVideoThread()
{
    qDebug() << "Video decode thread running...";

    bool eos_received = false;
    int consecutive_failures = 0;
    const int max_consecutive_failures = 10;

    while (!stop_requested_) {
        // 暂停处理
        if (state_ == State::Paused) {
            QMutexLocker locker(&video_mutex_);
            video_cond_.wait(&video_mutex_, 50);
            continue;
        }

        std::shared_ptr<AVData> packet;
        if (!video_packet_queue_.pop(packet, 200)) {
            if (stop_requested_) break;
            continue;
        }

        // 处理EOS
        if (packet->isEOS()) {
            qDebug() << "Video decode: Received EOS";
            eos_received = true;

            auto send_ret = avcodec_send_packet(video_codec_ctx_, nullptr);
            if (send_ret < 0 && send_ret != AVERROR_EOF) {
                qWarning() << "Failed to send EOS to video decoder:" << ffmpegErrorToString(send_ret);
            }

            // 读取所有剩余帧
            while (true) {
                AVFrame* frame = av_frame_alloc();
                int ret = avcodec_receive_frame(video_codec_ctx_, frame);

                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                    av_frame_free(&frame);
                    break;
                } else if (ret < 0) {
                    qWarning() << "Failed to receive video frame (EOS):" << ffmpegErrorToString(ret);
                    av_frame_free(&frame);
                    break;
                }

                processDecodedVideoFrame(frame);
                // av_frame_free(&frame);
            }

            // 推送EOS到输出队列
            video_frame_queue_.push(AVData::createEOS(), 100);
            break;
        }

        // 解码视频包
        if (decodeVideoPacket(packet)) {
            consecutive_failures = 0;
        } else {
            consecutive_failures++;
            if (consecutive_failures > max_consecutive_failures) {
                qWarning() << "Too many consecutive decode failures, stopping video decode thread";
                break;
            }
        }
    }

    qDebug() << "Video decode thread stopped, decoded frames:" << video_frames_decoded_;
}

void AVDecoder::decodeAudioThread()
{
    qDebug() << "Audio decode thread running...";
    audio_decode_start_ = std::chrono::steady_clock::now();

    bool eos_received = false;
    int consecutive_failures = 0;
    const int max_consecutive_failures = 10;

    while (!stop_requested_) {
        if (state_ == State::Paused) {
            QMutexLocker locker(&audio_mutex_);
            audio_cond_.wait(&audio_mutex_, 50);
            continue;
        }

        std::shared_ptr<AVData> packet;
        if (!audio_packet_queue_.pop(packet, 50)) {
            if (stop_requested_) break;
            continue;
        }

        if (packet->isEOS()) {
            qDebug() << "Audio decode: Received EOS";
            eos_received = true;

            avcodec_send_packet(audio_codec_ctx_, nullptr);

            while (true) {
                AVFrame* frame = av_frame_alloc();
                int ret = avcodec_receive_frame(audio_codec_ctx_, frame);

                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                    av_frame_free(&frame);
                    break;
                } else if (ret < 0) {
                    av_frame_free(&frame);
                    break;
                }

                processDecodedAudioFrame(frame);
                av_frame_free(&frame);
            }

            audio_frame_queue_.push(AVData::createEOS(), 100);
            break;
        }

        if (decodeAudioPacket(packet)) {
            consecutive_failures = 0;
        } else {
            consecutive_failures++;
            if (consecutive_failures > max_consecutive_failures) {
                qWarning() << "Too many consecutive decode failures, stopping audio decode thread";
                break;
            }
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    total_audio_decode_time_ = end_time - audio_decode_start_;

    qDebug() << "Audio decode thread stopped, decoded frames:" << audio_frames_decoded_;
}

std::shared_ptr<AVData> AVDecoder::getNextVideoFrame(int timeout_ms) {
    std::shared_ptr<AVData> frame;
    bool ret = video_frame_queue_.pop(frame, -1);
    if (!ret) {
        return nullptr;
    }

    return frame;
}

std::shared_ptr<AVData> AVDecoder::getNextAudioFrame(int timeout_ms)
{
    std::shared_ptr<AVData> frame;
    bool ret = audio_frame_queue_.pop(frame, -1);
    if (!ret) {
        return nullptr;
    }


    return frame;
}

void AVDecoder::setAudioOutputSpec(const AudioSpec& spec)
{
    // 1. 复制原始规格
    audio_spec_ = spec;

    // 2. 重要：确保格式是SDL兼容的打包格式
    if (audio_spec_.is_planar) {
        // 平面格式 -> 转换为打包格式
        audio_spec_.format = static_cast<AVSampleFormat>(
            av_get_packed_sample_fmt(audio_spec_.format)
        );
        audio_spec_.is_planar = false;

        qDebug() << "[AudioSpec] 转换平面格式为打包格式:"
                 << av_get_sample_fmt_name(spec.format) << "->"
                 << av_get_sample_fmt_name(audio_spec_.format);
    }

    // 3. 获取SDL格式并验证
    SDL_AudioFormat sdl_format = audio_spec_.toSDLAudioFormat();
    qDebug() << "[AudioSpec] 目标规格:"
             << "采样率:" << audio_spec_.sample_rate
             << "声道:" << audio_spec_.channels
             << "FFmpeg格式:" << av_get_sample_fmt_name(audio_spec_.format)
             << "是平面格式?" << (audio_spec_.is_planar ? "是" : "否");

    // 4. 重新配置重采样器
    if (swr_ctx_) {
        swr_free(&swr_ctx_);
        swr_ctx_ = nullptr;
    }

    if (audio_codec_ctx_ && audio_codec_ctx_->sample_fmt != AV_SAMPLE_FMT_NONE) {
        qDebug() << "[Resampler] 配置重采样器:";
        qDebug() << "  输入: 格式=" << av_get_sample_fmt_name(audio_codec_ctx_->sample_fmt)
                 << "采样率=" << audio_codec_ctx_->sample_rate
                 << "声道=" << audio_codec_ctx_->channels;
        qDebug() << "  输出: 格式=" << av_get_sample_fmt_name(audio_spec_.format)
                 << "采样率=" << audio_spec_.sample_rate
                 << "声道=" << audio_spec_.channels;

        swr_ctx_ = swr_alloc();
        if (!swr_ctx_) {
            qCritical() << "无法分配重采样器";
            return;
        }

        av_opt_set_int(swr_ctx_, "in_channel_layout",
                      av_get_default_channel_layout(audio_codec_ctx_->channels), 0);
        av_opt_set_int(swr_ctx_, "in_sample_rate", audio_codec_ctx_->sample_rate, 0);
        av_opt_set_sample_fmt(swr_ctx_, "in_sample_fmt",
                            audio_codec_ctx_->sample_fmt, 0);

        av_opt_set_int(swr_ctx_, "out_channel_layout",
                      av_get_default_channel_layout(audio_spec_.channels), 0);
        av_opt_set_int(swr_ctx_, "out_sample_rate", audio_spec_.sample_rate, 0);
        av_opt_set_sample_fmt(swr_ctx_, "out_sample_fmt", audio_spec_.format, 0);

        int ret = swr_init(swr_ctx_);
        if (ret < 0) {
            qCritical() << "无法初始化重采样器:" << ffmpegErrorToString(ret);
            swr_free(&swr_ctx_);
            swr_ctx_ = nullptr;
        } else {
            qDebug() << "重采样器初始化成功";
        }
    }
}

bool AVDecoder::decodeVideoPacket(std::shared_ptr<AVData> packet)
{
    if (!packet || !packet->packet()) {
        return false;
    }

    AVPacket* av_packet = packet->packet();
    AVPacket* pkt = av_packet_clone(av_packet);
    if (!pkt) {
        qWarning() << "Failed to clone packet";
        return false;
    }

    int ret = avcodec_send_packet(video_codec_ctx_, pkt);
    av_packet_free(&pkt);

    if (ret < 0) {
        QString err_msg = ffmpegErrorToString(ret);
        qWarning() << "Failed to send video packet:" << err_msg;

        if (ret == AVERROR_INVALIDDATA) {
            qWarning() << "无效数据，跳过此包";
            return false;
        } else if (ret == AVERROR(EAGAIN)) {
            qDebug() << "Receive::AVERROR(EAGAIN)";
            /*
            AVPacket* retry_pkt = av_packet_clone(av_packet);
            if (retry_pkt) {
                ret = avcodec_send_packet(video_codec_ctx_, retry_pkt);
                av_packet_free(&retry_pkt);
                if (ret < 0) {
                    qWarning() << "重试发送包也失败:" << ffmpegErrorToString(ret);
                    return false;
                }
            }
            */
        } else {
            std::lock_guard<std::mutex> lock(m_codecMutex);
            avcodec_flush_buffers(video_codec_ctx_);
            return false;
        }
    }

    // 接收解码后的帧
    while (true) {
        AVFrame* frame = av_frame_alloc();
        ret = avcodec_receive_frame(video_codec_ctx_, frame);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&frame);
            break;
        } else if (ret < 0) {
            QString err_msg = ffmpegErrorToString(ret);

            if (err_msg.contains("reference picture") ||
                err_msg.contains("co located POCs") ||
                err_msg.contains("Missing reference")) {
                qDebug() << "H264解码警告（可忽略）:" << err_msg;
            } else {
                qWarning() << "Failed to receive video frame:" << err_msg;
            }

            av_frame_free(&frame);
            continue;
        }

        // 简化：直接处理帧，无同步计算
        processDecodedVideoFrame(frame);
    }

    return true;
}

bool AVDecoder::decodeAudioPacket(std::shared_ptr<AVData> packet)
{
    if (!packet || !packet->packet()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_audioCodecMutex);
    AVPacket* av_packet = packet->packet();
    int ret = avcodec_send_packet(audio_codec_ctx_, av_packet);

    if (ret < 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            qDebug() << "音频解码需更多数据（EAGAIN/EOF），继续等待";
            return true;
        }
        // 致命错误：返回false
        qWarning() << "Failed to send audio packet:" << ffmpegErrorToString(ret);
        return false;
    }

    while (ret >= 0) {
        AVFrame* frame = av_frame_alloc();
        ret = avcodec_receive_frame(audio_codec_ctx_, frame);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&frame);
            break;
        }
        if (ret < 0) { // 剩余才是致命错误
            qWarning() << "音频解码失败:" << ffmpegErrorToString(ret);
            return false;
        }

        processDecodedAudioFrame(frame);
        av_frame_free(&frame);
    }

    return true;
}

void AVDecoder::processDecodedVideoFrame(AVFrame* frame)
{
    if (!frame || !frame->data[0]) {
        return;
    }

    bool valid_frame = true;
    if (frame->width <= 0 || frame->height <= 0) {
        valid_frame = false;
    }


    auto video_data = createVideoFrame(frame);
    if (!video_data) {
        return;
    }

    video_frames_decoded_++;

    bool success = video_frame_queue_.push(video_data, 200);
    if (!success) {
        qWarning() << "Video queue full, frame dropped";
        video_frames_dropped_++;
        emit frameDropped(true, video_frames_dropped_.load());
    }

    av_frame_free(&frame);
}

void AVDecoder::processDecodedAudioFrame(AVFrame* frame) {
    if (!frame || !frame->data[0]) {
        return;
    }

    static int64_t audio_sample_count = 0;
    static bool first_frame = true;

    double frame_duration = frame->nb_samples / (double)frame->sample_rate;

    if (first_frame && frame->nb_samples != 2048 && frame->nb_samples != 1024) {
        qWarning() << "Audio: First frame has abnormal sample count:"
                   << frame->nb_samples << "(expected 2048 or 1024)";
    }
    first_frame = false;


    audio_sample_count += frame->nb_samples;

    std::shared_ptr<AVData> audio_data;

    if (frame->sample_rate != audio_spec_.sample_rate ||
        frame->format != audio_spec_.format ||
        frame->channels != audio_spec_.channels) {
        audio_data = resampleAudioFrame(frame);
    } else {
        audio_data = createAudioFrame(frame);
    }

    if (audio_data) {
        audio_frames_decoded_++;

        bool success = audio_frame_queue_.push(audio_data, 200);
        if (!success) {
            qWarning() << "Audio queue full, frame dropped";
            audio_frames_dropped_++;
            emit frameDropped(false, audio_frames_dropped_.load());
        }
    }
}

std::shared_ptr<AVData> AVDecoder::createVideoFrame(AVFrame* frame)
{
    if (!frame || !frame->data[0]) {
        return nullptr;
    }

    int64_t pts_ms = AV_NOPTS_VALUE;
    if (frame->pts != AV_NOPTS_VALUE && video_time_base_.num != 0 && video_time_base_.den != 0) {
        pts_ms = av_rescale_q(frame->pts, video_time_base_, av_make_q(1, 1000));
    }


    bool is_key_frame = (frame->key_frame == 1) || (frame->flags & AV_FRAME_FLAG_KEY);

    uint8_t* data_array[4] = {nullptr};
    int linesize[4] = {0};

    for (int i = 0; i < 3; ++i) {
        int plane_height = (i == 0) ? frame->height : frame->height / 2;
        int plane_size = frame->linesize[i] * plane_height;

        if (plane_size > 0) {
            data_array[i] = static_cast<uint8_t*>(av_malloc(plane_size));
            if (data_array[i]) {
                memcpy(data_array[i], frame->data[i], plane_size);
                linesize[i] = frame->linesize[i];
            }
        }
    }

    auto video_data = AVData::createVideo(
        data_array,
        linesize,
        frame->width,
        frame->height,
        static_cast<AVPixelFormat>(frame->format),
        pts_ms,
        is_key_frame
    );

    if (video_data) {
        video_data->setCleanupFunc([data_array]() {
            for (int i = 0; i < 4; ++i) {
                if (data_array[i]) {
                    av_free(data_array[i]);
                }
            }
        });
    }

    return video_data;
}

std::shared_ptr<AVData> AVDecoder::createAudioFrame(AVFrame* frame)
{
    if (!frame) {
        return nullptr;
    }

    int64_t pts_ms = AV_NOPTS_VALUE;
    if (frame->pts != AV_NOPTS_VALUE && audio_time_base_.num != 0 && audio_time_base_.den != 0) {
        pts_ms = av_rescale_q(frame->pts, audio_time_base_, av_make_q(1, 1000));
    }

    auto audio_data = AVData::createAudio(
        const_cast<uint8_t**>(frame->data),
        frame->nb_samples,
        frame->sample_rate,
        frame->channels,
        static_cast<AVSampleFormat>(frame->format),
        pts_ms
    );

    return audio_data;
}

std::shared_ptr<AVData> AVDecoder::resampleAudioFrame(AVFrame* frame) {
    if (!frame) return nullptr;

    AVSampleFormat input_format = static_cast<AVSampleFormat>(frame->format);
    AVSampleFormat target_format = audio_spec_.format;

    AVSampleFormat dst_format = target_format;
    if (av_sample_fmt_is_planar(dst_format)) {
        dst_format = static_cast<AVSampleFormat>(av_get_packed_sample_fmt(dst_format));
    }

    bool needs_format_conversion = false;
    if (av_get_bytes_per_sample(input_format) != av_get_bytes_per_sample(dst_format)) {
        needs_format_conversion = true;
        qDebug() << "需要位深度转换:"
                 << av_get_bytes_per_sample(input_format) << "字节 →"
                 << av_get_bytes_per_sample(dst_format) << "字节";
    }

    int64_t delay = swr_get_delay(swr_ctx_, frame->sample_rate);
    int dst_nb_samples = av_rescale_rnd(
        delay + frame->nb_samples,
        audio_spec_.sample_rate,
        frame->sample_rate,
        AV_ROUND_UP
    );

    if (dst_nb_samples <= 0) {
        qWarning() << "计算的目标样本数无效:" << dst_nb_samples;
        return nullptr;
    }

    AVFrame* resampled_frame = av_frame_alloc();
    if (!resampled_frame) {
        qWarning() << "无法分配重采样帧";
        return nullptr;
    }

    resampled_frame->sample_rate = audio_spec_.sample_rate;
    resampled_frame->channels = audio_spec_.channels;
    resampled_frame->format = dst_format;
    resampled_frame->nb_samples = dst_nb_samples;
    resampled_frame->pts = frame->pts;

    int ret = av_frame_get_buffer(resampled_frame, 0);
    if (ret < 0) {
        av_frame_free(&resampled_frame);
        qWarning() << "无法获取重采样帧缓冲区:" << ffmpegErrorToString(ret);
        return nullptr;
    }

    ret = swr_convert(
        swr_ctx_,
        resampled_frame->data,
        dst_nb_samples,
        (const uint8_t**)frame->data,
        frame->nb_samples
    );

    if (ret < 0) {
        av_frame_free(&resampled_frame);
        qWarning() << "重采样失败:" << ffmpegErrorToString(ret);
        return nullptr;
    }

    resampled_frame->nb_samples = ret;

    auto audio_data = createAudioFrame(resampled_frame);
    av_frame_free(&resampled_frame);

    return audio_data;
}

AVDecoder::Statistics AVDecoder::getStatistics() const
{
    Statistics stats;

    stats.video_packets_fetched = video_packets_fetched_.load();
    stats.audio_packets_fetched = audio_packets_fetched_.load();
    stats.video_frames_decoded = video_frames_decoded_.load();
    stats.audio_frames_decoded = audio_frames_decoded_.load();
    stats.video_frames_dropped = video_frames_dropped_.load();
    stats.audio_frames_dropped = audio_frames_dropped_.load();
    stats.video_frames_output = video_frames_output_.load();
    stats.audio_frames_output = audio_frames_output_.load();

    if (total_video_decode_time_.count() > 0) {
        stats.video_decode_fps = video_frames_decoded_.load() / total_video_decode_time_.count();
    }

    if (total_audio_decode_time_.count() > 0) {
        stats.audio_decode_fps = audio_frames_decoded_.load() / total_audio_decode_time_.count();
    }

    return stats;
}

void AVDecoder::cleanup()
{
    if (video_codec_ctx_) {
        avcodec_free_context(&video_codec_ctx_);
        video_codec_ctx_ = nullptr;
    }

    if (audio_codec_ctx_) {
        avcodec_free_context(&audio_codec_ctx_);
        audio_codec_ctx_ = nullptr;
    }

    if (swr_ctx_) {
        swr_free(&swr_ctx_);
        swr_ctx_ = nullptr;
    }

    qDebug() << "AVDecoder: Cleanup completed";
}