#include "../Header/Clock/Clock.h"
#include <QDebug>
#include <QThread>
#include <QCoreApplication>

Clock::Clock(QObject *parent) : QObject(parent)
{
    m_syncTimer = new QTimer(this);
    m_syncTimer->setTimerType(Qt::PreciseTimer);
    m_syncTimer->setInterval(50); // 20Hz同步检查
    
    connect(this, &Clock::syncRequested, this, &Clock::processSyncRequest, Qt::QueuedConnection);
    connect(m_syncTimer, &QTimer::timeout, this, &Clock::updateFromSync);
}

Clock::~Clock()
{
    // 停止所有活动
    stopSync();

    QMutexLocker locker(&m_mutex);
    m_syncTimer->stop();
    disconnect();
}

// ==================== 核心同步控制 ====================
void Clock::start(double startTime)
{
    QMutexLocker locker(&m_mutex);

    // 如果正在同步，不启动自己的计时器
    if (!m_masterClock.isNull()) {
        qWarning() << "[Clock] Cannot start when synced to master clock";
        return;
    }

    m_baseTime = startTime;
    m_paused = false;
    m_pauseAccumulated = 0.0;
    m_timer.start();

    // 启动同步定时器（如果是主时钟则用于内部时间推进）
    updateSyncTimer();

    // 解锁后再发射信号
    locker.unlock();
    emit timeChanged(getTime());
    emit timeChangedMs(getTimeMs());
}

void Clock::pause()
{
    QMutexLocker locker(&m_mutex);

    if (m_paused) return;

    m_paused = true;
    m_lastPauseTime = calculateCurrentTime();

    updateSyncTimer();

    locker.unlock();
    emit pausedChanged(true);
}

void Clock::resume()
{
    QMutexLocker locker(&m_mutex);

    if (!m_paused) return;

    m_paused = false;
    m_pauseAccumulated += (calculateCurrentTime() - m_lastPauseTime);
    m_timer.restart();

    updateSyncTimer();

    locker.unlock();
    emit pausedChanged(false);
}

void Clock::stop()
{
    QMutexLocker locker(&m_mutex);

    bool wasSynced = !m_masterClock.isNull();
    bool wasPaused = m_paused;

    m_paused = true;
    m_baseTime = 0.0;
    m_pauseAccumulated = 0.0;

    updateSyncTimer();

    locker.unlock();

    // 在锁外发射信号，避免可能的循环
    if (!wasPaused) {
        emit pausedChanged(true);
    }
    emit timeChanged(0.0);
    emit timeChangedMs(0);

    if (wasSynced) {
        emit syncStateChanged(false);
    }
}

void Clock::setTime(double time)
{
    QMutexLocker locker(&m_mutex);

    if (!m_masterClock.isNull()) {
        // 如果正在同步，计算新的偏移量
        double masterTime = m_masterClock->getTime();
        m_timeOffset = time - masterTime;

        m_baseTime = time - m_pauseAccumulated;
        m_timer.restart();

        qDebug() << "[Clock] Time offset updated to:" << m_timeOffset * 1000 << "ms";
    } else {
        m_baseTime = time - m_pauseAccumulated;
        m_timer.restart();
    }

    locker.unlock();

    // 使用防重入保护发射信号
    if (!isEmittingTimeChanged()) {
        setEmittingTimeChanged(true);
        emit timeChanged(getTime());
        emit timeChangedMs(getTimeMs());
        setEmittingTimeChanged(false);
    }
}

// ==================== 时钟同步功能 ====================
void Clock::syncToMaster(Clock* masterClock)
{
    if (!masterClock) {
        stopSync();
        return;
    }

    // 防止递归调用
    if (m_processingSyncRequest) {
        qWarning() << "[Clock] Sync request already in progress, skipping";
        return;
    }

    m_processingSyncRequest = true;
    emit syncRequested(masterClock);
}

void Clock::processSyncRequest(Clock* masterClock)
{
    m_processingSyncRequest = false;

    QMutexLocker locker(&m_mutex);

    if (m_masterClock == masterClock) {
        return;
    }

    qDebug() << "[Clock] Processing sync request in thread:" << QThread::currentThreadId();

    // 断开旧的主时钟连接
    if (!m_masterClock.isNull()) {
        disconnect(m_masterClock, &Clock::timeChanged, this, &Clock::onMasterTimeChanged);
        disconnect(m_masterClock, &Clock::pausedChanged, this, &Clock::onMasterPausedChanged);
        disconnect(m_masterClock, &Clock::destroyed, this, &Clock::onMasterDestroyed);
    }

    m_masterClock = masterClock;
    m_syncActive = !m_masterClock.isNull();

    if (!m_masterClock.isNull()) {
        // 连接到主时钟信号（使用QueuedConnection确保跨线程安全）
        connect(masterClock, &Clock::timeChanged,
                this, &Clock::onMasterTimeChanged, Qt::QueuedConnection);
        connect(masterClock, &Clock::pausedChanged,
                this, &Clock::onMasterPausedChanged, Qt::QueuedConnection);
        connect(masterClock, &Clock::destroyed,
                this, &Clock::onMasterDestroyed, Qt::QueuedConnection);

        // 初始化同步
        m_lastSyncTime = masterClock->getTime();
        double currentTime = m_lastSyncTime + m_timeOffset;

        m_baseTime = currentTime - m_pauseAccumulated;
        m_timer.restart();

        // 同步暂停状态
        m_paused = masterClock->isPaused();

        updateSyncTimer();

        qDebug() << "[Clock] Started sync to master clock, initial time:" << currentTime;
    } else {
        m_syncTimer->stop();
        qDebug() << "[Clock] Stopped sync";
    }

    locker.unlock();
    emit syncStateChanged(m_syncActive);
}

