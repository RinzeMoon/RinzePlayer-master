#include "../Header/AudioPart/Decode/AudioDecoder.h"
#include <QDebug>
#include <QFileInfo>
#include <cstdlib>
#include <libavutil/error.h>
#include <QThread>

// 静态中断回调函数
int AudioDecoder::interrupt_callback(void* ctx) {
    AudioDecoder* decoder = static_cast<AudioDecoder*>(ctx);

    // 检查是否请求中断
    if (decoder->interrupt_abort_.load()) {
        return 1;
    }

    // 检查是否超时
    int64_t start_time = decoder->interrupt_start_time_.load();
    int64_t timeout_us = decoder->interrupt_timeout_us_.load();

    if (start_time == 0 || timeout_us == 0) {
        return 0;
    }

    int64_t current_time = av_gettime();
    int64_t elapsed = current_time - start_time;

    if (elapsed > timeout_us) {
        qDebug() << "[interrupt_callback] 操作超时，耗时:" << elapsed / 1000 << "ms";
        decoder->interrupt_abort_.store(true);
        return 1;
    }

    return 0;
}

AudioDecoder::AudioDecoder() {
    pkt_ = av_packet_alloc();
    if (!pkt_) {
        qCritical() << "构造函数：av_packet_alloc() 失败，pkt_ 为 NULL！";
        return;
    }

    frame_ = av_frame_alloc();
    resampled_frame_ = av_frame_alloc();
}

AudioDecoder::~AudioDecoder() {
    close();

    // 清理选项
    if (decoder_options_) {
        av_dict_free(&decoder_options_);
    }
}

void AudioDecoder::set_option(const QString& key, const QString& value) {
    std::lock_guard<std::mutex> lock(mtx_);
    av_dict_set(&decoder_options_, key.toUtf8().constData(), value.toUtf8().constData(), 0);
}

void AudioDecoder::disable_interrupt_callback() {
    if (fmt_ctx_) {
        fmt_ctx_->interrupt_callback.opaque = nullptr;
        fmt_ctx_->interrupt_callback.callback = nullptr;
    }
}

bool AudioDecoder::open(const QString& file_path) {
    close();

    qDebug() << "[AudioDecoder] 打开文件:" << file_path;

    // 重置中断状态
    interrupt_abort_.store(false);
    interrupt_start_time_.store(0);
    interrupt_timeout_us_.store(0);

    // 首先尝试普通打开（不使用中断回调）
    fmt_ctx_ = avformat_alloc_context();
    if (!fmt_ctx_) {
        qCritical() << "[AudioDecoder] 无法分配AVFormatContext";
        return false;
    }

    // 设置FFmpeg选项
    AVDictionary* format_opts = nullptr;

    // 使用合理的默认值，避免FFmpeg死循环
    av_dict_set(&format_opts, "analyzeduration", "1000000", 0);  // 1秒
    av_dict_set(&format_opts, "probesize", "65536", 0);         // 64KB
    av_dict_set(&format_opts, "max_analyze_duration", "1000000", 0);

    // 对于MP3文件，添加特定选项
    if (file_path.endsWith(".mp3", Qt::CaseInsensitive) ||
        file_path.endsWith(".MP3", Qt::CaseInsensitive)) {
        qDebug() << "[AudioDecoder] MP3文件，启用快速解码";
        av_dict_set(&format_opts, "flags", "+fastseek", 0);
    }

    // 尝试打开文件（不使用中断回调）
    qDebug() << "[AudioDecoder] 调用avformat_open_input...";
    int ret = avformat_open_input(&fmt_ctx_, file_path.toUtf8().constData(), nullptr, &format_opts);
    av_dict_free(&format_opts);

    if (ret < 0) {
        qDebug() << "[AudioDecoder] 普通打开失败，尝试使用中断回调...";
        return open_with_interrupt(file_path);
    }

    qDebug() << "[AudioDecoder] avformat_open_input成功";

    // 继续后续流程...
    return initialize_after_open();
}

