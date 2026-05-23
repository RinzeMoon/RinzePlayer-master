// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EXPONENTIAL_BACKOFF_H
#define EXPONENTIAL_BACKOFF_H

#include "streaming_types.h"
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

class ExponentialBackoff : public QObject {
    Q_OBJECT
public:
    explicit ExponentialBackoff(QObject* parent = nullptr);
    ~ExponentialBackoff();

    // 初始化配置
    void init(const StreamingConfig& config);

    // 重置状态
    void reset();

    // 标记失败 - 增加重试计数器并计算下次等待时间
    void onFailure();

    // 标记成功 - 重置重试状态
    void onSuccess();

    // 获取当前应该等待的时间 (ms)
    double getCurrentWaitTimeMs() const;

    // 获取已重试次数
    int getRetryCount() const;

    // 检查是否超过最大重试次数
    bool hasReachedMaxRetries() const;

    // 等待指定时间后回调
    void scheduleNextRetry(std::function<void()> callback);

    // 取消计划的重试
    void cancelRetry();

signals:
    // 可以进行重试时的信号
    void retryReady();

private:
    StreamingConfig m_config;
    int m_retryCount = 0;
    double m_currentIntervalMs = 0.0;
    QTimer* m_retryTimer = nullptr;
    std::function<void()> m_pendingCallback;
};

#endif // EXPONENTIAL_BACKOFF_H
