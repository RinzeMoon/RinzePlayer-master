#ifndef AVDECODER_H
#define AVDECODER_H

#include <QObject>
#include <memory>
#include <thread>
#include <atomic>
#include <RingBuffer/RingBuffer.h>
#include <QMutex>
#include <QWaitCondition>
#include <functional>
#include <queue>

#include <SDL2/SDL.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/rational.h>
}

#include "../Header/VideoPart/AVRes/AVData.h"

class AVDecoder : public QObject {
    Q_OBJECT

public:
    enum class State {
        Stopped,
        Decoding,
        Paused,
        Seeking
    };

    struct AudioSpec {
        int sample_rate = 0;
        int channels = 0;
        AVSampleFormat format = AV_SAMPLE_FMT_NONE;
        bool is_planar = false;
        uint64_t channel_layout = 0;

        bool isValid() const {
            return sample_rate > 0 && channels > 0 && format != AV_SAMPLE_FMT_NONE;
        }

        bool needsResamplingTo(const AudioSpec& target) const {
            return sample_rate != target.sample_rate ||
                   channels != target.channels ||
                   format != target.format;
        }

        SDL_AudioFormat toSDLAudioFormat() const {
            AVSampleFormat packed_fmt = is_planar ?
                (AVSampleFormat)av_get_packed_sample_fmt(format) : format;

            switch (packed_fmt) {
            case AV_SAMPLE_FMT_U8:   return AUDIO_U8;
            case AV_SAMPLE_FMT_S16:  return AUDIO_S16SYS;
            case AV_SAMPLE_FMT_S32:  return AUDIO_S32SYS;
            case AV_SAMPLE_FMT_FLT:  return AUDIO_F32SYS;
            default:                 return AUDIO_S16SYS;
            }
        }
    };

    using PacketFetcher = std::function<std::shared_ptr<AVData>()>;

    explicit AVDecoder(QObject* parent = nullptr);
    ~AVDecoder();

    bool initVideoDecoder(AVCodecParameters* params, const uint8_t* extradata = nullptr, int extradata_size = 0);
    bool initAudioDecoder(AVCodecParameters* params, const uint8_t* extradata = nullptr, int extradata_size = 0);

    bool initVideoDecoder(AVCodecParameters* params);
    bool initAudioDecoder(AVCodecParameters* params);

    void setVideoFetcher(const PacketFetcher& fetcher) {
        video_fetcher_ = fetcher;
    }

    void setAudioFetcher(const PacketFetcher& fetcher) {
        audio_fetcher_ = fetcher;
    }

    void start();
    void pause();
    void resume();
    void stop();
    void flush();
    void seek(int64_t timestamp_ms);

    std::shared_ptr<AVData> getNextVideoFrame(int timeout_ms = 10);
    std::shared_ptr<AVData> getNextAudioFrame(int timeout_ms = 10);


    bool hasVideoFrames() const {
        return use_video_callback_ ? video_callback_count_ > 0 : video_frame_queue_.size() > 0;
    }

    bool hasAudioFrames() const {
        return use_audio_callback_ ? audio_callback_count_ > 0 : audio_frame_queue_.size() > 0;
    }

    State state() const { return state_; }
    bool hasVideo() const { return video_codec_ctx_ != nullptr; }
    bool hasAudio() const { return audio_codec_ctx_ != nullptr; }
    size_t videoQueueSize() const {
        return use_video_callback_ ? video_callback_count_.load(): video_frame_queue_.size();
    }
    int getAudioQueueSize() const {
        return use_audio_callback_ ? audio_callback_count_.load() : static_cast<int>(audio_frame_queue_.size());
    }

    void setVideoStreamIndex(int index) { video_stream_index_ = index; }
    void setAudioStreamIndex(int index) { audio_stream_index_ = index; }
    void setVideoTimeBase(const AVRational& time_base) {
        video_time_base_ = time_base;
    }
    void setAudioTimeBase(const AVRational& time_base) {
        audio_time_base_ = time_base;
    }
    void setAudioOutputSpec(const AudioSpec& spec);

