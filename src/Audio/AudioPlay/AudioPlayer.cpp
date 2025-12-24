#include "../../../Header/AudioPart/AudioPlay/AudioPlayer.h"
#include <QDebug>
#include <QFileInfo>

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent),
      m_workThread(new QThread()),
      m_shouldStop(false),
      m_isPaused(false),
      m_state(PlayState::Stopped),
      m_sdlDevId(0),
      m_totalDurationMs(0),
      m_frameCounter(0),
      m_seekPositionMs(0),
      m_hasSeekRequest(false),
      m_isResourceInitialized(false),
      m_consecutiveDecodeFailures(0),
      m_clock(new Clock(this))
{
    qDebug() << "[AudioPlayer] 构造函数被调用";
    qDebug() << "[AudioPlayer] 主线程ID:" << QThread::currentThreadId();
    qDebug() << "[AudioPlayer] 工作线程对象地址:" << m_workThread;

    connect(m_workThread, &QThread::finished, m_workThread, &QObject::deleteLater);
    qDebug() << "[AudioPlayer] 调用initAndStartThread前";
    initAndStartThread();
    qDebug() << "[AudioPlayer] 调用initAndStartThread后";
}

AudioPlayer::~AudioPlayer()
{
    stop();
    if (m_workThread->isRunning()) {
        m_workThread->quit();
        m_workThread->wait(1000);
    }
    cleanupResources();
}

// === 进度信号发射 ===
void AudioPlayer::emitProgressSignals()
{
    checkAndSyncClock();

    if (m_state != PlayState::Playing && m_state != PlayState::Paused) {
        return;
    }

    // 从时钟获取当前时间
    double currentTime = m_clock->getTime();
    int64_t currentMs = static_cast<int64_t>(currentTime * 1000);

    // 发射位置信号
    emit positionChanged(currentMs);

    // 发射进度百分比信号
    if (m_totalDurationMs > 0) {
        float progress = qMin((float)currentMs / m_totalDurationMs, 1.0f);
        emit progressUpdated(progress, currentMs, m_totalDurationMs);

        // 检查播放完成
        if (currentMs >= m_totalDurationMs) {
            m_progressTimer->stop();
            emit playbackFinished();
            emit currentSongFinished();
        }
    }
}

// === 时钟同步 ===
void AudioPlayer::syncClockWithAudioPTS(const AudioFrame &frame)
{
    if (frame.pts == AV_NOPTS_VALUE) return;

    AVRational timeBase = m_decoder.get_audio_time_base();
    double audioPts = frame.pts * av_q2d(timeBase);

    if (audioPts > 0) {
        double currentClockTime = m_clock->getTime();
        double diff = std::abs(audioPts - currentClockTime);

        // 如果差异超过阈值，进行时钟同步
        if (diff > 0.05) { // 50ms阈值
            m_clock->setTime(audioPts);
        }
    }
}

// === 播放控制方法 ===
bool AudioPlayer::play(const QString &filePath)
{
    qDebug() << "\n=== AudioPlayer::play开始 ===";
    qDebug() << "[play] 文件路径:" << filePath;
    qDebug() << "[play] 调用线程ID:" << QThread::currentThreadId();
    qDebug() << "[play] AudioPlayer对象所在线程:" << this->thread();
    qDebug() << "[play] 工作线程状态 - 运行中:" << m_workThread->isRunning();
    qDebug() << "[play] 工作线程状态 - 已完成:" << m_workThread->isFinished();

    // 如果已经在播放同一文件且状态是暂停，则恢复播放
    if (m_currentFilePath == filePath && m_state == PlayState::Paused) {
        qDebug() << "[AudioPlayer] 恢复播放当前文件";
        resume();
        return true;
    }

    // 切换到新文件
    if (!switchToNewFile(filePath)) {
        qDebug() << "[AudioPlayer] 切换到新文件失败";
        return false;
    }

    resetPlaybackState();

    m_state = PlayState::Playing;
    emit stateChanged(PlayState::Playing);

    qDebug() << "AudioPlayer::play被调用，线程ID:" << QThread::currentThreadId();

    bool success = QMetaObject::invokeMethod(this,"runPlayLoop",Qt::QueuedConnection,Q_ARG(QString,filePath));
    qDebug() << "已执行QMetaObject::invokeMethod" << "结果:" << success;

    if (!success) {
        qDebug() << "[play] invokeMethod失败! 可能原因:";
        qDebug() << "  1. runPlayLoop不是槽函数?";
        qDebug() << "  2. 参数类型不匹配?";
        qDebug() << "  3. AudioPlayer对象没有移动到工作线程?";
        qDebug() << "  当前对象线程:" << this->thread();
        qDebug() << "  工作线程:" << m_workThread;
    }

    return success;
}

