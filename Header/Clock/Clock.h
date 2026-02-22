#ifndef RINZEPLAYER_CLOCK_H
#define RINZEPLAYER_CLOCK_H

#include <QObject>
#include <QElapsedTimer>
#include <QMutex>
#include <QPointer>
#include <QTimer>
#include <QAtomicInteger>

class Clock : public QObject
{
    Q_OBJECT

public:
    explicit Clock(QObject *parent = nullptr);
    ~Clock() override;

    // ==================== 核心同步控制（完全兼容原有调用） ====================
    void start(double startTime = 0);
    void pause();
    void resume();
    void stop();
    void setTime(double time);

    // ==================== 时钟同步功能（兼容原有同步逻辑） ====================
    Q_INVOKABLE void syncToMaster(Clock* masterClock);
    void stopSync();
    bool isSynced() const { return !m_masterClock.isNull(); }

    // 设置时间偏移（从时钟可以有自己的偏移）
    void setTimeOffset(double offset);
    double getTimeOffset() const { return m_timeOffset; }

    // ==================== 时间获取（兼容原有调用） ====================
    double getTime() const;
    qint64 getTimeMs() const;
    bool isPaused() const;

    // ==================== 直接时间操作（跨线程安全） ====================
    Q_INVOKABLE void setTimeMs(qint64 ms);
    Q_INVOKABLE void startFromMs(qint64 ms);

signals:
    void timeChanged(double time);
    void timeChangedMs(qint64 ms);
    void pausedChanged(bool paused);
    void syncStateChanged(bool synced);
    void syncRequested(Clock* masterClock);
private slots:
    void onMasterTimeChanged(double time);
    void onMasterPausedChanged(bool paused);
    void onMasterDestroyed();
    void updateFromSync();
    void processSyncRequest(Clock* masterClock);
    void onInternalTimerTimeout();
private:
    // ==================== 内部计算（核心修复：自动走时逻辑） ====================
    double calculateCurrentTime() const;
    void updateSyncTimer();
    bool isEmittingTimeChanged() const;
    void setEmittingTimeChanged(bool emitting);

    // ==================== 时钟状态（保留原有所有变量） ====================
    double m_baseTime = 0.0;
    double m_timeOffset = 0.0;
    bool m_paused = true;
    double m_lastPauseTime = 0.0;

    // ==================== 同步相关（保留原有） ====================
    QPointer<Clock> m_masterClock;
    QTimer* m_syncTimer = nullptr;
    double m_lastSyncTime = 0.0;
    bool m_syncActive = false;

    // ==================== 计时器（保留原有） ====================
    QElapsedTimer m_timer;

    // ==================== 线程安全（保留原有） ====================
    mutable QMutex m_mutex;
    QAtomicInteger<int> m_emittingTimeChanged = 0;

    // ==================== 新增：内部走时定时器（核心自动走时） ====================
    // QTimer* m_internalTimer = nullptr;
    // 频率控制：避免信号频繁发射（每秒仅发射1次整秒信号）
    double m_lastEmitSecond = -1.0;
};

#endif // RINZEPLAYER_CLOCK_H