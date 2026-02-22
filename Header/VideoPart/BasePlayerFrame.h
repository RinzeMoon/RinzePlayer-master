//
// Created by lsy on 2026/1/13.
//

#ifndef BASEPLAYERFRAME_H
#define BASEPLAYERFRAME_H


#include <QVBoxLayout>
#include "../Header/VideoPart/AvSourceParse/AVSourceResolver.h"
#include "VideoPart/AVDataFetch/AVDataFetcher.h"
#include "../VideoPart/OpenGLRender/OpenGLRender.h"
#include "../VideoPart/AVDecode/AVDecoder.h"
#include "../VideoPart/AOutput/AudioOutput.h"

#include <QtConcurrent/QtConcurrentRun>


#include <../Global/Global.h>

#include "AVSync/AVSync.h"

class StandardController;
using RinGlobal::VideoPlayerMode;

// 控制器接口
class BaseController : public QWidget {
    Q_OBJECT
public:
    BaseController(QWidget* parent = nullptr) : QWidget(parent) {}
    virtual ~BaseController() = default;

    virtual void setMode(VideoPlayerMode mode) = 0;
    virtual void updateState(bool playing, qint64 position, qint64 duration) = 0;

signals:
    void playClicked();
    void pauseClicked();
    void stopClicked();
    void seekRequested(qint64 position);
    void volumeChanged(int volume);
    void fullscreenToggled();
    void modeChanged(VideoPlayerMode mode);
};

// 播放接口
class BasePlayer : public QWidget {
    Q_OBJECT
public:
    BasePlayer(QWidget* parent = nullptr) : QWidget(parent) {}
    virtual ~BasePlayer() = default;

    virtual bool load(const QString& source, VideoPlayerMode mode) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seek(qint64 position) = 0;
    virtual void setVolume(int volume) = 0;

    virtual bool isPlaying() const = 0;
    virtual qint64 duration() const = 0;
    virtual qint64 position() const = 0;
    virtual int volume() const = 0;

signals:
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void playbackStarted();
    void playbackPaused();
    void playbackStopped();
    void errorOccurred(const QString& error);
};

// 播放器框架基类（左侧播放页面）
class BasePlayerFrame : public QWidget
{
    Q_OBJECT

public:
    explicit BasePlayerFrame(QWidget* parent = nullptr);
    virtual ~BasePlayerFrame();

    // 简化：先去掉setController和setPlayer
    void loadMedia(const QString& source, VideoPlayerMode mode);
    VideoPlayerMode currentMode() const { return m_currentMode; }

    // 获取视频渲染器
    OpenGLRender* videoRenderer() const { return m_videoRenderer; }

    // 接收Fetcher的方法
    bool setFetcher(std::unique_ptr<AVDataFetcher> fetcher);
    bool loadWithFetcher(VideoPlayerMode mode);

    // 辅助方法
    bool isPlaying() const;
    const QString& currentSource() const;

    void setController(QWidget* controller);

    SourceInfo getCurrentSource() const
    {
        return m_currentSource;
    }

    void startAsyncFrameExtraction();
    void startAudioExtraction();

    void startAudioPlayback();

    void sdlAudioCallback(uint8_t* stream, int len);
    signals:
        void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void playbackStarted();
    void playbackPaused();
    void playbackStopped();
    void errorOccurred(const QString& error);


protected:
    void initUi();
    void connectSignals();

private:
    bool startPlayback();

protected:
    QVBoxLayout* m_mainLayout;
    OpenGLRender* m_videoRenderer;
    AudioOutput* m_AOutput;
    VideoPlayerMode m_currentMode = VideoPlayerMode::LOCAL_FILE;

    SourceInfo m_currentSource;

    QWidget* m_controller;

    AVSync * getSync()
    {
        return m_avSync;
    }


private:
    using FrameCallback = std::function<void(const std::shared_ptr<AVData>&)>;

    std::unique_ptr<AVDataFetcher> m_dataFetcher;

    // 解码器
    std::shared_ptr<AVDecoder> m_Decoder = nullptr;

    // 播放状态
    std::atomic<bool> m_isPlaying{false};

    QFuture<void> m_extractorFuture; // 视频...
    std::atomic<bool> m_shouldStopExtractor{false};

    QFuture<void> m_audioExtractorFuture;       // 音频提取线程
    std::atomic<bool> m_shouldStopAudioExtractor{false}; // 音频停止标志

    QFuture<void> m_audioPlaybackFuture;

    AVSync* m_avSync = nullptr;

private slots:
    void onPlayClicked();
    void onPauseClicked();
    void onStopClicked();
    void onSeekRequested(qint64 position);
    void onVolumeChanged(int volume);
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onDataPacketReady(std::shared_ptr<AVData> packet);
    void onVideoFrameReady(std::shared_ptr<AVData> frame);

    // void onVideoFrameReady_(AVFrame* frame);
public slots:
    void onResReady(const SourceInfo &sf);
    void onPlayRequest(const QString id, VideoPlayerMode mode);
private:
    std::atomic<int64_t> m_total_samples_played{0};
    std::atomic<double> m_audio_base_pts_seconds{0.0};
    std::atomic<bool> m_audio_base_set{false};
    std::atomic<int> m_audio_sample_rate{0};
    std::atomic<int> m_audio_channels{0};
    std::atomic<int> m_audio_bytes_per_sample{2};
    std::atomic<int64_t> m_samples_before_first_audio{0};
    std::atomic<bool> m_first_audio_received{false};

    // 辅助变量：控制时钟计算频率（减少回调耗时）
    std::atomic<int> m_callback_counter{0};
    // 辅助变量：控制初始化日志只打印一次
    std::atomic<bool> m_audio_init_log_printed{false};
};


#endif // BASEPLAYERFRAME_H