void AudioPlayer::pause()
{
    qDebug() << "[AudioPlayer] 尝试暂停播放";

    if (m_state != PlayState::Playing) {
        qDebug() << "[AudioPlayer] 暂停失败：当前状态不是播放状态";
        qDebug() << "CurrentState::" << static_cast<int>(m_state);
        return;
    }

    if (m_sdlDevId == 0) {
        qDebug() << "[AudioPlayer] 暂停失败：SDL设备未初始化";
        return;
    }

    SDL_PauseAudioDevice(m_sdlDevId, 1);
    m_state = PlayState::Paused;
    m_isPaused = true;

    qDebug() << "[AudioPlayer] 播放已暂停";
    //m_clock->pause();
    //m_progressTimer->stop();

    emit stateChanged(m_state);
}

void AudioPlayer::stop()
{
    if (m_state == PlayState::Stopped) return;

    qDebug() << "[AudioPlayer] 停止播放";
    m_shouldStop = true;
    m_state = PlayState::Stopped;

    //m_clock->stop();
    //m_progressTimer->stop();

    emit stateChanged(PlayState::Stopped);
}

void AudioPlayer::resume()
{
    qDebug() << "[AudioPlayer] 尝试恢复播放";

    if (m_state != PlayState::Paused) {
        qDebug() << "[AudioPlayer] 恢复失败：当前状态不是暂停状态";
        return;
    }

    if (m_sdlDevId == 0) {
        qDebug() << "[AudioPlayer] 恢复失败：SDL设备未初始化";
        m_state = PlayState::Error;
        emit stateChanged(m_state);
        return;
    }

    SDL_PauseAudioDevice(m_sdlDevId, 0);
    m_state = PlayState::Playing;
    m_isPaused = false;

    qDebug() << "[AudioPlayer] 播放已恢复";
    //m_clock->resume();
    //m_progressTimer->start();

    emit stateChanged(m_state);
}

void AudioPlayer::seek(int64_t targetMs)
{
    if (m_state == PlayState::Stopped) return;

    m_seekPositionMs = targetMs;
    m_hasSeekRequest = true;

    // 立即更新时钟
    m_clock->setTime(targetMs / 1000.0);

    // 立即发射进度信号
    emitProgressSignals();

    qDebug() << "[Seek]:" << targetMs;
}

