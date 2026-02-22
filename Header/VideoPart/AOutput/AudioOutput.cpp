// AudioOutput.cpp
#include "AudioOutput.h"

#include "AudioOutput.h"
#include <QDebug>
#include <QThread>
#include <cmath>

extern "C" {
#include <libavutil/opt.h>
}

AudioOutput::AudioOutput(QObject* parent)
    : QObject(parent) {
    // 初始化SDL2音频子系统
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        last_error_ = QString("SDL音频初始化失败: %1").arg(SDL_GetError());
        qCritical() << "AudioOutput:" << last_error_;
    } else {
        qDebug() << "AudioOutput: SDL2音频子系统初始化成功";

    }
}

AudioOutput::~AudioOutput() {
    stop();
    cleanup();

    // 退出SDL2音频子系统
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    qDebug() << "AudioOutput: Destroyed";
}

bool AudioOutput::initialize(const AVDecoder::AudioSpec& spec) {
    if (!spec.isValid()) {
        qCritical() << "AudioOutput: 无效的音频参数";
        return false;
    }

    // 保存原始规格
    AVDecoder::AudioSpec source_spec_ = spec;
    AVDecoder::AudioSpec target_spec_;

    // 转换为SDL兼容的格式（打包格式）
    target_spec_.sample_rate = spec.sample_rate;
    target_spec_.channels = spec.channels;
    target_spec_.format = spec.is_planar ?
        (AVSampleFormat)av_get_packed_sample_fmt(spec.format) : spec.format;

    qDebug() << "AudioOutput: 源规格 -> SDL规格"
             << "\n  采样率:" << source_spec_.sample_rate << "->" << target_spec_.sample_rate
             << "\n  声道:" << source_spec_.channels << "->" << target_spec_.channels
             << "\n  格式:" << av_get_sample_fmt_name(source_spec_.format)
             << "->" << av_get_sample_fmt_name(target_spec_.format)
             << (source_spec_.is_planar ? " (平面->打包)" : "");

    return initialize(target_spec_.sample_rate, target_spec_.channels, target_spec_.format);
}

bool AudioOutput::initialize(int sample_rate, int channels, AVSampleFormat format) {
    if (is_initialized_) {
        cleanup();
    }

    buffer_samples_ = 8192;

    AVSampleFormat packed_format = (AVSampleFormat)av_get_packed_sample_fmt(format);
    if (packed_format == AV_SAMPLE_FMT_NONE || packed_format == AV_SAMPLE_FMT_FLT) {
        packed_format = AV_SAMPLE_FMT_FLT; // 兜底为SDL支持的浮点打包格式
    }

    sample_rate_ = sample_rate;
    channels_ = channels;
    format_ = packed_format;
    bytes_per_sample_ = getBytesPerSample(format);

    // 设置SDL2音频参数
    if (!setupSDL()) {
        last_error_ = "设置SDL音频参数失败";
        qWarning() << "AudioOutput:" << last_error_;
        return false;
    }

    is_initialized_ = true;
    state_ = State::Stopped;

    qDebug() << "AudioOutput: Initialized -"
             << "Sample Rate:" << sample_rate_
             << "Channels:" << channels_
             << "Format:" << av_get_sample_fmt_name(format_)
             << "Buffer Samples:" << buffer_samples_;

    return true;
}

bool AudioOutput::setupSDL() {
    // 清除旧的spec
    memset(&desired_spec_, 0, sizeof(desired_spec_));
    memset(&obtained_spec_, 0, sizeof(obtained_spec_));

    // 设置期望的音频参数
    desired_spec_.freq = sample_rate_;
    desired_spec_.channels = static_cast<Uint8>(channels_);
    desired_spec_.format = avFormatToSDL(format_);
    desired_spec_.samples = static_cast<Uint16>(buffer_samples_);
    desired_spec_.userdata = this;  // 传递this指针
    desired_spec_.callback = sdlAudioCallback;  // 设置静态回调函数

    if (desired_spec_.format == 0) {
        last_error_ = "不支持的音频格式";
        return false;
    }

    // 打开音频设备
    return openAudioDevice();
}

