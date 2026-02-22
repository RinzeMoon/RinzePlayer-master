//
// Created by lsy on 2026/1/26.
//
#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QObject>
#include <QMutex>
#include <QAtomicInteger>
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <memory>

#include "../AVDecode/AVDecoder.h"

class AudioOutput : public QObject {
    Q_OBJECT

public:
    enum class State {
        Stopped,
        Playing,
        Paused,
        Error
    };

    explicit AudioOutput(QObject* parent = nullptr);
    ~AudioOutput();

    // ========== 初始化与配置 ==========
    bool initialize(const AVDecoder::AudioSpec& spec);
    bool initialize(int sample_rate, int channels, AVSampleFormat format = AV_SAMPLE_FMT_S16);

    // ========== 控制接口 ==========
    bool start();
    void pause();
    void resume();
    void stop();
    void flush();

    // ========== 配置接口 ==========
    void setVolume(float volume);  // 0.0 ~ 1.0
    void setBufferSize(int samples) { buffer_samples_ = samples; }

    // ========== 状态查询 ==========
    State state() const { return state_; }
    bool isInitialized() const { return is_initialized_; }
    bool isPlaying() const { return state_ == State::Playing; }
    bool isPaused() const { return state_ == State::Paused; }
    float volume() const;

    // ========== 获取信息 ==========
    int sampleRate() const { return sample_rate_; }
    int channels() const { return channels_; }
    int bufferSamples() const { return buffer_samples_; }
    int bytesPerSample() const { return bytes_per_sample_; }

    void fillSingleChannelSilence(uint8_t* stream, int samples, AVSampleFormat format);

    using AudioCallback = std::function<void(uint8_t* stream, int len)>;

    void setAudioCallback(AudioCallback callback) {
        audio_callback_ = callback;
    }

    SDL_AudioDeviceID getDeviceID()
    {
        return audio_device_;
    }

    void debugDump() const;
signals:
    void stateChanged(State state);
    void errorOccurred(const QString& error);
    void bufferUnderrun();      // 缓冲区欠载
    void bufferOverrun();       // 缓冲区过载

private:

    void processAudio(uint8_t* stream, int len);

    // ========== 内部方法 ==========
    bool setupSDL();
    bool openAudioDevice();
    void closeAudioDevice();
    void cleanup();
    void fillSilence(uint8_t* stream, int len, AVSampleFormat format);

    // 音频格式转换
    SDL_AudioFormat avFormatToSDL(AVSampleFormat format);
    int getBytesPerSample(AVSampleFormat format);

    // ========== 成员变量 ==========
    // SDL2音频设备
    SDL_AudioDeviceID audio_device_ = 0;
    SDL_AudioSpec desired_spec_;
    SDL_AudioSpec obtained_spec_;

    // 音频参数
    int sample_rate_ = 44100;
    int channels_ = 2;
    AVSampleFormat format_ = AV_SAMPLE_FMT_S16;
    int bytes_per_sample_ = 2;
    int buffer_samples_ = 1024;  // 缓冲区样本数

    QMutex decoder_mutex_;

    // 状态控制
    State state_ {(State::Stopped)};
    QAtomicInteger<bool> is_initialized_{false};
    QAtomicInteger<bool> stop_requested_{false};
    QAtomicInteger<bool> pause_requested_{false};

    // 错误处理
    QString last_error_;

    // ========== 新增：音频时钟控制 ==========
    qint64 playback_start_time_ = 0;         // 开始播放的系统时间（毫秒）
    double audio_position_ = 0.0;            // 当前播放位置（秒）
    double total_audio_duration_ = 0.0;      // 总共播放的音频时长（秒）
    double playback_speed_ = 1.0;            // 播放速度因子
    QAtomicInteger<int> underrun_count_{0};  // 缓冲区下溢计数
    QAtomicInteger<int> overrun_count_{0};   // 缓冲区过载计数

private:
    SwrContext* m_swrCtx = nullptr; // FLTP转S16的重采样器

    AudioCallback audio_callback_;

    static void sdlAudioCallback(void* userdata, Uint8* stream, int len);

};

#endif // AUDIOOUTPUT_H