bool AudioDecoder::open_with_interrupt(const QString& file_path) {
    qDebug() << "[AudioDecoder] 使用中断回调打开文件...";

    // 清理之前的上下文
    if (fmt_ctx_) {
        avformat_free_context(fmt_ctx_);
        fmt_ctx_ = nullptr;
    }

    // 创建新上下文
    fmt_ctx_ = avformat_alloc_context();
    if (!fmt_ctx_) {
        qCritical() << "[AudioDecoder] 无法分配AVFormatContext";
        return false;
    }

    // 设置中断回调
    interrupt_abort_.store(false);
    interrupt_start_time_.store(av_gettime());
    interrupt_timeout_us_.store(15 * 1000000); // 15秒超时

    fmt_ctx_->interrupt_callback.opaque = this;
    fmt_ctx_->interrupt_callback.callback = interrupt_callback;

    // 设置更保守的选项
    AVDictionary* format_opts = nullptr;
    av_dict_set(&format_opts, "analyzeduration", "500000", 0);  // 0.5秒
    av_dict_set(&format_opts, "probesize", "32768", 0);         // 32KB
    av_dict_set(&format_opts, "max_analyze_duration", "500000", 0);

    // 打开文件
    int ret = avformat_open_input(&fmt_ctx_, file_path.toUtf8().constData(), nullptr, &format_opts);
    av_dict_free(&format_opts);

    // 立即禁用中断回调
    disable_interrupt_callback();

    if (ret < 0) {
        if (interrupt_abort_.load()) {
            qCritical() << "[AudioDecoder] 打开文件超时（15秒）";
        } else {
            char error_msg[256];
            av_strerror(ret, error_msg, sizeof(error_msg));
            qCritical() << "[AudioDecoder] 即使使用中断回调也无法打开文件:" << error_msg;
        }
        avformat_free_context(fmt_ctx_);
        fmt_ctx_ = nullptr;
        return false;
    }

    qDebug() << "[AudioDecoder] avformat_open_input成功（使用中断回调）";

    // 重置中断状态
    interrupt_abort_.store(false);
    interrupt_start_time_.store(0);
    interrupt_timeout_us_.store(0);

    // 继续后续流程...
    return initialize_after_open();
}

