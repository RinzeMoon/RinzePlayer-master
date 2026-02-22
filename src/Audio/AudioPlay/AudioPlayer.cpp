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
      m_consecutiveDecodeFailures(0)
      //m_clock(new Clock())
{
    qDebug() << "[AudioPlayer] 构造函数被调用";
    qDebug() << "[AudioPlayer] 主线程ID:" << QThread::currentThreadId();
    qDebug() << "[AudioPlayer] 工作线程对象地址:" << m_workThread;

    connect(m_workThread, &QThread::finished, m_workThread, &QObject::deleteLater);
    qDebug() << "[AudioPlayer] 调用initAndStartThread前";
    initAndStartThread();
    qDebug() << "[AudioPlayer] 调用initAndStartThread后";

    connect(m_workThread, &QThread::started, this, [this]()
    {
        qDebug() << "[initInWorkerThread] 在工作线程中初始化，线程ID:" << QThread::currentThreadId();

        // 在工作线程中创建时钟和定时器
        m_clock = new Clock();
        m_progressTimer = new QTimer();

        // 设置定时器
        m_progressTimer->setInterval(100);
        m_progressTimer->setTimerType(Qt::PreciseTimer);

        // 连接信号槽（现在都在工作线程）
        connect(m_progressTimer, &QTimer::timeout,
                this, &AudioPlayer::emitProgressSignals,
                Qt::DirectConnection);

        qDebug() << "[initInWorkerThread] 时钟和定时器在工作线程创建完成";
    });

    QMetaObject::invokeMethod(this, [this]() {
        connect(m_progressTimer, &QTimer::timeout,
                this, &AudioPlayer::emitProgressSignals,
                Qt::DirectConnection);
    }, Qt::QueuedConnection);
}

AudioPlayer::~AudioPlayer()
{
    // 停止播放
    stop();

    // 等待工作线程退出
    if (m_workThread && m_workThread->isRunning()) {
        m_workThread->quit();
        m_workThread->wait(1000);
    }

    // 安全删除时钟和定时器
    if (m_progressTimer) {
        m_progressTimer->deleteLater();
        m_progressTimer = nullptr;
    }

    if (m_clock) {
        m_clock->deleteLater();
        m_clock = nullptr;
    }

    // 清理其他资源
    cleanupResources();
}