void Clock::stopSync()
{
    // 清除处理标志，允许新的同步请求
    m_processingSyncRequest = false;
    emit syncRequested(nullptr);
}

void Clock::onMasterTimeChanged(double time)
{
    QMutexLocker locker(&m_mutex);

    if (m_masterClock.isNull() || m_paused) return;

    m_lastSyncTime = time;
    double syncTime = time + m_timeOffset;

    m_baseTime = syncTime - m_pauseAccumulated;
    m_timer.restart();

    locker.unlock();

    // 防重入保护
    if (!isEmittingTimeChanged()) {
        setEmittingTimeChanged(true);
        emit timeChanged(syncTime);
        emit timeChangedMs(static_cast<qint64>(syncTime * 1000));
        setEmittingTimeChanged(false);
    }
}

void Clock::onMasterPausedChanged(bool paused)
{
    QMutexLocker locker(&m_mutex);

    if (m_masterClock.isNull()) return;

    if (m_paused != paused) {
        m_paused = paused;

        if (paused) {
            m_lastPauseTime = calculateCurrentTime();
        } else {
            m_pauseAccumulated += (calculateCurrentTime() - m_lastPauseTime);
            m_timer.restart();
        }

        updateSyncTimer();

        locker.unlock();
        emit pausedChanged(paused);
    }
}

void Clock::onMasterDestroyed()
{
    qDebug() << "[Clock] Master clock destroyed, stopping sync";
    stopSync();
}

void Clock::updateFromSync()
{
    QMutexLocker locker(&m_mutex);

    if (m_masterClock.isNull() || m_paused) return;

    double masterTime = m_masterClock->getTime();
    double currentTime = calculateCurrentTime();
    double expectedTime = masterTime + m_timeOffset;
    double drift = expectedTime - currentTime;

    // 如果漂移超过阈值（10ms），强制同步
    if (qAbs(drift) > 0.01) {
        m_lastSyncTime = masterTime;
        m_baseTime = expectedTime - m_pauseAccumulated;
        m_timer.restart();

        qDebug() << "[Clock] Drift detected:" << drift * 1000 << "ms, correcting...";

        locker.unlock();

        // 防重入保护
        if (!isEmittingTimeChanged()) {
            setEmittingTimeChanged(true);
            emit timeChanged(expectedTime);
            emit timeChangedMs(static_cast<qint64>(expectedTime * 1000));
            setEmittingTimeChanged(false);
        }
    }
}

// ==================== 辅助函数 ====================
void Clock::updateSyncTimer()
{
    if (m_syncActive && !m_paused && !m_masterClock.isNull()) {
        if (!m_syncTimer->isActive()) {
            m_syncTimer->start();
        }
    } else {
        if (m_syncTimer->isActive()) {
            m_syncTimer->stop();
        }
    }
}

double Clock::getTime() const
{
    QMutexLocker locker(&m_mutex);

    if (!m_masterClock.isNull() && !m_paused) {
        return m_masterClock->getTime() + m_timeOffset;
    }

    return calculateCurrentTime();
}

qint64 Clock::getTimeMs() const
{
    return static_cast<qint64>(getTime() * 1000);
}

bool Clock::isPaused() const
{
    QMutexLocker locker(&m_mutex);
    return m_paused;
}

double Clock::calculateCurrentTime() const
{
    if (m_paused) {
        return m_lastPauseTime;
    }

    return m_baseTime + (m_timer.elapsed() / 1000.0) - m_pauseAccumulated;
}

void Clock::setTimeMs(qint64 ms)
{
    setTime(ms / 1000.0);
}

void Clock::startFromMs(qint64 ms)
{
    start(ms / 1000.0);
}

void Clock::setTimeOffset(double offset)
{
    QMutexLocker locker(&m_mutex);
    m_timeOffset = offset;

    if (!m_masterClock.isNull()) {
        double masterTime = m_masterClock->getTime();
        locker.unlock();
        setTime(masterTime + offset);
    }
}

bool Clock::isEmittingTimeChanged() const
{
    return m_emittingTimeChanged.loadAcquire() > 0;
}

void Clock::setEmittingTimeChanged(bool emitting)
{
    m_emittingTimeChanged.storeRelease(emitting ? 1 : 0);
}