// === 播放循环 ===
void AudioPlayer::runPlayLoop(const QString &filePath)
{
    qDebug() << "进入runPlayloop";

    // 在工作线程中创建定时器（确保只创建一次）
    if (!m_progressTimer) {
        m_progressTimer = new QTimer(this);
        m_progressTimer->setInterval(50);
        m_progressTimer->setTimerType(Qt::PreciseTimer);

        // 使用DirectConnection，因为定时器和槽在同一线程
        connect(m_progressTimer, &QTimer::timeout,
                this, &AudioPlayer::emitProgressSignals,
                Qt::DirectConnection);

        qDebug() << "[runPlayLoop] 定时器在工作线程创建，线程ID:"
                 << QThread::currentThreadId();
    }


    qDebug() << "[runPlayLoop] 已初始化Timer";

    if (!m_isResourceInitialized || m_currentFilePath != filePath) {
        qDebug() << "[runPlayLoop] 资源未正确初始化，退出播放循环";
        m_state = PlayState::Error;
        emit stateChanged(m_state);
        return;
    }

    resetPlaybackState();
    normalCompletion = false;

    SDL_PauseAudioDevice(m_sdlDevId, 0);
    m_state = PlayState::Playing;
    emit stateChanged(m_state);

    // 在这里启动时钟和定时器（在工作线程中）
    m_clock->start(0);
    if (!m_progressTimer->isActive()) {
        m_progressTimer->start();
        qDebug() << "[runPlayLoop] 定时器已启动";
    }

    qDebug() << "[runPlayLoop] SDL启动播放，开始解码循环";

    AudioFrame frame{};
    const size_t bytesPerSample = 2;
    const size_t maxQueueSize = (size_t)2048 * m_decoder.get_target_Channels() * bytesPerSample * 10;

    try {
        while (!m_shouldStop && !m_decoder.is_eof()) {
            // 处理跳转请求
            if (m_hasSeekRequest) {
                handleSeekRequest();
                m_hasSeekRequest = false;
                m_consecutiveDecodeFailures = 0;
                m_clock->setTime(m_seekPositionMs / 1000.0);
                emitProgressSignals();
            }

            emitProgressSignals();

            // 处理暂停
            if (m_isPaused) {
                if (m_state != PlayState::Paused) {
                    m_state = PlayState::Paused;
                    emit stateChanged(m_state);
                }
                QThread::msleep(100);
                continue;
            }

            if (m_state != PlayState::Playing) {
                m_state = PlayState::Playing;
                emit stateChanged(m_state);
            }

            // 解码一帧
            bool decoded = false;
            for (int retry = 0; retry < 3; ++retry) {
                if (m_decoder.decode_frame(frame)) {
                    decoded = true;
                    m_consecutiveDecodeFailures = 0;
                    break;
                }

                if (m_decoder.is_eof()) {
                    qDebug() << "[runPlayLoop] 解码器报告EOF，停止重试";
                    break;
                }
                QThread::msleep(10);
            }

            if (!decoded) {
                m_consecutiveDecodeFailures++;

                if (m_consecutiveDecodeFailures >= MAX_CONSECUTIVE_FAILURES) {
                    qDebug() << "[runPlayLoop] 连续解码失败超过阈值，退出播放循环";
                    emit errorOccurred("连续解码失败，播放终止");
                    break;
                }

                if (m_decoder.is_eof()) {
                    qDebug() << "[runPlayLoop] 解码失败且解码器EOF，退出播放循环";
                    break;
                }

                qDebug() << "[runPlayLoop] 解码失败，跳过当前帧，连续失败次数:"
                         << m_consecutiveDecodeFailures;
                continue;
            }

            // 校验帧有效性
            if (!frame.data || !frame.data[0] || frame.nb_samples <= 0) {
                qWarning() << "[runPlayLoop] 无效帧，跳过";
                m_decoder.release_Frame(frame);
                continue;
            }

            // 计算数据大小
            size_t dataSize = (size_t)frame.nb_samples * frame.channels * bytesPerSample;
            if (dataSize <= 0 || (frame.linesize > 0 && dataSize > (size_t)frame.linesize)) {
                qWarning() << "[runPlayLoop] 无效数据大小，跳过";
                m_decoder.release_Frame(frame);
                continue;
            }

            // 控制队列大小
            while (SDL_GetQueuedAudioSize(m_sdlDevId) > maxQueueSize) {
                QThread::msleep(5);
            }

            // 推流到SDL
            int queueResult = SDL_QueueAudio(m_sdlDevId, frame.data[0], dataSize);
            if (queueResult < 0) {
                qWarning() << "[runPlayLoop] SDL推流失败：" << SDL_GetError();
            } else {
                m_frameCounter++;

                // 时钟同步
                if (m_frameCounter % 10 == 0) {
                    syncClockWithAudioPTS(frame);
                }

                if (m_frameCounter % 50 == 0) {
                    qDebug() << "[runPlayLoop] 推流成功：帧序号=" << m_frameCounter;
                }
            }

            m_decoder.release_Frame(frame);
        }

        // 播放完成处理
        if (!m_shouldStop && m_decoder.is_eof()) {
            normalCompletion = true;
            qDebug() << "[runPlayLoop] 正常播放完成，发出完成信号";
            emit playbackFinished();
            emit currentSongFinished();
            m_clock->stop();
            m_progressTimer->stop();
        } else if (m_shouldStop) {
            qDebug() << "[runPlayLoop] 用户主动停止播放";
        } else if (m_consecutiveDecodeFailures >= MAX_CONSECUTIVE_FAILURES) {
            qDebug() << "[runPlayLoop] 因连续解码失败退出";
        } else {
            qWarning() << "[runPlayLoop] 异常结束播放";
            emit errorOccurred("播放异常结束");
        }

        // 等待剩余数据播放
        qDebug() << "[runPlayLoop] 等待剩余音频播放...";
        const int MAX_WAIT_MS = 1000;
        int waitedMs = 0;
        const int CHECK_INTERVAL_MS = 50;

        Uint32 queueSize = 0;
        while ((queueSize = SDL_GetQueuedAudioSize(m_sdlDevId)) > 0 &&
               !m_shouldStop && waitedMs < MAX_WAIT_MS) {
            QThread::msleep(CHECK_INTERVAL_MS);
            waitedMs += CHECK_INTERVAL_MS;
        }

        if (waitedMs >= MAX_WAIT_MS) {
            qWarning() << "[runPlayLoop] 等待音频播放超时，强制退出";
        } else {
            qDebug() << "[runPlayLoop] 音频播放完成";
        }

    } catch (const std::exception& e) {
        qCritical() << "[runPlayLoop] 发生异常:" << e.what();
    } catch (...) {
        qCritical() << "[runPlayLoop] 发生未知异常";
    }

    // 最终清理
    qDebug() << "[runPlayLoop] 开始清理播放资源...";

    QMutexLocker locker(&m_playbackMutex);

    if (m_sdlDevId != 0) {
        SDL_PauseAudioDevice(m_sdlDevId, 1);
        SDL_ClearQueuedAudio(m_sdlDevId);
    }

    m_clock->stop();
    m_progressTimer->stop();

    if (m_decoder.is_initialized()) {
        m_decoder.close();
        qDebug() << "[runPlayLoop] 解码器已关闭";
    }

    releaseSDL();

    m_state = PlayState::Stopped;
    m_isResourceInitialized = false;

    qDebug() << "[runPlayLoop] 播放循环结束";
    emit stateChanged(m_state);
}