bool AudioOutput::openAudioDevice() {
    // 尝试打开音频设备
    audio_device_ = SDL_OpenAudioDevice(
    nullptr,      // 设备名称
    0,            // 播放设备
    &desired_spec_,
    &obtained_spec_,
    SDL_AUDIO_ALLOW_FREQUENCY_CHANGE |  // 允许采样率变化
    SDL_AUDIO_ALLOW_CHANNELS_CHANGE     // 允许声道数变化
);

    if (audio_device_ == 0) {
        last_error_ = QString("打开音频设备失败: %1").arg(SDL_GetError());
        return false;
    }

    // 计算期望的字节数
    int bytes_per_sample_sdl = 0;
    switch (obtained_spec_.format) {
    case AUDIO_U8:
    case AUDIO_S8: bytes_per_sample_sdl = 1; break;
    case AUDIO_U16LSB:
    case AUDIO_S16LSB:
    case AUDIO_U16MSB:
    case AUDIO_S16MSB: bytes_per_sample_sdl = 2; break;
    case AUDIO_S32LSB:
    case AUDIO_S32MSB:
    case AUDIO_F32LSB:
    case AUDIO_F32MSB: bytes_per_sample_sdl = 4; break;
    }

    int expected_bytes = obtained_spec_.samples * obtained_spec_.channels * bytes_per_sample_sdl;
    qDebug() << "SDL期望每次回调:" << expected_bytes << "字节";
    qDebug() << "======================";

    // 更新我们的bytes_per_sample_
    bytes_per_sample_ = bytes_per_sample_sdl;
    // 检查实际获得的格式是否与期望的一致
    if (obtained_spec_.format != desired_spec_.format ||
        obtained_spec_.channels != desired_spec_.channels ||
        abs(obtained_spec_.freq - desired_spec_.freq) > desired_spec_.freq * 0.05) {  // 允许5%的采样率误差
        qWarning() << "AudioOutput: 获得的音频参数与期望的不完全一致";
        // 更新实际使用的参数
        sample_rate_ = obtained_spec_.freq;
        channels_ = obtained_spec_.channels;
    }

    return true;
}

bool AudioOutput::start() {
    if (!is_initialized_) {
        qWarning() << "AudioOutput: Not initialized";
        return false;
    }

    if (state_ == State::Playing) {
        qDebug() << "AudioOutput: Already playing";
        return true;
    }

    if (audio_device_ == 0) {
        qWarning() << "AudioOutput: Audio device not opened";
        return false;
    }

    // 重置状态
    stop_requested_ = false;
    pause_requested_ = false;

    // 重置播放时钟
    playback_start_time_ = QDateTime::currentMSecsSinceEpoch();
    audio_position_ = 0.0;
    total_audio_duration_ = 0.0;
    underrun_count_ = 0;
    overrun_count_ = 0;

    // 启动音频设备
    SDL_PauseAudioDevice(audio_device_, 0);  // 0表示取消暂停（开始播放）

    state_ = State::Playing;
    emit stateChanged(state_);

    qDebug() << "AudioOutput: Started ";
    return true;
}

void AudioOutput::pause() {
    if (state_ != State::Playing) {
        return;
    }

    // 暂停音频设备
    SDL_PauseAudioDevice(audio_device_, 1);  // 1表示暂停

    pause_requested_ = true;
    state_ = State::Paused;
    emit stateChanged(state_);

    qDebug() << "AudioOutput: Paused";
}

void AudioOutput::resume() {
    if (state_ != State::Paused) {
        return;
    }

    // 恢复音频设备
    SDL_PauseAudioDevice(audio_device_, 0);  // 0表示取消暂停

    pause_requested_ = false;
    state_ = State::Playing;
    emit stateChanged(state_);

    qDebug() << "AudioOutput: Resumed";
}

void AudioOutput::stop() {
    if (state_ == State::Stopped) {
        return;
    }

    stop_requested_ = true;

    // 暂停音频设备
    if (audio_device_ != 0) {
        SDL_PauseAudioDevice(audio_device_, 1);
    }

    // 清空音频缓冲区
    SDL_ClearQueuedAudio(audio_device_);

    state_ = State::Stopped;
    emit stateChanged(state_);

    qDebug() << "AudioOutput: Stopped";
}

void AudioOutput::flush() {
    if (audio_device_ != 0) {
        SDL_ClearQueuedAudio(audio_device_);
    }

    qDebug() << "AudioOutput: Flushed";
}

void AudioOutput::setVolume(float volume) {
    // SDL2的音量控制比较有限，通常需要混合器支持
    // 这里可以留空，或者使用SDL_Mixer库
    qDebug() << "AudioOutput: Volume control not implemented with SDL2 directly";
}

float AudioOutput::volume() const {
    // SDL2原生不支持直接获取音量
    return 1.0f;
}


void AudioOutput::processAudio(uint8_t* stream, int len) {
    // 1. 清空SDL缓冲区（避免杂音）
    memset(stream, 0, len);

    if (stop_requested_ || pause_requested_) {
        return;
    }

    // 2. 如果有自定义回调，调用它
    if (audio_callback_) {
        audio_callback_(stream, len);
        return;
    }

    // 3. 如果没有回调，填充静音
    fillSilence(stream, len, format_);
}


