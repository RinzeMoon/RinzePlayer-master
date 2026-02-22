#include "../../Header/Clock/Clock.h"
#include <QDebug>
#include <QThread>
#include <QCoreApplication>
#include <QtMath>

Clock::Clock(QObject *parent) : QObject(parent)
{

    qDebug() << "[Clock] 构造函数，线程:" << QThread::currentThreadId();
    m_baseTime = 0.0;
    m_lastPauseTime = 0.0;
    m_paused = true;
    m_timer.invalidate();
    m_lastEmitSecond = -1.0;

    // 复用 syncTimer 同时负责走时和漂移检测
    m_syncTimer = new QTimer(this);
    m_syncTimer->setTimerType(Qt::PreciseTimer);
    m_syncTimer->setInterval(50); // 50ms 一次
    // 合并槽函数：先更新走时，再检测漂移
    connect(m_syncTimer, &QTimer::timeout, this, [this]() {
        onInternalTimerTimeout(); // 原有走时逻辑
        updateFromSync();         // 原有漂移检测逻辑
    });

    connect(this, &Clock::syncRequested, this, &Clock::processSyncRequest, Qt::QueuedConnection);
}

Clock::~Clock()
{
    qDebug() << "[Clock] 析构函数";
    stopSync();


    if (m_syncTimer) {
        m_syncTimer->stop();
        m_syncTimer->deleteLater();
    }
}



// ==================== 核心时间控制（兼容原有调用，仅新增内部定时器启停） ====================
void Clock::start(double startTime)
{
    QMutexLocker locker(&m_mutex);

    qDebug() << "[Clock::start] 启动，初始时间:" << startTime << "s";

    // 确保时间不为负
    if (startTime < 0) {
        startTime = 0.0;
    }

    m_baseTime = startTime;
    m_paused = false;
    m_lastPauseTime = 0.0;
    m_lastEmitSecond = -1.0; // 重置频率控制

    // 关键：在设置时间后立即启动计时器
    m_timer.restart();

    // 启动同步定时器
    updateSyncTimer();

    locker.unlock();

    // 立即发射信号
    emit timeChanged(getTime());
    emit timeChangedMs(getTimeMs());
    emit pausedChanged(false);
}

void Clock::pause()
{
    QMutexLocker locker(&m_mutex);

    if (m_paused) {
        qDebug() << "[Clock::pause] 已经是暂停状态";
        return;
    }

    // 1. 计算当前时间
    double currentTime = calculateCurrentTime();

    // 2. 记录暂停时间
    m_lastPauseTime = currentTime;
    m_paused = true;

    // 3. 使计时器无效
    m_timer.invalidate();

    // 4. 停止同步定时器
    updateSyncTimer();

    qDebug() << "[Clock::pause] 暂停，时间:" << currentTime << "s"
             << "，基准时间:" << m_baseTime << "s";

    locker.unlock();

    // 发射信号
    emit pausedChanged(true);
}

void Clock::resume()
{
    QMutexLocker locker(&m_mutex);

    if (!m_paused) {
        qDebug() << "[Clock::resume] 已经是运行状态";
        return;
    }

    qDebug() << "[Clock::resume] 恢复，最后暂停时间:" << m_lastPauseTime << "s";

    // 1. 清除暂停状态
    m_paused = false;

    // 2. 如果最后暂停时间有效，设置为基准时间
    if (m_lastPauseTime > 0) {
        m_baseTime = m_lastPauseTime;
    }

    // 3. 清除最后暂停时间
    m_lastPauseTime = 0.0;
    m_lastEmitSecond = -1.0; // 重置频率控制

    // 4. 重启计时器
    m_timer.restart();

    // 5. 启动同步定时器
    updateSyncTimer();

    locker.unlock();

    // 发射信号
    emit pausedChanged(false);
    emit timeChanged(getTime());
    emit timeChangedMs(getTimeMs());
}