// === 进度信号发射 ===
void AudioPlayer::emitProgressSignals()
{
    if (m_state == PlayState::Stopped) {
        return;
    }

    // 频率限制：每100ms最多发射一次信号
    static QElapsedTimer lastEmitTimer;
    if (lastEmitTimer.isValid() && lastEmitTimer.elapsed() < 100) {
        return;
    }
    lastEmitTimer.start();

    // 如果处于暂停状态，使用记录的位置
    if (m_state == PlayState::Paused) {
        if (m_pausedPositionMs > 0 && m_totalDurationMs > 0) {
            float progress = qMin((float)m_pausedPositionMs / m_totalDurationMs, 1.0f);
            emit progressUpdated(progress, m_pausedPositionMs, m_totalDurationMs);
        }
        return;
    }

    // 检查时钟是否有效
    if (!m_clock) {
        return;
    }

    // 获取当前时间
    double currentTime = m_clock->getTime();

    // 关键修改：计算SDL缓冲对应的时间，扣除后得到实际已播放时间
    double bufferTime = 0.0;
    if (m_sdlDevId != 0) {
        Uint32 queuedBytes = SDL_GetQueuedAudioSize(m_sdlDevId);
        if (queuedBytes > 0) {
            int bytesPerSample = 2 * m_decoder.get_target_Channels();
            bufferTime = (double)queuedBytes / (bytesPerSample * m_decoder.get_target_sample_rate());
        }
    }
    // 实际已播放时间 = Clock时间 - 缓冲时间
    double actualPlayedTime = currentTime - bufferTime;
    if (actualPlayedTime < 0) actualPlayedTime = 0;

    // 重要：确保时间不会超过总时长
    if (m_totalDurationMs > 0) {
        double maxTime = m_totalDurationMs / 1000.0;
        if (actualPlayedTime > maxTime) {
            actualPlayedTime = maxTime;
        }
    }

    int64_t currentMs = static_cast<int64_t>(actualPlayedTime * 1000);

    // 发射位置信号（使用实际已播放时间）
    emit positionChanged(currentMs);

    // 发射进度百分比信号
    if (m_totalDurationMs > 0) {
        float progress = qMin((float)currentMs / m_totalDurationMs, 1.0f);
        emit progressUpdated(progress, currentMs, m_totalDurationMs);

        // 检查播放完成
        if (currentMs >= m_totalDurationMs - 100) { // 留100ms余量
            if (m_progressTimer && m_progressTimer->isActive()) {
                m_progressTimer->stop();
            }
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
    if (timeBase.num == 0 || timeBase.den == 0) {
        return;
    }
    double audioPts = frame.pts * av_q2d(timeBase);

    // 关键修改1：计算SDL队列中未播放的音频对应的时间（扣除缓冲时间）
    double bufferTime = 0.0;
    if (m_sdlDevId != 0) {
        Uint32 queuedBytes = SDL_GetQueuedAudioSize(m_sdlDevId);
        if (queuedBytes > 0) {
            int bytesPerSample = 2 * m_decoder.get_target_Channels();
            bufferTime = (double)queuedBytes / (bytesPerSample * m_decoder.get_target_sample_rate());
        }
    }
    // 实际已播放的音频时间 = 当前帧PTS - 队列缓冲时间
    double actualPlayedTime = audioPts - bufferTime;

    double currentClockTime =  m_clock->getTime();
    double diff = actualPlayedTime - currentClockTime;

    static bool firstSync = true;
    static double firstAudioPts = 0.0;

    if (firstSync && audioPts > 0) {
        firstAudioPts = audioPts;
        firstSync = false;
        qDebug() << "[时钟同步] 首次同步，音频PTS:" << audioPts
                 << "实际播放时间:" << actualPlayedTime
                 << "时钟时间:" << currentClockTime
                 << "缓冲时间:" << bufferTime << "s";
        // 首次同步直接对齐实际播放时间
        m_clock->setTime(actualPlayedTime);
        return;
    }

    if (qAbs(diff) > 0.05) { // 50ms 阈值
        qDebug() << "[时钟同步] 差异:" << diff * 1000 << "ms"
                 << "音频PTS:" << audioPts << "s"
                 << "缓冲时间:" << bufferTime << "s"
                 << "实际播放时间:" << actualPlayedTime << "s"
                 << "Clock当前时间:" << currentClockTime << "s";

        m_clock->setTime(actualPlayedTime);
        qDebug() << "[时钟同步] 调整Clock到:" << actualPlayedTime << "s";
    }
}

bool AudioPlayer::play(const QString &filePath)
{
    qDebug() << "\n=== AudioPlayer::play开始 ===";
    qDebug() << "[play] 文件路径:" << filePath;

    // 如果已经在播放同一文件且状态是暂停，则恢复播放
    if (m_currentFilePath == filePath && m_state == PlayState::Paused) {
        qDebug() << "[AudioPlayer] 恢复播放当前文件";

        // 使用专门的恢复函数
        QMetaObject::invokeMethod(this, "resumeFromPaused", Qt::QueuedConnection);
        return true;
    }

    // 如果是停止状态，需要重新初始化
    if (m_state == PlayState::Stopped) {
        // 重置停止标志
        m_shouldStop = false;
        m_isPaused = false;
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

    // 检查是否在工作线程调用
    if (QThread::currentThread() != this->thread()) {
        qDebug() << "[AudioPlayer] 警告：pause() 在工作线程外调用，使用异步调用";
        QMetaObject::invokeMethod(this, "pause", Qt::QueuedConnection);
        return;
    }

    if (m_state != PlayState::Playing) {
        qDebug() << "[AudioPlayer] 暂停失败：当前状态不是播放状态";
        return;
    }

    // 1. 设置暂停标志（让 runPlayLoop 退出）
    m_isPaused = true;
    m_shouldStop = true;  // 添加这行，让 runPlayLoop 退出

    // 2. 暂停SDL音频
    if (m_sdlDevId != 0) {
        SDL_PauseAudioDevice(m_sdlDevId, 1);
        qDebug() << "[AudioPlayer] SDL音频已暂停";
    }

    // 3. 暂停时钟并记录位置
    if (m_clock) {
        // 先获取当前时间
        double currentTime = m_clock->getTime();
        m_pausedPositionMs = static_cast<int64_t>(currentTime * 1000);

        // 验证时间有效性
        if (m_pausedPositionMs < 0) {
            qWarning() << "[AudioPlayer::pause] 获取到负数时间:" << m_pausedPositionMs
                       << "ms，修正为0";
            m_pausedPositionMs = 0;
        }

        // 暂停时钟
        m_clock->pause();

        qDebug() << "[AudioPlayer] 时钟时间:" << currentTime << "s"
                 << "->" << m_pausedPositionMs << "ms，时钟已暂停";
    }

    // 4. 更新状态
    m_state = PlayState::Paused;

    // 5. 停止进度定时器
    if (m_progressTimer && m_progressTimer->isActive()) {
        m_progressTimer->stop();
        qDebug() << "[AudioPlayer] 进度定时器已停止";
    }

    // 6. 等待 runPlayLoop 退出
    QThread::msleep(50);  // 等待一小段时间

    // 7. 重置停止标志，为可能的恢复做准备
    m_shouldStop = false;

    // 8. 发送一次进度信号
    emitProgressSignals();

    qDebug() << "[AudioPlayer] 播放已暂停";
    emit stateChanged(m_state);
}

void AudioPlayer::stop()
{
    if (m_state == PlayState::Stopped) return;

    if (QThread::currentThread() != this->thread()) {
        qDebug() << "[AudioPlayer] stop() 在工作线程外调用，使用异步调用";
        QMetaObject::invokeMethod(this, "stop", Qt::QueuedConnection);
        return;
    }

    qDebug() << "[AudioPlayer] 停止播放";

    // 1. 设置停止标志
    m_shouldStop = true;

    // 2. 清除暂停标志
    m_isPaused = false;

    // 3. 等待一小段时间让runPlayLoop退出
    QThread::msleep(100);

    // 4. 更新状态
    m_state = PlayState::Stopped;

    // 5. 确保SDL已关闭
    if (m_sdlDevId != 0) {
        SDL_PauseAudioDevice(m_sdlDevId, 1);
    }

    // 6. 停止时钟和定时器
    if (m_clock) {
        m_clock->stop();
    }

    if (m_progressTimer && m_progressTimer->isActive()) {
        m_progressTimer->stop();
    }

    // 7. 清理资源
    cleanupResources();

    qDebug() << "[AudioPlayer] 播放已停止";
    emit stateChanged(PlayState::Stopped);
}

void AudioPlayer::resume()
{
    qDebug() << "[AudioPlayer] 尝试恢复播放";

    // 检查是否在工作线程调用
    if (QThread::currentThread() != this->thread()) {
        qDebug() << "[AudioPlayer] 警告：resume() 在工作线程外调用，使用异步调用";
        QMetaObject::invokeMethod(this, "resume", Qt::QueuedConnection);
        return;
    }

    if (m_state != PlayState::Paused) {
        qDebug() << "[AudioPlayer] 恢复失败：当前状态不是暂停状态，实际状态:" << (int)m_state;
        return;
    }

    // 验证暂停位置
    if (m_pausedPositionMs < 0) {
        qWarning() << "[AudioPlayer] 恢复时暂停位置为负数:" << m_pausedPositionMs << "ms，修正为0";
        m_pausedPositionMs = 0;
    }

    qDebug() << "[AudioPlayer] 从暂停位置恢复:" << m_pausedPositionMs << "ms";

    // 1. 清除暂停标志
    m_isPaused = false;

    // 2. 如果暂停位置超过总时长，调整到最大位置
    if (m_totalDurationMs > 0 && m_pausedPositionMs > m_totalDurationMs) {
        qWarning() << "[AudioPlayer] 暂停位置超过总时长，调整为总时长";
        m_pausedPositionMs = m_totalDurationMs;
    }

    // 3. 设置时钟时间并恢复时钟
    if (m_clock) {
        // 关键修复：恢复时钟，而不是重新启动
        m_clock->setTime(m_pausedPositionMs / 1000.0);
        m_clock->resume();  // 恢复时钟运行
        qDebug() << "[AudioPlayer] 时钟已恢复，设置时间:" << (m_pausedPositionMs / 1000.0) << "s";
    }

    // 4. 重置暂停位置
    int64_t resumePosition = m_pausedPositionMs;
    m_pausedPositionMs = 0;

    // 5. 更新状态
    m_state = PlayState::Playing;

    // 6. 启动进度定时器
    if (m_progressTimer && !m_progressTimer->isActive()) {
        m_progressTimer->start();
        qDebug() << "[AudioPlayer] 进度定时器已启动";
    }

    // 7. 恢复SDL音频
    if (m_sdlDevId != 0) {
        SDL_PauseAudioDevice(m_sdlDevId, 0);
        qDebug() << "[AudioPlayer] SDL音频已恢复";
    }

    // 8. 关键修复：不要再调用 runPlayLoop，只需要恢复解码循环继续运行
    // 恢复播放状态，让解码循环继续
    m_shouldStop = false;  // 清除停止标志

    // 如果解码器还在运行，直接恢复即可
    // 如果解码器已停止，需要重新开始解码循环
    if (m_decoder.is_initialized() && !m_decoder.is_eof()) {
        // 解码器还在有效状态，直接恢复
        qDebug() << "[AudioPlayer] 解码器状态有效，恢复播放";
    } else {
        // 解码器可能已停止，需要重新启动解码循环
        qDebug() << "[AudioPlayer] 解码器需要重新启动，重新开始解码循环";
        bool success = QMetaObject::invokeMethod(this, "runPlayLoop",
                              Qt::QueuedConnection,
                              Q_ARG(QString, m_currentFilePath));

        if (!success) {
            qDebug() << "[AudioPlayer] 恢复播放失败，无法启动解码循环";
            m_state = PlayState::Paused;
            return;
        }
    }

    qDebug() << "[AudioPlayer] 播放已恢复";
    emit stateChanged(m_state);
}

void AudioPlayer::seek(int64_t targetMs)
{
    if (m_state == PlayState::Stopped) return;

    // 跳转频率限制（200ms最小间隔）
    if (m_lastSeekTime.isValid() && m_lastSeekTime.elapsed() < 200) {
        qDebug() << "[Seek] 跳转过于频繁，忽略本次请求";
        return;
    }

    m_lastSeekTime.start();

    qDebug() << "[Seek] 目标位置:" << targetMs;

    // 如果当前是暂停状态，更新暂停位置
    if (m_state == PlayState::Paused) {
        m_pausedPositionMs = targetMs;
        qDebug() << "[Seek] 暂停状态下更新暂停位置:" << m_pausedPositionMs;
    }

    // 原子操作设置跳转目标
    m_seekTargetMs.store(targetMs, std::memory_order_release);
    m_seekPending.store(true, std::memory_order_release);

    // 安全地更新时钟
    if (m_clock) {
        m_clock->setTimeMs(targetMs);
    }

    // 发送跳转请求到工作线程
    bool invokeResult = QMetaObject::invokeMethod(this, "handleInternalSeek",
                              Qt::QueuedConnection,
                              Q_ARG(qint64, targetMs));

    // 立即发送一次进度信号（使用目标位置）
    if (m_totalDurationMs > 0) {
        float progress = qMin((float)targetMs / m_totalDurationMs, 1.0f);
        emit progressUpdated(progress, targetMs, m_totalDurationMs);
    }

    qDebug() << "[Seek] 异步跳转请求已发出，调用结果:" << invokeResult;
}

void AudioPlayer::runPlayLoop(const QString &filePath)
{
    qDebug() << "进入runPlayloop，线程ID:" << QThread::currentThreadId();

    // 检查是否应该直接退出（比如暂停状态）
    if (m_isPaused) {
        qDebug() << "[runPlayLoop] 检测到暂停状态，直接退出";
        return;
    }

    // 如果是从暂停恢复，需要重新定位解码器
    if (m_pausedPositionMs > 0) {
        qDebug() << "[runPlayLoop] 从暂停恢复，跳转到位置:" << m_pausedPositionMs << "ms";

        // 执行解码器跳转
        {
            QMutexLocker locker(&m_decoderMutex);
            bool seekSuccess = m_decoder.seek(m_pausedPositionMs);
            qDebug() << "[runPlayLoop] 恢复跳转结果:" << (seekSuccess ? "成功" : "失败");

            // 如果跳转失败，重新打开解码器
            if (!seekSuccess) {
                qDebug() << "[runPlayLoop] 跳转失败，尝试重新打开解码器";
                m_decoder.close();
                m_decoder.open(filePath);
                m_decoder.seek(m_pausedPositionMs);
            }
        }

        // 预解码几帧
        if (m_sdlDevId != 0) {
            preDecodeAfterSeek(3);
        }
    }

    // 启动时钟 - 在SDL开始播放前启动
    double initialTime = m_pausedPositionMs / 1000.0;
    if (m_clock) {
        m_clock->start(initialTime);
        qDebug() << "[runPlayLoop] 时钟启动，初始时间:" << initialTime << "秒";
    }

    // 启动进度定时器
    if (!m_progressTimer->isActive()) {
        m_progressTimer->start();
        qDebug() << "[runPlayLoop] 进度定时器已启动";
    }

    // 检查资源是否初始化
    if (!m_isResourceInitialized) {
        qDebug() << "[runPlayLoop] 资源未正确初始化，退出播放循环";
        m_state = PlayState::Error;
        emit stateChanged(m_state);
        return;
    }

    // 更新状态
    m_state = PlayState::Playing;
    emit stateChanged(m_state);

    // 重置播放状态
    m_shouldStop = false;
    m_consecutiveDecodeFailures = 0;
    m_frameCounter = 0;

    // 重置暂停位置（在恢复播放后）
    m_pausedPositionMs = 0;

    // 关键修复：等待一小段时间确保时钟开始运行
    QThread::msleep(10);

    // 获取时钟启动后的初始时间用于校准
    if (m_clock) {
        double actualStartTime = m_clock->getTime();
        qDebug() << "[runPlayLoop] 时钟实际开始时间:" << actualStartTime << "秒";
    }

    // 启动SDL播放
    SDL_PauseAudioDevice(m_sdlDevId, 0);
    qDebug() << "[runPlayLoop] SDL启动播放，开始解码循环";

    AudioFrame frame{};
    const size_t bytesPerSample = 2;
    const size_t targetQueueDurationMs = 500;
    const size_t maxQueueSize = (size_t)((targetQueueDurationMs / 1000.0) *
                                         m_decoder.get_target_sample_rate() *
                                         m_decoder.get_target_Channels() *
                                         bytesPerSample);

    try {
        while (!m_shouldStop && !m_decoder.is_eof()) {
            // 处理事件
            static int eventCounter = 0;
            if (++eventCounter % 20 == 0) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                eventCounter = 0;
            }

            // 检查暂停标志 - 如果是暂停，则等待
            if (m_isPaused) {
                // 暂停状态下，只处理事件，不解码
                QThread::msleep(100);
                continue;
            }

            // 检查跳转标志
            if (m_seekPending.load(std::memory_order_acquire)) {
                QThread::msleep(10);
                continue;
            }

            // 解码一帧
            bool decoded = false;
            int maxRetries = (m_consecutiveDecodeFailures > 0) ? 1 : 3;
            for (int retry = 0; retry < maxRetries; ++retry) {
                if (m_seekPending.load(std::memory_order_acquire)) {
                    break; // 跳转来了，停止重试
                }

                if (m_decoder.decode_frame(frame)) {
                    decoded = true;
                    m_consecutiveDecodeFailures = 0;
                    break;
                }
                if (m_decoder.is_eof()) break;
                QThread::msleep(2);
            }

            if (!decoded || m_seekPending.load(std::memory_order_acquire)) {
                m_consecutiveDecodeFailures++;

                if (decoded) m_decoder.release_Frame(frame);

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

            // 控制队列大小（可被跳转中断）
            while (SDL_GetQueuedAudioSize(m_sdlDevId) > maxQueueSize) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                if (m_seekPending.load(std::memory_order_acquire)) {
                    break; // 跳转来了，立即退出等待
                }
                QThread::msleep(2);
            }

            // 再次检查跳转标志
            if (m_seekPending.load(std::memory_order_acquire)) {
                m_decoder.release_Frame(frame);
                continue;
            }

            // 推流到SDL
            int queueResult = SDL_QueueAudio(m_sdlDevId, frame.data[0], dataSize);
            if (queueResult < 0) {
                qWarning() << "[runPlayLoop] SDL推流失败：" << SDL_GetError();
            } else
            {
                m_frameCounter++;

                if (m_frameCounter % 4 == 0)
                {
                    qDebug() << "[runPlayLoop] 推流成功，帧计数:" << m_frameCounter << "，执行同步";
                    syncClockWithAudioPTS(frame);
                }
            }

            m_decoder.release_Frame(frame);
        }

        // 播放完成处理 - 只有在真正停止时才清理
        if (m_shouldStop) {
            qDebug() << "[runPlayLoop] 用户主动停止播放";
        } else if (!m_isPaused && m_decoder.is_eof()) { // 不是暂停状态下的自然结束
            normalCompletion = true;
            qDebug() << "[runPlayLoop] 正常播放完成，发出完成信号";
            // 关键：先停Clock和进度定时器，再发射完成信号
            if (m_clock) {
                m_clock->stop();
                qDebug() << "[runPlayLoop] 播放结束，停止时钟";
            }
            if (m_progressTimer && m_progressTimer->isActive()) {
                m_progressTimer->stop();
                qDebug() << "[runPlayLoop] 播放结束，停止进度定时器";
            }

            // 最后发射完成信号
            emit playbackFinished();
            emit currentSongFinished();
        } else if (m_consecutiveDecodeFailures >= MAX_CONSECUTIVE_FAILURES) {
            qDebug() << "[runPlayLoop] 因连续解码失败退出";
        } else if (m_isPaused) {
            qDebug() << "[runPlayLoop] 暂停状态，保持资源不清理";
            // 暂停状态下不清理，直接返回
            return;
        } else {
            qWarning() << "[runPlayLoop] 异常结束播放";
            emit errorOccurred("播放异常结束");
        }

        // 只有在真正停止时才清理资源
        if (m_shouldStop) {
            qDebug() << "[runPlayLoop] 开始清理播放资源...";

            QMutexLocker locker(&m_playbackMutex);

            if (m_sdlDevId != 0) {
                SDL_PauseAudioDevice(m_sdlDevId, 1);
                SDL_ClearQueuedAudio(m_sdlDevId);
            }

            m_clock->stop();
            if (m_progressTimer && m_progressTimer->isActive()) {
                m_progressTimer->stop();
            }

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
    } catch (const std::exception& e) {
        qCritical() << "[runPlayLoop] 发生异常:" << e.what();
    } catch (...) {
        qCritical() << "[runPlayLoop] 发生未知异常";
    }
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
    m_pausedPositionMs = 0;  // 重置暂停位置
    m_consecutiveDecodeFailures = 0;
    m_seekPositionMs = 0;
    m_frameCounter = 0;

    // 重置原子标志
    if (m_seekPending.is_lock_free()) {
        m_seekPending.store(false, std::memory_order_release);
    }
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

void AudioPlayer::handleInternalSeek(qint64 positionMs)
{
    qDebug() << ">>>>>>>> [handleInternalSeek] 开始执行，位置:" << positionMs;

    // 记录当前播放状态
    bool wasPlaying = (m_state == PlayState::Playing);

    // 暂停SDL音频（如果正在播放）
    if (m_sdlDevId != 0 && wasPlaying) {
        SDL_PauseAudioDevice(m_sdlDevId, 1);
        qDebug() << "[handleInternalSeek] SDL已暂停";
    }

    // 清空队列
    if (m_sdlDevId != 0) {
        SDL_ClearQueuedAudio(m_sdlDevId);
        qDebug() << "[handleInternalSeek] SDL队列已清空";
    }

    // 执行解码器跳转
    bool seekSuccess = false;
    {
        QMutexLocker locker(&m_decoderMutex);
        seekSuccess = m_decoder.seek(positionMs);
        qDebug() << "[handleInternalSeek] 解码器跳转结果:" << (seekSuccess ? "成功" : "失败");
    }

    // 重置播放状态
    m_consecutiveDecodeFailures = 0;
    m_frameCounter = 0;

    // 预解码2帧（避免过多）
    if (m_sdlDevId != 0 && seekSuccess) {
        preDecodeAfterSeek(2);
    }

    // 更新时钟（使用安全调用）
    if (m_clock) {
        m_clock->setTime(positionMs / 1000.0);
        qDebug() << "[handleInternalSeek] 时钟时间已设置为:" << (positionMs / 1000.0) << "s";
    }

    // 重置跳转标志
    m_seekPending.store(false, std::memory_order_release);

    // 重新启动SDL播放（如果之前是播放状态）
    if (m_sdlDevId != 0 && wasPlaying) {
        SDL_PauseAudioDevice(m_sdlDevId, 0);
        qDebug() << "[handleInternalSeek] SDL重新启动播放";
    }

    // 发送一次进度更新
    QTimer::singleShot(50, this, [this]() {
        emitProgressSignals();
    });

    qDebug() << "<<<<<<<< [handleInternalSeek] 执行完成";
}

// 可选的预解码函数
int AudioPlayer::preDecodeAfterSeek(int frameCount)
{
    qDebug() << "[预解码] 开始预解码" << frameCount << "帧";

    AudioFrame frame{};
    int decodedCount = 0;

    for(int i = 0; i < frameCount; ++i) {
        if(m_decoder.decode_frame(frame)) {
            size_t dataSize = (size_t)frame.nb_samples * frame.channels * 2;

            // 检查SDL设备是否有效
            if (m_sdlDevId != 0) {
                SDL_QueueAudio(m_sdlDevId, frame.data[0], dataSize);
            }

            m_decoder.release_Frame(frame);
            decodedCount++;
        } else {
            break;
        }
    }

    qDebug() << "[预解码] 完成，成功解码" << decodedCount << "帧";
    return decodedCount;
}

void AudioPlayer::checkSDLStatus()
{
    if (m_sdlDevId == 0) {
        qDebug() << "[SDL状态] 设备未初始化";
        return;
    }

    SDL_AudioStatus status = SDL_GetAudioDeviceStatus(m_sdlDevId);
    Uint32 queueSize = SDL_GetQueuedAudioSize(m_sdlDevId);

    qDebug() << "[SDL状态] 设备状态:"
             << (status == SDL_AUDIO_PLAYING ? "播放中" :
                 status == SDL_AUDIO_PAUSED ? "暂停" :
                 status == SDL_AUDIO_STOPPED ? "停止" : "未知")
             << "，队列大小:" << queueSize << "字节";
}

void AudioPlayer::resetDecoderState()
{
    QMutexLocker locker(&m_decoderMutex);

    // 如果连续跳转超过5次，重置解码器状态
    static int seekCount = 0;
    seekCount++;

    if (seekCount >= 5) {
        qDebug() << "[resetDecoderState] 多次跳转后重置解码器状态";

        // 暂时关闭并重新打开解码器
        if (m_decoder.is_initialized()) {
            QString currentFile = m_currentFilePath;
            m_decoder.close();
            QThread::msleep(10);
            m_decoder.open(currentFile);
        }

        seekCount = 0;
    }
}

