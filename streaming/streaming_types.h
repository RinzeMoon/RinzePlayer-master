// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef STREAMING_TYPES_H
#define STREAMING_TYPES_H

#include <QString>
#include <QElapsedTimer>
#include <chrono>

// 流媒体协议类型
enum class StreamProtocol {
    Unknown,
    HLS,       // HTTP Live Streaming (.m3u8)
    RTMP,      // Real-Time Messaging Protocol
    RTSP,      // Real-Time Streaming Protocol
    LocalFile  // 本地文件
};

// 流媒体连接状态
enum class StreamState {
    Idle,           // 空闲
    Connecting,     // 连接中
    Connected,      // 已连接
    Buffering,      // 缓冲中
    Playing,        // 播放中
    Paused,         // 暂停
    Reconnecting,   // 重连中
    Error,          // 错误
    Disconnected    // 已断开
};

// 流媒体配置
struct StreamingConfig {
    // 指数退避参数
    double initialIntervalMs = 1000.0;        // 初始重试间隔(ms)
    double backoffFactor = 2.0;              // 退避因子
    double maxIntervalMs = 30000.0;          // 最大重试间隔(ms)
    int maxRetryCount = 5;                  // 最大重试次数

    // 缓冲配置 (MB)
    double preBufferSizeMB = 2.0;           // 预缓冲大小
    double minBufferThresholdMB = 0.5;      // 最小缓冲阈值

    // 播放控制防抖(ms)
    int debounceTimeMs = 300;               // 防抖间隔

    // 目标延迟(秒) - 尽量控制
    double maxLatencySec = 3.0;
};

// 流媒体状态信息
struct StreamingStats {
    StreamState state = StreamState::Idle;
    StreamProtocol protocol = StreamProtocol::Unknown;
    QString url;

    // 连接信息
    int connectionAttempts = 0;              // 连接尝试次数
    int retryCount = 0;                      // 重试次数
    double lastErrorTime = 0.0;             // 上次错误时间
    QString lastErrorMsg;

    // 缓冲信息 (MB)
    double currentBufferMB = 0.0;
    double bufferProgressPercent = 0.0;
    bool isBuffering = false;

    // 播放质量
    double bitrateKbps = 0.0;
    int resolutionWidth = 0;
    int resolutionHeight = 0;
    double framerate = 0.0;

    // 重置
    void reset() {
        state = StreamState::Idle;
        protocol = StreamProtocol::Unknown;
        connectionAttempts = 0;
        retryCount = 0;
        lastErrorTime = 0.0;
        lastErrorMsg.clear();
        currentBufferMB = 0.0;
        bufferProgressPercent = 0.0;
        isBuffering = false;
        bitrateKbps = 0.0;
        resolutionWidth = 0;
        resolutionHeight = 0;
        framerate = 0.0;
    }
};

#endif // STREAMING_TYPES_H