bool AudioDecoder::initialize_after_open() {
    if (!fmt_ctx_) {
        return false;
    }

    // 查找流信息（不使用中断回调）
    qDebug() << "[AudioDecoder] 调用avformat_find_stream_info...";
    int ret = avformat_find_stream_info(fmt_ctx_, nullptr);

    if (ret < 0) {
        qWarning() << "[AudioDecoder] avformat_find_stream_info失败，尝试继续...";
        // 即使失败也尝试继续
    } else {
        qDebug() << "[AudioDecoder] avformat_find_stream_info成功";
    }

    // 查找音频流
    audio_stream_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_stream_index_ < 0) {
        qCritical() << "[AudioDecoder] 未找到音频流";
        close();
        return false;
    }

    audio_stream_ = fmt_ctx_->streams[audio_stream_index_];
    qDebug() << "[AudioDecoder] 音频流信息:";
    qDebug() << "  编码器ID:" << audio_stream_->codecpar->codec_id;
    qDebug() << "  编码器名称:" << avcodec_get_name(audio_stream_->codecpar->codec_id);
    qDebug() << "  采样率:" << audio_stream_->codecpar->sample_rate;
    qDebug() << "  声道数:" << audio_stream_->codecpar->channels;

    // 查找解码器
    const AVCodec* codec = avcodec_find_decoder(audio_stream_->codecpar->codec_id);
    if (!codec) {
        qCritical() << "[AudioDecoder] 未找到解码器";
        close();
        return false;
    }

    // 创建解码器上下文
    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) {
        qCritical() << "[AudioDecoder] 无法分配解码器上下文";
        close();
        return false;
    }

    // 复制参数到解码器上下文
    if (avcodec_parameters_to_context(codec_ctx_, audio_stream_->codecpar) < 0) {
        qCritical() << "[AudioDecoder] 无法复制流参数到解码器";
        close();
        return false;
    }

    // 设置解码器选项
    AVDictionary* decoder_opts = nullptr;

    // 如果是MP3解码器，设置特殊选项避免FFmpeg 6.1问题
    if (audio_stream_->codecpar->codec_id == AV_CODEC_ID_MP3) {
        qDebug() << "[AudioDecoder] MP3解码器，设置特殊选项";
        av_dict_set(&decoder_opts, "flags2", "+fast", 0);
    }

    // 合并全局选项
    if (decoder_options_) {
        AVDictionaryEntry* entry = nullptr;
        while ((entry = av_dict_get(decoder_options_, "", entry, AV_DICT_IGNORE_SUFFIX))) {
            av_dict_set(&decoder_opts, entry->key, entry->value, 0);
        }
    }

    // 打开解码器
    if (avcodec_open2(codec_ctx_, codec, &decoder_opts) < 0) {
        qCritical() << "[AudioDecoder] 无法打开解码器";
        if (decoder_opts) {
            av_dict_free(&decoder_opts);
        }
        close();
        return false;
    }

    if (decoder_opts) {
        av_dict_free(&decoder_opts);
    }

    // 解析元信息
    parse_metadata();

    // 初始化重采样器
    if (!init_resampler()) {
        qCritical() << "[AudioDecoder] 重采样器初始化失败";
        close();
        return false;
    }

    is_initialized_ = true;
    qInfo() << "[AudioDecoder] 初始化成功（目标格式：" << av_get_sample_fmt_name(target_sample_fmt_)
            << "，目标采样率：" << target_sample_rate_
            << "，目标声道数：" << target_channels_ << "）";
    return true;
}

bool AudioDecoder::reset_decoder_state() {
    std::lock_guard<std::mutex> lock(mtx_);

    qDebug() << "[AudioDecoder] 重置解码器状态...";

    // 重置中断标志
    interrupt_abort_.store(false);
    interrupt_start_time_.store(0);
    interrupt_timeout_us_.store(0);

    // 刷新解码器缓冲区
    if (codec_ctx_) {
        avcodec_flush_buffers(codec_ctx_);
        qDebug() << "[AudioDecoder] 解码器缓冲区已刷新";
    }

    // 重置EOF标志
    is_eof_ = false;
    _EOF_sent = false;

    qDebug() << "[AudioDecoder] 解码器状态已重置";
    return true;
}

bool AudioDecoder::request_safe_interrupt() {
    std::lock_guard<std::mutex> lock(mtx_);

    qDebug() << "[AudioDecoder] 请求安全中断...";

    // 设置中断标志
    interrupt_abort_.store(true);

    // 刷新解码器缓冲区
    if (codec_ctx_) {
        avcodec_flush_buffers(codec_ctx_);
        qDebug() << "[AudioDecoder] 解码器缓冲区已刷新";
    }

    // 重置EOF标志
    is_eof_ = false;
    _EOF_sent = false;

    qDebug() << "[AudioDecoder] 安全中断完成";
    return true;
}

void AudioDecoder::close() {
    std::lock_guard<std::mutex> lock(mtx_);

    // 禁用中断回调
    disable_interrupt_callback();

    // 重置中断状态
    interrupt_abort_.store(false);
    interrupt_start_time_.store(0);
    interrupt_timeout_us_.store(0);

    if (swr_ctx_) {
        swr_free(&swr_ctx_);
        swr_ctx_ = nullptr;
    }

    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }

    if (fmt_ctx_) {
        avformat_close_input(&fmt_ctx_);
        fmt_ctx_ = nullptr;
    }

    is_initialized_ = false;
    is_eof_ = false;
    audio_stream_index_ = -1;
    audio_stream_ = nullptr;
}