// === 资源管理方法 ===
bool AudioPlayer::switchToNewFile(const QString& filePath)
{
    qDebug() << "[AudioPlayer] 切换到新文件:" << filePath;

    if (m_currentFilePath == filePath && m_isResourceInitialized) {
        qDebug() << "[AudioPlayer] 重新初始化当前文件";

        if (m_decoder.is_initialized()) {
            m_decoder.close();
        }

        return initializePlayback(filePath);
    }

    if (m_state == PlayState::Playing || m_state == PlayState::Paused) {
        qDebug() << "[AudioPlayer] 检测到正在播放，立即停止当前播放";
        immediateStopAndCleanup();
    }

    return initializePlayback(filePath);
}

bool AudioPlayer::initializePlayback(const QString& filePath)
{
    qDebug() << "[AudioPlayer] 初始化播放资源:" << filePath;

    QMutexLocker locker(&m_playbackMutex);

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qDebug() << "[AudioPlayer] 文件不存在:" << filePath;
        return false;
    }

    if (!m_decoder.open(filePath) || !m_decoder.is_initialized()) {
        qDebug() << "[AudioPlayer] 解码器打开失败:" << filePath;
        return false;
    }

    AudioMeta meta = m_decoder.get_meta();
    m_totalDurationMs = meta.durationMs > 0 ? meta.durationMs : 60 * 1000;

    int targetSampleRate = m_decoder.get_target_SampleRate();
    int targetChannels = m_decoder.get_target_Channels();

    if (targetSampleRate <= 0 || targetChannels <= 0 || targetChannels > 8) {
        qDebug() << "[AudioPlayer] 无效音频参数:" << targetSampleRate << targetChannels;
        m_decoder.close();
        return false;
    }

    if (!initSDL(targetSampleRate, targetChannels)) {
        qDebug() << "[AudioPlayer] SDL初始化失败";
        m_decoder.close();
        return false;
    }

    m_currentFilePath = filePath;
    m_isResourceInitialized = true;

    qDebug() << "[AudioPlayer] 播放资源初始化成功，时长:" << m_totalDurationMs << "ms";
    emit durationChanged(m_totalDurationMs);
    return true;
}

