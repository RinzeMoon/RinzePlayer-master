#ifndef RINZEPLAYER_AUDIOPLAYER_H
#define RINZEPLAYER_AUDIOPLAYER_H

#include <QObject>
#include <QThread>
#include <QString>
#include <atomic>
#include "SDL2/SDL.h"

#include <../Header/AudioPart/Decode/AudioDecoder.h>
#include <../Header/AudioPart/FFmpegAudioProcessor.h>

#include <Global.h>
#include <QMutex>
#include <QTimer>
#include <QCoreApplication>

#include "Clock/Clock.h"

using RinGlobal::PlayState;
using AudioUtil::AudioMeta;
using AudioUtil::AudioFrame;

class AudioPlayer : public QObject
{
    Q_OBJECT
    Q_ENUMS(PlayState)
public:
    static AudioPlayer* getInstance()
    {
        static AudioPlayer instance;
        return &instance;
    }

    void resetDecoderState();

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    ~AudioPlayer() override;

    PlayState currentState() const { return m_state; }

    void initAndStartThread()
    {
        qDebug() << "[initAndStartThread] 开始初始化";
        qDebug() << "[initAndStartThread] 当前线程ID:" << QThread::currentThreadId();

        qDebug() << "[initAndStartThread] 工作线程是否运行:" << m_workThread->isRunning();
        qDebug() << "[initAndStartThread] 工作线程是否完成:" << m_workThread->isFinished();

        this->moveToThread(m_workThread);
        m_workThread->start();

        qDebug() << "[initAndStartThread] 线程启动后状态 - 运行中:" << m_workThread->isRunning();
        qDebug() << "[initAndStartThread] 工作线程ID:" << m_workThread->currentThreadId();
    }

    Clock* getClock() const { return m_clock; }
    int64_t getCurrentPositionMs() const { return m_clock->getTimeMs(); }
    int64_t getTotalDurationMs() const { return m_totalDurationMs; }

    int preDecodeAfterSeek(int numFrames);
    void checkSDLStatus();

signals:
    void stateChanged(PlayState state);
    void positionChanged(qint64 position);
    void progressUpdated(float progress, int64_t currentMs, int64_t totalMs);
    void errorOccurred(const QString &errorMsg);
    void durationChanged(quint64 duration);
    void playbackFinished();
    void currentSongFinished();

    void internalSeekRequested(qint64 positionMs);

public slots:
    bool play(const QString &filePath);
    void pause();
    void stop();
    void seek(int64_t targetMs);
    void resume();

    void handleInternalSeek(qint64 positionMs);

private slots:
    void runPlayLoop(const QString &filePath);
    void emitProgressSignals();

private:
    explicit AudioPlayer(QObject *parent = nullptr);

    // 音频设备方法
    bool initSDL(int sampleRate, int channels);
    void releaseSDL();

    // 资源管理方法
    void cleanupResources();
    bool initializePlayback(const QString& filePath);
    bool switchToNewFile(const QString& filePath);
    void handleSeekRequest();
    void resetPlaybackState();
    void immediateStopAndCleanup();

    void checkAndSyncClock();

    // 时钟同步方法
    void syncClockWithAudioPTS(const AudioFrame &frame);

private:
    // 多线程相关
    QThread *m_workThread;
    std::atomic<bool> m_shouldStop;
    std::atomic<bool> m_isPaused;

    // 状态与资源
    PlayState m_state;
    SDL_AudioDeviceID m_sdlDevId;
    SDL_AudioSpec m_sdlSpec;

    // 依赖的现有模块
    AudioDecoder m_decoder;
    FFmpegAudioProcessor m_ffmpegProcessor;

    // 音频元数据
    int64_t m_totalDurationMs;
    int m_frameCounter;

    // 递归锁解决死锁问题
    QRecursiveMutex m_playbackMutex;

    qint64 m_seekPositionMs;
    bool m_hasSeekRequest;
    QString m_currentFilePath;

    bool normalCompletion = false;
    bool m_isResourceInitialized = false;
    QString m_previousFilePath;

    // 解码失败保护
    int m_consecutiveDecodeFailures = 0;
    const int MAX_CONSECUTIVE_FAILURES = 5;

    // 时钟系统
    Clock* m_clock;
    QTimer* m_progressTimer;

    // 一些锁 & 标志
    // 原子操作的状态标志
    std::atomic<bool> m_seekPending{false};
    std::atomic<qint64> m_seekTargetMs{0};
    std::atomic<int64_t> m_pausedPositionMs{0};
    // 保护解码器操作的互斥锁（仅用于seek和解码器访问）
    QMutex m_decoderMutex;

    QElapsedTimer m_lastSeekTime;  // 记录上次跳转时间
    const int MIN_SEEK_INTERVAL_MS = 200;  // 最小跳转间隔200ms
};

#endif //RINZEPLAYER_AUDIOPLAYER_H