bool AudioDecoder::init_resampler() {
    if (!need_resample()) {
        qInfo() << "[重采样] 无需重采样（原始参数与目标一致）";
        return true;
    }

    if (swr_ctx_) {
        swr_free(&swr_ctx_);
    }

    swr_ctx_ = swr_alloc_set_opts(
        nullptr,
        AV_CH_LAYOUT_STEREO,
        target_sample_fmt_,
        target_sample_rate_,
        av_get_default_channel_layout(codec_ctx_->channels),
        codec_ctx_->sample_fmt,
        codec_ctx_->sample_rate,
        0, nullptr
    );

    if (!swr_ctx_ || swr_init(swr_ctx_) < 0) {
        qWarning() << "[重采样] 初始化失败";
        return false;
    }

    qInfo() << "[重采样] 初始化成功（原始格式：" << av_get_sample_fmt_name(codec_ctx_->sample_fmt)
            << "，原始采样率：" << codec_ctx_->sample_rate << "）";
    return true;
}

bool AudioDecoder::need_resample() const {
    if (!codec_ctx_) return false;
    return (codec_ctx_->sample_fmt != target_sample_fmt_) ||
           (codec_ctx_->sample_rate != target_sample_rate_) ||
           (codec_ctx_->channels != target_channels_);
}

void AudioDecoder::parse_metadata() {
    meta_.sampleRate = codec_ctx_->sample_rate;
    meta_.channels = codec_ctx_->channels;
    meta_.sampleFmt = codec_ctx_->sample_fmt;
    if (audio_stream_->duration != AV_NOPTS_VALUE) {
        meta_.durationMs = (int64_t)(av_q2d(audio_stream_->time_base) * audio_stream_->duration * 1000);
    } else {
        meta_.durationMs = 0;
    }
    qInfo() << "[元信息] 解析完成（时长：" << meta_.durationMs << "ms，声道数：" << meta_.channels << "）";
}

AVRational AudioDecoder::get_audio_time_base() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (audio_stream_) {
        return audio_stream_->time_base;
    }
    return {1, 1000};
}

