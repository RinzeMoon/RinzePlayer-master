// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef STREAM_MANAGER_H
#define STREAM_MANAGER_H

#include "streaming_types.h"
#include "exponential_backoff.h"
#include "debounce_manager.h"
#include <QObject>
#include <QUrl>
#include <QTimer>
#include <QElapsedTimer>

// 前向声明
class Demux;

class StreamManager : public QObject {
    Q_OBJECT
public:
    explicit StreamManager(QObject* parent = nullptr);
    ~StreamManager();

    // 初始化配置
    void init(const StreamingConfig& config);

    // 打开流
    bool open(const QUrl& url);

    // 关闭流
    void close();

    // 播放/暂停
    void play();
    void pause();

    // 快进/快退
    void seek(double positionSec);

    // 设置 Demux 指针
    void setDemux(Demux* demux);

    // 获取当前状态
    const StreamingStats& stats() const;

    // 手动触发重连
    void manualReconnect();

    // 重置缓冲监控
    void resetBufferMonitoring();

signals:
    // 状态变化
    void stateChanged(StreamState oldState, StreamState newState);

    // 缓冲进度变化 (0-100%)
    void bufferProgressChanged(double percent, double bufferMB);

    // 连接成功/失败
    void connected();
    void connectionFailed(const QString& error);

    // 缓冲状态
    void bufferingStarted();
    void bufferingEnded();

    // 重连尝试
    void reconnecting(int attempt, int maxAttempts);

    // 错误提示
    void error(const QString& msg);

    // 播放就绪，可以开始
    void readyToPlay();

public slots:
    // 从Demux回调的状态更新
    void onDemuxEOF();
    void onDemuxError(const QString& error);
    void onDemuxConnected();

    // 更新缓冲状态 - 从packet/frame队列大小估算
    void updateBufferStatus();

private:
    // 识别URL协议
    StreamProtocol detectProtocol(const QUrl& url);

    // 设置状态
    void setState(StreamState state);

    // 开始重连流程
    void startReconnect();

    // 执行重连
    void performReconnect();

    // 检查缓冲是否足够
    bool hasEnoughBuffer();

    // 计算估算的缓冲大小 (MB)
    double calculateEstimatedBufferMB();

    // 更新播放统计信息
    void updateStatsFromDemux();

    StreamingConfig m_config;
    StreamingStats m_stats;
    QUrl m_currentUrl;

    ExponentialBackoff* m_backoff = nullptr;
    DebounceManager* m_debounce = nullptr;

    Demux* m_demux = nullptr;

    QTimer* m_bufferMonitorTimer = nullptr;
    QElapsedTimer m_connectTimer;

    bool m_isShuttingDown = false;
};

#endif // STREAM_MANAGER_H
