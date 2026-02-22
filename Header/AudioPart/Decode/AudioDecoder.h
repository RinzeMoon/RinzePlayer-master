#ifndef RINZEPLAYER_AUDIODECODER_H
#define RINZEPLAYER_AUDIODECODER_H

#include <QString>
#include <mutex>
#include <atomic>

#include "../../../Global/Global.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/samplefmt.h>
#include <libavutil/time.h>
}

using AudioUtil::AudioFrame;
using AudioUtil::AudioMeta;

class AudioDecoder
{
public:
    AudioDecoder();
    ~AudioDecoder();

    bool open(const QString& file_path);
    void close();
    bool decode_frame(AudioFrame& out_frame);
    bool seek(int64_t target_ms);
    const AudioMeta& get_meta() const { return meta_; }
    bool is_initialized() const { return is_initialized_; }
    bool is_eof() const { return is_eof_; }
    bool is_interrupted() const { return interrupt_abort_.load(); }
    AVRational get_audio_time_base();

    // 重置解码器状态（中断后恢复）
    bool reset_decoder_state();

    // 安全中断方法
    bool request_safe_interrupt();

    // 设置解码器选项
    void set_option(const QString& key, const QString& value);

    int get_target_sample_rate() const { return target_sample_rate_; }
    int get_target_channels() const { return target_channels_; }
    int get_target_SampleRate() const { return target_sample_rate_; }
    int get_target_Channels() const { return target_channels_; }

    void release_Frame(AudioFrame& out_frame);

    bool initialize_after_open();

    double get_current_time();
    double get_duration_in_seconds();
    int64_t get_current_position_ms();

private:
    // 私有方法
    bool open_with_interrupt(const QString& file_path);
    bool init_resampler();
    bool need_resample() const;
    void parse_metadata();
    void disable_interrupt_callback();

    // 静态中断回调函数
    static int interrupt_callback(void* ctx);

private:
    // FFmpeg核心资源
    AVFormatContext* fmt_ctx_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    AVStream* audio_stream_ = nullptr;
    int audio_stream_index_ = -1;
    AVPacket* pkt_ = nullptr;
    AVFrame* frame_ = nullptr;

    // 重采样相关
    SwrContext* swr_ctx_ = nullptr;
    AVFrame* resampled_frame_ = nullptr;
    AVSampleFormat target_sample_fmt_ = AV_SAMPLE_FMT_S16;
    int target_sample_rate_ = 44100;
    int target_channels_ = 2;

    // 中断控制（原子操作，线程安全）
    std::atomic<bool> interrupt_abort_{false};
    std::atomic<int64_t> interrupt_start_time_{0};
    std::atomic<int64_t> interrupt_timeout_us_{0};

    // 状态与元信息
    AudioMeta meta_;
    bool is_initialized_ = false;
    bool is_eof_ = false;
    std::mutex mtx_;
    bool _EOF_sent = false;

    // 解码器选项
    AVDictionary* decoder_options_ = nullptr;
};

#endif // RINZEPLAYER_AUDIODECODER_H