void Clock::stop() {
    QMutexLocker locker(&m_mutex);
    qDebug() << "[Clock::stop] 停止";

    // 1. 停止定时器
    if (m_syncTimer && m_syncTimer->isActive()) {
        m_syncTimer->stop();
    }

    // 2. 重置状态变量，但不要清空主时钟！
    m_baseTime = 0.0;
    m_lastPauseTime = 0.0;
    m_paused = true;
    m_lastEmitSecond = -1.0;
    m_timer.invalidate();
    // 重要：不要清空 m_masterClock 和 m_syncActive！
    // m_masterClock.clear(); // 注释掉这行
    // m_syncActive = false;  // 注释掉这行
    m_timeOffset = 0.0;    // 重置时间偏移

    locker.unlock();
    emit timeChanged(0.0);
    emit timeChangedMs(0);
}

void Clock::setTime(double time)
{
    // 1. 简化锁逻辑，确保能修改m_baseTime
    QMutexLocker locker(&m_mutex);
    if (time < 0) time = 0;

    // 2. 强制更新基准时间（去掉所有条件判断）
    qDebug() << "[Clock::setTime] 旧基准时间:" << m_baseTime << "s → 新基准时间:" << time << "s";
    m_baseTime = time; // 核心：确保这行执行
    m_lastEmitSecond = -1.0;

    // 3. 暂停/运行状态都更新对应时间
    if (m_paused) {
        m_lastPauseTime = time;
    } else {
        // 重启计时器，让后续elapsed时间基于新基准时间累加
        m_timer.restart();
    }

    // 4. 立即发射信号，同步UI
    locker.unlock();
    emit timeChanged(getTime());
    emit timeChangedMs(getTimeMs());
}