void AudioOutput::fillSilence(uint8_t* stream, int len, AVSampleFormat format) {
    if (len <= 0 || !stream) return;
    int bytes_per_sample = av_get_bytes_per_sample(format);
    int channels = channels_;
    bool is_planar = av_sample_fmt_is_planar(format);

    if (bytes_per_sample <= 0 || channels <= 0) {
        memset(stream, 0, len);
        return;
    }

    int total_samples = len / bytes_per_sample;
    if (is_planar) {
        int samples_per_channel = total_samples / channels;
        int bytes_per_channel = samples_per_channel * bytes_per_sample;
        for (int ch = 0; ch < channels; ch++) {
            uint8_t* ch_stream = stream + ch * bytes_per_channel;
            fillSingleChannelSilence(ch_stream, samples_per_channel, format);
        }
    } else {
        fillSingleChannelSilence(stream, total_samples, format);
    }
}

void AudioOutput::fillSingleChannelSilence(uint8_t* stream, int samples, AVSampleFormat format) {
    if (samples <= 0 || !stream) return;
    int bytes = samples * av_get_bytes_per_sample(format);
    switch (format) {
    case AV_SAMPLE_FMT_U8: case AV_SAMPLE_FMT_U8P:
        memset(stream, 128, bytes);
        break;
    case AV_SAMPLE_FMT_S16: case AV_SAMPLE_FMT_S16P:
    case AV_SAMPLE_FMT_S32: case AV_SAMPLE_FMT_S32P:
        memset(stream, 0, bytes);
        break;
    case AV_SAMPLE_FMT_FLT: case AV_SAMPLE_FMT_FLTP:
        memset(stream, 0, bytes);
        break;
    default:
        memset(stream, 0, bytes);
    }
}

SDL_AudioFormat AudioOutput::avFormatToSDL(AVSampleFormat format) {
    AVSampleFormat packed_fmt = (AVSampleFormat)av_get_packed_sample_fmt(format);
    switch (packed_fmt) {
    case AV_SAMPLE_FMT_U8:   return AUDIO_U8;
    case AV_SAMPLE_FMT_S16:  return AUDIO_S16SYS;
    case AV_SAMPLE_FMT_S32:  return AUDIO_S32SYS;
    case AV_SAMPLE_FMT_FLT:  return AUDIO_F32SYS;
    default: return AUDIO_S16SYS;
    }
}

int AudioOutput::getBytesPerSample(AVSampleFormat format) {
    switch (format) {
        case AV_SAMPLE_FMT_U8:   return 1;
        case AV_SAMPLE_FMT_S16:  return 2;
        case AV_SAMPLE_FMT_S32:  return 4;
        case AV_SAMPLE_FMT_FLT:  return 4;
        case AV_SAMPLE_FMT_U8P:  return 1;
        case AV_SAMPLE_FMT_S16P: return 2;
        case AV_SAMPLE_FMT_S32P: return 4;
        case AV_SAMPLE_FMT_FLTP: return 4;
        default:                 return 2;
    }
}

void AudioOutput::closeAudioDevice() {
    if (audio_device_ != 0) {
        SDL_CloseAudioDevice(audio_device_);
        audio_device_ = 0;
    }
}

void AudioOutput::cleanup() {
    stop();
    closeAudioDevice();
    is_initialized_ = false;
    qDebug() << "AudioOutput: Cleanup completed";
}

void AudioOutput::debugDump() const {
    qDebug() << "=== AudioOutput 状态诊断 ===";
    qDebug() << "设备ID:" << audio_device_;
    qDebug() << "初始化状态:" << is_initialized_;
    qDebug() << "当前状态:" << (state_ == State::Playing ? "Playing" :
                                state_ == State::Paused ? "Paused" : "Stopped");
    if (audio_device_ != 0) {
        SDL_AudioStatus status = SDL_GetAudioDeviceStatus(audio_device_);
        qDebug() << "SDL硬件状态:" << (status == SDL_AUDIO_PLAYING ? "PLAYING" :
                                      status == SDL_AUDIO_PAUSED ? "PAUSED" : "STOPPED");
        qDebug() << "SDL获得格式: freq=" << obtained_spec_.freq
                 << ", channels=" << (int)obtained_spec_.channels
                 << ", format=" << QString::number(obtained_spec_.format, 16);
    }
    qDebug() << "==========================";
}

void AudioOutput::sdlAudioCallback(void* userdata, Uint8* stream, int len) {
    AudioOutput* self = static_cast<AudioOutput*>(userdata);

    if (self) {
        // 调试：显示回调被调用
        static int call_count = 0;
        if (++call_count % 100 == 0) {
            qDebug() << "【SDL音频回调】#"
                     << call_count << "请求" << len << "字节";
        }

        // 先填充静音兜底
        memset(stream, 0, len);

        if (self->audio_callback_) {
            // 调用自定义回调
            self->audio_callback_(stream, len);
        } else {
            // 没有设置回调，填充静音
            if (call_count % 100 == 0) {
                qDebug() << "【警告】没有设置音频回调，填充静音";
            }
        }
    } else {
        // self为空时，强制填充静音
        memset(stream, 0, len);
    }
}