void AudioPlayer::immediateStopAndCleanup()
{
    qDebug() << "[AudioPlayer] 立即停止并清理资源";

    QMutexLocker locker(&m_playbackMutex);

    m_shouldStop = true;

    if (m_sdlDevId != 0) {
        SDL_PauseAudioDevice(m_sdlDevId, 1);
        SDL_ClearQueuedAudio(m_sdlDevId);
        qDebug() << "[AudioPlayer] SDL音频已立即暂停并清空队列";
    }

    if (m_decoder.is_initialized()) {
        m_decoder.close();
        qDebug() << "[AudioPlayer] 解码器已立即关闭";
    }

    releaseSDL();

    m_state = PlayState::Stopped;
    m_isResourceInitialized = false;
    m_hasSeekRequest = false;
    m_seekPositionMs = 0;
    m_totalDurationMs = 0;
    m_frameCounter = 0;
    m_consecutiveDecodeFailures = 0;
    normalCompletion = false;

    qDebug() << "[AudioPlayer] 资源立即清理完成";
}

void AudioPlayer::handleSeekRequest()
{
    if (!m_isResourceInitialized) return;

    QMutexLocker locker(&m_playbackMutex);

    qDebug() << "[AudioPlayer] 处理跳转请求:" << m_seekPositionMs << "ms";
    bool seekSuccess = m_decoder.seek(m_seekPositionMs);
    if (!seekSuccess) {
        qDebug() << "[AudioPlayer] 跳转失败";
        emit errorOccurred("跳转失败");
    } else {
        qDebug() << "[AudioPlayer] 跳转成功";
    }
}

void AudioPlayer::resetPlaybackState()
{
    qDebug() << "[AudioPlayer] 重置播放状态";
    m_shouldStop = false;
    m_isPaused = false;
    m_consecutiveDecodeFailures = 0;
    m_hasSeekRequest = false;
    m_seekPositionMs = 0;
    m_frameCounter = 0;
}

void AudioPlayer::cleanupResources()
{
    qDebug() << "[AudioPlayer] 清理音频资源...";

    QMutexLocker locker(&m_playbackMutex);

    m_previousFilePath = m_currentFilePath;
    m_shouldStop = true;

    if (m_decoder.is_initialized()) {
        m_decoder.close();
        qDebug() << "[AudioPlayer] 解码器已关闭";
    }

    releaseSDL();

    m_isResourceInitialized = false;
    m_hasSeekRequest = false;
    m_seekPositionMs = 0;
    m_totalDurationMs = 0;
    m_frameCounter = 0;
    m_consecutiveDecodeFailures = 0;

    qDebug() << "[AudioPlayer] 资源清理完成，上一个文件:" << m_previousFilePath;
}

// === SDL音频设备方法 ===
bool AudioPlayer::initSDL(int sampleRate, int channels)
{
    releaseSDL();

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        qDebug() << "[initSDL] SDL初始化失败:" << SDL_GetError();
        return false;
    }

    SDL_AudioSpec desiredSpec{};
    desiredSpec.freq = sampleRate;
    desiredSpec.channels = channels;
    desiredSpec.format = AUDIO_S16SYS;
    desiredSpec.samples = 2048;
    desiredSpec.callback = nullptr;

    m_sdlDevId = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, nullptr, 0);
    if (m_sdlDevId == 0) {
        qDebug() << "[initSDL] SDL打开设备失败:" << SDL_GetError();
        SDL_Quit();
        return false;
    }
    qDebug() << "[initSDL] SDL设备打开成功（ID:" << m_sdlDevId << "）";

    return true;
}

void AudioPlayer::releaseSDL()
{
    if (m_sdlDevId != 0) {
        SDL_PauseAudioDevice(m_sdlDevId, 1);
        SDL_ClearQueuedAudio(m_sdlDevId);
        SDL_CloseAudioDevice(m_sdlDevId);
        m_sdlDevId = 0;
        qDebug() << "[releaseSDL] SDL设备已关闭";
    }

    if (SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) {
        SDL_Quit();
        qDebug() << "[releaseSDL] SDL子系统已退出";
    }
}

void AudioPlayer::checkAndSyncClock()
{
    if (m_state == PlayState::Playing && m_clock->isPaused()) {
        qDebug() << "[时钟同步] 音频播放中但时钟暂停，自动恢复时钟";
        m_clock->resume();
    } else if (m_state == PlayState::Paused && !m_clock->isPaused()) {
        qDebug() << "[时钟同步] 音频暂停中但时钟运行，自动暂停时钟";
        m_clock->pause();
    }
}