// ==================== 时间获取（保留原有逻辑，仅优化调试输出） ====================
double Clock::getTime() const
{
    QMutexLocker locker(&m_mutex);

    double time = 0.0;

    // 同步状态下直接从主时钟获取（线程安全）
    if (!m_masterClock.isNull() && !m_paused) {
        // 重要：这里调用的是主时钟的 getTime()，它是线程安全的
        time = m_masterClock->getTime() + m_timeOffset;
        qDebug() << "[Clock::getTime] 同步状态，从主时钟获取时间：" << time;
    } else {
        time = calculateCurrentTime();
    }

    return time;
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

// ==================== 内部计算（核心修复：确保运行时时间持续累加） ====================
double Clock::calculateCurrentTime() const
{
    if (m_paused) {
        return m_lastPauseTime;
    }
    if (!m_timer.isValid()) {
        return m_baseTime;
    }

    qint64 elapsedMs = m_timer.elapsed();
    double elapsedSec = static_cast<double>(elapsedMs) / 1000.0;
    double currentTime = m_baseTime + elapsedSec;

    // 新增日志：输出基础计时信息（每2秒输出一次，避免刷屏）
    static QElapsedTimer debugTimer;
    static double lastDebugTime = 0.0;
    if (!debugTimer.isValid()) {
        debugTimer.start();
    }
    if (debugTimer.elapsed() > 2000) {
        qDebug() << "[Clock::calculateCurrentTime] 基准时间:" << m_baseTime
                 << "，已流逝:" << elapsedSec << "s，当前时间:" << currentTime << "s";
        lastDebugTime = currentTime;
        debugTimer.restart();
    }

    return qMax(0.0, currentTime);
}

// ==================== 同步功能（完全保留原有逻辑，无改动） ====================
void Clock::syncToMaster(Clock* masterClock)
{
    qDebug() << "[Clock::syncToMaster] 开始同步，当前时钟：" << this
             << "，主时钟：" << masterClock
             << "，线程：" << QThread::currentThreadId();

    if (!masterClock) {
        stopSync();
        qDebug() << "[Clock::syncToMaster] 主时钟为空，停止同步";
        return;
    }

    // 直接发射信号
    qDebug() << "[Clock::syncToMaster] 发射 syncRequested 信号";
    emit syncRequested(masterClock);
}

void Clock::processSyncRequest(Clock* masterClock)
{
    qDebug() << "[Clock::processSyncRequest] === 开始处理同步请求 ===";
    qDebug() << "  当前线程：" << QThread::currentThreadId();
    qDebug() << "  当前时钟地址：" << this;
    qDebug() << "  主时钟地址：" << masterClock;

    QMutexLocker locker(&m_mutex);

    // 如果已经是同一个主时钟，不需要重复设置
    if (m_masterClock == masterClock) {
        qDebug() << "[Clock::processSyncRequest] 已经是同一个主时钟，跳过";
        return;
    }

    // 断开旧的主时钟所有连接
    if (!m_masterClock.isNull()) {
        qDebug() << "[Clock::processSyncRequest] 断开旧主时钟的所有连接";
        disconnect(m_masterClock, nullptr, this, nullptr);
    }

    m_masterClock = masterClock;
    m_syncActive = !m_masterClock.isNull();

    if (!m_masterClock.isNull()) {
        qDebug() << "[Clock::processSyncRequest] 开始连接主时钟信号...";

        // 使用新式语法连接信号槽
        bool timeConnected = connect(masterClock, &Clock::timeChanged,
                                     this, &Clock::onMasterTimeChanged, Qt::AutoConnection);
        bool pausedConnected = connect(masterClock, &Clock::pausedChanged,
                                       this, &Clock::onMasterPausedChanged, Qt::AutoConnection);
        bool destroyedConnected = connect(masterClock, &Clock::destroyed,
                                          this, &Clock::onMasterDestroyed, Qt::AutoConnection);

        qDebug() << "[Clock::processSyncRequest] 连接结果：";
        qDebug() << "  timeChanged: " << timeConnected;
        qDebug() << "  pausedChanged: " << pausedConnected;
        qDebug() << "  destroyed: " << destroyedConnected;

        // 初始化同步：立即获取一次主时钟时间
        double masterTime = masterClock->getTime();
        qDebug() << "[Clock::processSyncRequest] 获取主时钟时间：" << masterTime;

        m_lastSyncTime = masterTime;
        m_baseTime = masterTime + m_timeOffset;

        // 重置计时器
        if (!m_paused) {
            m_timer.restart();
        }

        qDebug() << "[Clock::processSyncRequest] 同步设置完成：";
        qDebug() << "  主时间：" << masterTime;
        qDebug() << "  偏移：" << m_timeOffset;
        qDebug() << "  基准时间：" << m_baseTime;
    } else {
        qDebug() << "[Clock::processSyncRequest] 停止同步";
    }

    updateSyncTimer();

    locker.unlock();

    // 发射同步状态信号
    emit syncStateChanged(m_syncActive);

    // 立即更新时间信号（确保UI立即响应）
    if (!m_masterClock.isNull()) {
        double currentTime = m_baseTime;
        qDebug() << "[Clock::processSyncRequest] 发射初始时间信号：" << currentTime;
        emit timeChanged(currentTime);
        emit timeChangedMs(static_cast<qint64>(currentTime * 1000));
    }

    qDebug() << "[Clock::processSyncRequest] === 处理完成 ===";
}

void Clock::stopSync()
{
    processSyncRequest(nullptr);
}

void Clock::onMasterTimeChanged(double time)
{
    // 先获取线程信息
    qDebug() << "[Clock::onMasterTimeChanged] 收到时间更新，线程："
             << QThread::currentThreadId();

    QMutexLocker locker(&m_mutex);

    // 检查主时钟是否有效
    if (m_masterClock.isNull()) {
        qDebug() << "[Clock::onMasterTimeChanged] 主时钟指针为空，忽略更新";
        return;
    }

    if (m_paused) {
        qDebug() << "[Clock::onMasterTimeChanged] 从时钟已暂停，忽略更新";
        return;
    }

    qDebug() << "[Clock::onMasterTimeChanged] 主时钟时间：" << time
             << "，偏移：" << m_timeOffset;

    m_lastSyncTime = time;
    double syncTime = time + m_timeOffset;

    // 更新基准时间
    m_baseTime = syncTime;

    // 重置计时器
    if (!m_paused) {
        m_timer.restart();
    }

    locker.unlock();

    // 发射信号
    if (!isEmittingTimeChanged()) {
        setEmittingTimeChanged(true);
        qDebug() << "[Clock::onMasterTimeChanged] 发射同步时间：" << syncTime;
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
            // 同步主时钟的暂停时间
            m_lastPauseTime = m_masterClock->getTime() + m_timeOffset;
            m_timer.invalidate();
        } else {
            // 清除最后暂停时间，重启计时器
            m_lastPauseTime = 0.0;
            m_timer.restart();
        }

        updateSyncTimer();

        locker.unlock();
        emit pausedChanged(paused);
    }
}