bool AudioDecoder::decode_frame(AudioFrame& out_frame) {
    std::lock_guard<std::mutex> lock(mtx_);

    // 初始状态检查
    if (!is_initialized_) {
        qWarning() << "[DecodeFrame] 解码器未初始化";
        return false;
    }

    if (is_eof_) {
        return false;
    }

    // 检查中断状态（仅用于外部中断，不是回调中断）
    if (interrupt_abort_.load()) {
        qDebug() << "[DecodeFrame] 外部中断请求，跳过解码";
        return false;
    }

    // 重置输出帧
    memset(&out_frame, 0, sizeof(AudioFrame));
    out_frame.data = nullptr;

    // 解码循环
    while (true) {
        // 读取音频数据包
        int ret = av_read_frame(fmt_ctx_, pkt_);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                qInfo() << "[DecodeFrame] 已到文件尾，发送空包刷新解码器剩余帧";
                ret = avcodec_send_packet(codec_ctx_, nullptr);
                is_eof_ = true;
                qInfo() << "[DecodeFrame] 标记解码器为EOF状态";
                if (ret < 0) {
                    char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
                    av_strerror(ret, err_buf, sizeof(err_buf));
                    qWarning() << "[DecodeFrame] 发送空包失败：" << err_buf;
                    av_packet_unref(pkt_);
                    return false;
                }
            } else {
                char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
                av_strerror(ret, err_buf, sizeof(err_buf));
                qWarning() << "[DecodeFrame] 读取数据包失败：" << err_buf;

                // 如果是中断错误，检查中断标志
                if (ret == AVERROR_EXIT && interrupt_abort_.load()) {
                    qDebug() << "[DecodeFrame] 读取被外部中断";
                }

                av_packet_unref(pkt_);
                return false;
            }
        } else if (pkt_->stream_index != audio_stream_index_) {
            qDebug() << "[DecodeFrame] 跳过非音频流（流索引：" << pkt_->stream_index << "）";
            av_packet_unref(pkt_);
            continue;
        }

        // 发送数据包到解码器
        ret = avcodec_send_packet(codec_ctx_, pkt_);
        av_packet_unref(pkt_);
        if (ret < 0) {
            char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(ret, err_buf, sizeof(err_buf));
            qWarning() << "[DecodeFrame] 发送数据包到解码器失败：" << err_buf;
            return false;
        }

        // 接收解码后的原始帧
        ret = avcodec_receive_frame(codec_ctx_, frame_);
        if (ret == AVERROR(EAGAIN)) {
            qDebug() << "[DecodeFrame] 数据不足（EAGAIN），继续读取数据包";
            continue;
        } else if (ret == AVERROR_EOF) {
            qInfo() << "[DecodeFrame] 解码器已无剩余帧，标记 EOF";
            is_eof_ = true;
            return false;
        } else if (ret < 0) {
            char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(ret, err_buf, sizeof(err_buf));
            qWarning() << "[DecodeFrame] 接收解码帧失败：" << err_buf;
            return false;
        }

        // 重采样处理
        AVFrame* final_frame = frame_;
        bool use_resampled = false;

        if (need_resample()) {
            /*
            qInfo() << "[DecodeFrame] 启动重采样（原始格式：" << av_get_sample_fmt_name((AVSampleFormat)frame_->format)
                    << "，原始采样率：" << frame_->sample_rate
                    << "，原始样本数：" << frame_->nb_samples << "）";*/

            // 计算目标样本数
            int dst_nb_samples = av_rescale_rnd(
                swr_get_delay(swr_ctx_, frame_->sample_rate) + frame_->nb_samples,
                target_sample_rate_,
                frame_->sample_rate,
                AV_ROUND_UP
            );

            if (dst_nb_samples <= 0) {
                qWarning() << "[DecodeFrame] 重采样目标样本数无效（" << dst_nb_samples << "），跳过该帧";
                av_frame_unref(frame_);
                return false;
            }

            // 初始化重采样帧
            av_frame_unref(resampled_frame_);
            resampled_frame_->nb_samples = dst_nb_samples;
            resampled_frame_->format = target_sample_fmt_;
            resampled_frame_->channel_layout = AV_CH_LAYOUT_STEREO;
            resampled_frame_->channels = target_channels_;
            resampled_frame_->sample_rate = target_sample_rate_;

            // 分配重采样帧缓冲区
            ret = av_frame_get_buffer(resampled_frame_, 16);
            if (ret < 0) {
                char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
                av_strerror(ret, err_buf, sizeof(err_buf));
                qWarning() << "[DecodeFrame] 重采样缓冲区分配失败：" << err_buf;
                av_frame_unref(frame_);
                return false;
            }

            if (!resampled_frame_->data[0]) {
                qWarning() << "[DecodeFrame] 重采样缓冲区data[0]为空，跳过该帧";
                av_frame_unref(frame_);
                av_frame_unref(resampled_frame_);
                return false;
            }
            /*
            qInfo() << "[DecodeFrame] 重采样缓冲区分配成功（data[0]：" << (void*)resampled_frame_->data[0]
                    << "，对齐检查：" << ((uintptr_t)resampled_frame_->data[0] % 16 == 0 ? "16字节对齐" : "未对齐")
                    << "，缓冲区大小：" << resampled_frame_->linesize[0] << "字节）";
            */
            // 执行重采样
            int converted_samples = swr_convert(
                swr_ctx_,
                resampled_frame_->data,
                dst_nb_samples,
                (const uint8_t**)frame_->data,
                frame_->nb_samples
            );

            if (converted_samples <= 0) {
                char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
                av_strerror(converted_samples < 0 ? converted_samples : AVERROR_UNKNOWN, err_buf, sizeof(err_buf));
                qWarning() << "[DecodeFrame] 重采样执行失败（有效样本数：" << converted_samples << "）：" << err_buf;
                av_frame_unref(frame_);
                av_frame_unref(resampled_frame_);
                return false;
            }

            resampled_frame_->nb_samples = converted_samples;
            //qInfo() << "[DecodeFrame] 重采样成功（转换后样本数：" << converted_samples << "）";

            final_frame = resampled_frame_;
            use_resampled = true;
        }

        // 最终帧验证
        if (!final_frame->data[0] || final_frame->nb_samples <= 0 || final_frame->channels <= 0) {
            qWarning() << "[DecodeFrame] 最终帧无效（data[0]：" << (void*)final_frame->data[0]
                    << "，样本数：" << final_frame->nb_samples << "，声道数：" << final_frame->channels << "）";
            av_frame_unref(frame_);
            if (use_resampled) av_frame_unref(resampled_frame_);
            return false;
        }

        /*
        qInfo() << "[DecodeFrame] 最终帧验证通过（格式：" << av_get_sample_fmt_name((AVSampleFormat)final_frame->format)
                << "，声道数：" << final_frame->channels
                << "，样本数：" << final_frame->nb_samples
                << "，数据指针：" << (void*)final_frame->data[0] << "）";
        */
        // 填充自定义AudioFrame
        out_frame.channels = final_frame->channels;
        out_frame.sample_rate = final_frame->sample_rate;
        out_frame.nb_samples = final_frame->nb_samples;
        out_frame.sample_fmt = (AVSampleFormat)final_frame->format;
        out_frame.pts = final_frame->pts;

        // 计算缓冲区大小
        int bytes_per_sample = av_get_bytes_per_sample(out_frame.sample_fmt);
        size_t channel_data_size = (size_t)out_frame.nb_samples * bytes_per_sample;
        size_t total_data_size = channel_data_size * out_frame.channels;
        /*
        qInfo() << "[DecodeFrame] 计算缓冲区：单声道=" << channel_data_size << "字节，总大小=" << total_data_size
                << "字节（样本数=" << out_frame.nb_samples << "，字节/样本=" << bytes_per_sample << "）";
        */
        // 分配指针数组
        out_frame.data = new (std::nothrow) uint8_t*[out_frame.channels];
        if (!out_frame.data) {
            qCritical() << "[DecodeFrame] 指针数组分配失败（内存不足）";
            av_frame_unref(frame_);
            if (use_resampled) av_frame_unref(resampled_frame_);
            return false;
        }

        // 分配对齐的数据流缓冲区
        const int align = 16;
        if (out_frame.sample_fmt == AV_SAMPLE_FMT_S16) {
            uint8_t* buffer = nullptr;
            ret = posix_memalign((void**)&buffer, align, total_data_size);
            if (ret != 0 || !buffer) {
                qCritical() << "[DecodeFrame] 交错缓冲区分配失败（错误码：" << ret << "）";
                delete[] out_frame.data;
                out_frame.data = nullptr;
                av_frame_unref(frame_);
                if (use_resampled) av_frame_unref(resampled_frame_);
                return false;
            }

            out_frame.data[0] = buffer;
            out_frame.data[1] = nullptr;
            /*
            qInfo() << "[DecodeFrame] 交错缓冲区：地址=" << (void*)buffer
                    << "，对齐检查：" << ((uintptr_t)buffer % align == 0 ? "通过" : "失败")
                    << "，大小=" << total_data_size << "字节";
            */
            memcpy(out_frame.data[0], final_frame->data[0], total_data_size);
        } else if (out_frame.sample_fmt == AV_SAMPLE_FMT_S16P) {
            for (int i = 0; i < out_frame.channels; i++) {
                uint8_t* buffer = nullptr;
                ret = posix_memalign((void**)&buffer, align, channel_data_size);
                if (ret != 0 || !buffer) {
                    qCritical() << "[DecodeFrame] 平面缓冲区分配失败（声道" << i << "，错误码：" << ret << "）";
                    for (int j = 0; j < i; j++) {
                        free(out_frame.data[j]);
                    }
                    delete[] out_frame.data;
                    out_frame.data = nullptr;
                    av_frame_unref(frame_);
                    if (use_resampled) av_frame_unref(resampled_frame_);
                    return false;
                }

                out_frame.data[i] = buffer;
                memcpy(out_frame.data[i], final_frame->data[i], channel_data_size);
                /*
                qInfo() << "[DecodeFrame] 平面缓冲区（声道" << i << "）：地址=" << (void*)buffer
                        << "，对齐检查：" << ((uintptr_t)buffer % align == 0 ? "通过" : "失败")
                        << "，大小=" << channel_data_size << "字节";*/
            }
        } else {
            qCritical() << "[DecodeFrame] 不支持的音频格式：" << av_get_sample_fmt_name(out_frame.sample_fmt);
            delete[] out_frame.data;
            out_frame.data = nullptr;
            av_frame_unref(frame_);
            if (use_resampled) av_frame_unref(resampled_frame_);
            return false;
        }

        // 计算linesize
        out_frame.linesize = (out_frame.sample_fmt == AV_SAMPLE_FMT_S16) ? total_data_size : channel_data_size;
        //qInfo() << "[DecodeFrame] 填充完成：linesize=" << out_frame.linesize << "，data[0]=" << (void*)out_frame.data[0];

        // 释放临时资源
        av_frame_unref(frame_);
        if (use_resampled) {
            av_frame_unref(resampled_frame_);
        }

        // 解码成功
        // qInfo() << "[DecodeFrame] 解码完成，成功输出一帧";
        return true;
    }
}