    AudioSpec getAudioSpec() const { return audio_spec_; }

    struct Statistics {
        int64_t video_packets_fetched = 0;
        int64_t audio_packets_fetched = 0;
        int64_t video_frames_decoded = 0;
        int64_t audio_frames_decoded = 0;
        int64_t video_frames_dropped = 0;
        int64_t audio_frames_dropped = 0;
        int64_t video_frames_output = 0;
        int64_t audio_frames_output = 0;
        double video_decode_fps = 0.0;
        double audio_decode_fps = 0.0;
    };

    Statistics getStatistics() const;

    double getFPS()
    {
        auto fps = av_q2d(video_codec_ctx_->framerate);
        qDebug() << "从编码上下文获取帧率：" << fps << "fps";
        return fps;
    }
signals:
    void stateChanged(State state);
    void errorOccurred(const QString& error);
    void videoFrameAvailable();
    void audioFrameAvailable();
    void frameDropped(bool is_video, int64_t count);

private:
    void fetchThread();
    void decodeVideoThread();
    void decodeAudioThread();

    bool decodeVideoPacket(std::shared_ptr<AVData> packet);
    bool decodeAudioPacket(std::shared_ptr<AVData> packet);

    void processDecodedVideoFrame(AVFrame* frame);
    void processDecodedAudioFrame(AVFrame* frame);

    std::shared_ptr<AVData> createVideoFrame(AVFrame* frame);
    std::shared_ptr<AVData> createAudioFrame(AVFrame* frame);
    std::shared_ptr<AVData> resampleAudioFrame(AVFrame* frame);

    void cleanup();

private:
    AVCodecContext* video_codec_ctx_ = nullptr;
    AVCodecContext* audio_codec_ctx_ = nullptr;

    PacketFetcher video_fetcher_;
    PacketFetcher audio_fetcher_;

    std::atomic<bool> use_video_callback_{false};
    std::atomic<bool> use_audio_callback_{false};

    RingBuffer<std::shared_ptr<AVData>> video_packet_queue_{400};
    RingBuffer<std::shared_ptr<AVData>> audio_packet_queue_{1000};
    RingBuffer<std::shared_ptr<AVData>> video_frame_queue_{800};
    RingBuffer<std::shared_ptr<AVData>> audio_frame_queue_{1000};

    std::thread fetch_thread_;
    std::thread video_decode_thread_;
    std::thread audio_decode_thread_;

    std::atomic<State> state_{State::Stopped};
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> pause_fetch_{false};

    SwrContext* swr_ctx_ = nullptr;
    AudioSpec audio_spec_;

    int video_stream_index_ = -1;
    int audio_stream_index_ = -1;
    AVRational video_time_base_ = {0, 0};
    AVRational audio_time_base_ = {0, 0};

    QMutex mutex_;
    QMutex audio_mutex_;
    QMutex video_mutex_;
    QMutex VOutputMutex;
    QMutex AOutputMutex;
    QWaitCondition video_cond_;
    QWaitCondition audio_cond_;
    QWaitCondition fetch_cond_;
    QWaitCondition output_video_cond_;
    QWaitCondition output_audio_cond_;

    std::atomic<int> video_packets_fetched_{0};
    std::atomic<int> audio_packets_fetched_{0};
    std::atomic<int> video_frames_decoded_{0};
    std::atomic<int> audio_frames_decoded_{0};
    std::atomic<int> video_frames_dropped_{0};
    std::atomic<int> audio_frames_dropped_{0};
    std::atomic<int> video_frames_output_{0};
    std::atomic<int> audio_frames_output_{0};
    std::atomic<int> video_callback_count_{0};
    std::atomic<int> audio_callback_count_{0};

    std::chrono::steady_clock::time_point video_decode_start_;
    std::chrono::steady_clock::time_point audio_decode_start_;
    std::chrono::duration<double> total_video_decode_time_{0};
    std::chrono::duration<double> total_audio_decode_time_{0};

    std::mutex m_codecMutex;
    std::mutex m_audioCodecMutex;
    std::atomic<bool> m_isDecodePaused{false};
};

#endif // AVDECODER_H