void Clock::onMasterDestroyed()
{
    qDebug() << "[Clock] 主时钟已销毁，停止同步";
    stopSync();
}

void Clock::updateFromSync() {
    QMutexLocker locker(&m_mutex);
    if (m_masterClock.isNull() || m_paused) return;

    double masterTime = m_masterClock->getTime();
    double currentTime = calculateCurrentTime();
    double expectedTime = masterTime + m_timeOffset;
    double drift = expectedTime - currentTime;

    // 阈值从 0.02（20ms）改为 0.1（100ms）
    if (qAbs(drift) > 0.1) {
        m_lastSyncTime = masterTime;
        m_baseTime = expectedTime;
        m_timer.restart();
        qDebug() << "[Clock] 检测到漂移:" << drift * 1000 << "ms，已修正";
        locker.unlock();
        if (!isEmittingTimeChanged()) {
            setEmittingTimeChanged(true);
            emit timeChanged(expectedTime);
            emit timeChangedMs(static_cast<qint64>(expectedTime * 1000));
            setEmittingTimeChanged(false);
        }
    }
}

// ==================== 辅助函数（保留原有） ====================
void Clock::updateSyncTimer()
{
    if (m_syncActive && !m_paused && !m_masterClock.isNull()) {
        if (!m_syncTimer->isActive()) {
            qDebug() << "[Clock::updateSyncTimer] 启动同步定时器";
            m_syncTimer->start();
        }
    } else {
        if (m_syncTimer->isActive()) {
            qDebug() << "[Clock::updateSyncTimer] 停止同步定时器";
            m_syncTimer->stop();
        }
    }
}

void Clock::setTimeMs(qint64 ms)
{
    setTime(static_cast<double>(ms) / 1000.0);
}

void Clock::startFromMs(qint64 ms)
{
    start(static_cast<double>(ms) / 1000.0);
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

void Clock::onInternalTimerTimeout()
{
    QMutexLocker locker(&m_mutex);

    // 暂停状态不处理
    if (m_paused) return;

    double currentTime;

    if (!m_masterClock.isNull()) {
        // 同步状态下：从主时钟获取时间
        double masterTime = m_masterClock->getTime();
        currentTime = masterTime + m_timeOffset;

        qDebug() << "[Clock::onInternalTimerTimeout] 同步状态更新："
                 << "主时钟时间：" << masterTime
                 << "，当前时间：" << currentTime;

        // 更新基准时间，保持内部计时器同步
        m_baseTime = currentTime;
        m_timer.restart();
    } else {
        // 独立运行状态：使用内部计时器
        currentTime = calculateCurrentTime();
    }

    locker.unlock();

    // 总是发射时间更新信号，确保UI持续刷新
    if (!isEmittingTimeChanged()) {
        setEmittingTimeChanged(true);
        // 添加频率控制，避免过于频繁的日志
        static int emitCount = 0;
        if (++emitCount % 20 == 0) { // 每20次（约1秒）输出一次
            qDebug() << "[Clock::onInternalTimerTimeout] 发射时间更新信号：" << currentTime;
            emitCount = 0;
        }
        emit timeChanged(currentTime);
        emit timeChangedMs(static_cast<qint64>(currentTime * 1000));
        setEmittingTimeChanged(false);
    }
}