bool AudioDecoder::seek(int64_t target_ms) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!fmt_ctx_ || !audio_stream_) return false;

    int64_t target_pts = (int64_t)(target_ms / 1000.0 * audio_stream_->time_base.den / audio_stream_->time_base.num);
    int ret = avformat_seek_file(fmt_ctx_, audio_stream_index_, 0, target_pts, target_pts, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, err_buf, sizeof(err_buf));
        qWarning() << "[定位] 失败：" << err_buf;
        return false;
    }

    avcodec_flush_buffers(codec_ctx_);
    is_eof_ = false;
    _EOF_sent = false;

    // 重置中断标志
    interrupt_abort_.store(false);

    qInfo() << "[定位] 成功到" << target_ms << "ms";
    return true;
}

void AudioDecoder::release_Frame(AudioFrame& out_frame) {
    if (!out_frame.data) return;

    // 释放数据缓冲区
    if (out_frame.sample_fmt == AV_SAMPLE_FMT_S16) {
        if (out_frame.data[0]) {
            free(out_frame.data[0]);
        }
    } else if (out_frame.sample_fmt == AV_SAMPLE_FMT_S16P) {
        for (int i = 0; i < out_frame.channels; i++) {
            if (out_frame.data[i]) {
                free(out_frame.data[i]);
            }
        }
    }

    // 释放指针数组
    delete[] out_frame.data;
    out_frame.data = nullptr;
}



double AudioDecoder::get_current_time()
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (!is_initialized_ || !audio_stream_) {
        return 0.0;
    }

    // 如果有当前的解码帧，使用帧的PTS
    if (frame_ && frame_->pts != AV_NOPTS_VALUE) {
        return frame_->pts * av_q2d(audio_stream_->time_base);
    }

    // 如果没有帧信息，返回0
    return 0.0;
}

double AudioDecoder::get_duration_in_seconds()
{
    if (!is_initialized_ || !audio_stream_) {
        return 0.0;
    }

    // 使用流时长，如果无效则使用容器时长
    if (audio_stream_->duration != AV_NOPTS_VALUE) {
        return audio_stream_->duration * av_q2d(audio_stream_->time_base);
    } else if (fmt_ctx_->duration != AV_NOPTS_VALUE) {
        return fmt_ctx_->duration / (double)AV_TIME_BASE;
    }

    return 0.0;
}

int64_t AudioDecoder::get_current_position_ms()
{
    return static_cast<int64_t>(get_current_time() * 1000);
}