// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "stream_manager.h"
#include "demux/demux.h"
#include <QDebug>
#include <QFileInfo>

StreamManager::StreamManager(QObject* parent)
    : QObject(parent), m_demux(nullptr), m_isShuttingDown(false) {

    m_backoff = new ExponentialBackoff(this);
    m_debounce = new DebounceManager(this);

    // 缓冲监控定时器
    m_bufferMonitorTimer = new QTimer(this);
    m_bufferMonitorTimer->setInterval(100); // 每100ms检查一次
    connect(m_bufferMonitorTimer, &QTimer::timeout, this, &StreamManager::updateBufferStatus);
}

StreamManager::~StreamManager() {
    m_isShuttingDown = true;
    close();
}

void StreamManager::init(const StreamingConfig& config) {
    m_config = config;
    m_backoff->init(config);
    m_debounce->init(config);
}

StreamProtocol StreamManager::detectProtocol(const QUrl& url) {
    QString scheme = url.scheme().toLower();
    QString path = url.path().toLower();

    if (scheme == "rtmp" || scheme == "rtmps" || scheme == "rtmpte" || scheme == "rtmpt") {
        return StreamProtocol::RTMP;
    } else if (scheme == "rtsp" || scheme == "rtsps") {
        return StreamProtocol::RTSP;
    } else if (path.endsWith(".m3u8") || scheme == "http" || scheme == "https") {
        // HLS 通常是 m3u8，也可能是 http/https 直播流
        return StreamProtocol::HLS;
    } else if (url.isLocalFile() || scheme.isEmpty()) {
        return StreamProtocol::LocalFile;
    }
    return StreamProtocol::Unknown;
}

bool StreamManager::open(const QUrl& url) {
    m_currentUrl = url;
    m_stats.reset();
    m_stats.url = url.toString();
    m_stats.protocol = detectProtocol(url);

    qDebug() << "Opening stream:" << url.toString();
    qDebug() << "Detected protocol:" << static_cast<int>(m_stats.protocol);

    setState(StreamState::Connecting);
    m_stats.connectionAttempts++;
    m_backoff->reset();
    m_connectTimer.start();

    // 缓冲监控启动
    m_bufferMonitorTimer->start();

    return true;
}

void StreamManager::close() {
    m_isShuttingDown = true;

    m_bufferMonitorTimer->stop();
    m_backoff->cancelRetry();

    setState(StreamState::Disconnected);
    m_stats.reset();
    m_currentUrl.clear();

    m_isShuttingDown = false;
}

void StreamManager::play() {
    m_debounce->debounce("play_pause", [this]() {
        if (m_stats.state == StreamState::Paused || 
            m_stats.state == StreamState::Buffering || 
            m_stats.state == StreamState::Connected) {
            setState(StreamState::Playing);
        }
    });
}

void StreamManager::pause() {
    m_debounce->debounce("play_pause", [this]() {
        if (m_stats.state == StreamState::Playing) {
            setState(StreamState::Paused);
        }
    });
}

void StreamManager::seek(double positionSec) {
    m_debounce->debounce("seek", [this, positionSec]() {
        qDebug() << "Seek to:" << positionSec << "s";
        if (m_demux) {
            m_demux->seekBySec(positionSec, 0.0);
        }
    });
}

void StreamManager::setDemux(Demux* demux) {
    m_demux = demux;
}

const StreamingStats& StreamManager::stats() const {
    return m_stats;
}

void StreamManager::manualReconnect() {
    if (m_stats.state != StreamState::Reconnecting) {
        startReconnect();
    }
}

void StreamManager::resetBufferMonitoring() {
    m_stats.currentBufferMB = 0.0;
    m_stats.bufferProgressPercent = 0.0;
    m_stats.isBuffering = false;
}

void StreamManager::setState(StreamState state) {
    if (m_stats.state == state) return;

    StreamState oldState = m_stats.state;
    m_stats.state = state;

    qDebug() << "State changed from" << static_cast<int>(oldState) 
             << "to" << static_cast<int>(state);

    emit stateChanged(oldState, state);

    // 处理状态变化的特殊逻辑
    if (state == StreamState::Buffering) {
        emit bufferingStarted();
    } else if (oldState == StreamState::Buffering && 
               (state == StreamState::Playing || state == StreamState::Paused)) {
        emit bufferingEnded();
    }

    if (state == StreamState::Connected) {
        emit connected();
    } else if (state == StreamState::Error) {
        emit error(m_stats.lastErrorMsg);
    }
}

void StreamManager::startReconnect() {
    if (m_backoff->hasReachedMaxRetries()) {
        m_stats.lastErrorMsg = "Maximum retry attempts reached";
        setState(StreamState::Error);
        emit connectionFailed(m_stats.lastErrorMsg);
        return;
    }

    setState(StreamState::Reconnecting);
    emit reconnecting(m_backoff->getRetryCount() + 1, m_config.maxRetryCount);

    m_backoff->onFailure();
    m_backoff->scheduleNextRetry([this]() {
        performReconnect();
    });
}

void StreamManager::performReconnect() {
    if (m_isShuttingDown) return;

    qDebug() << "Performing reconnect attempt";
    setState(StreamState::Connecting);
    m_stats.connectionAttempts++;

    // 实际重连逻辑应该触发Demux重新连接
    if (m_demux) {
        // 这里可以添加实际重连调用
    }
}

bool StreamManager::hasEnoughBuffer() {
    return m_stats.currentBufferMB >= m_config.minBufferThresholdMB;
}

double StreamManager::calculateEstimatedBufferMB() {
    // 这里我们简单估算，实际可以从demux获取更准确的字节数
    // 对于流媒体，我们通过队列中帧数来估算
    double estimatedMB = 0.0;

    // 一个简单的估算方法，每帧按100KB估算
    // 实际应该从 codec 参数获取 bitrate 进行计算
    if (m_stats.bitrateKbps > 0) {
        // 按 bitrate 估算，假设缓冲了 0.5秒的数据
        double bits = m_stats.bitrateKbps * 500.0;
        estimatedMB = bits / (8.0 * 1024.0 * 1024.0);
    } else {
        // 默认预估值
        estimatedMB = m_config.preBufferSizeMB;
    }

    return estimatedMB;
}

void StreamManager::updateStatsFromDemux() {
    if (!m_demux) return;

    // 从Demux获取分辨率和帧率信息（实际项目里应该从解码后获取）
    auto* videoStream = m_demux->getStream(MediaType::Video);
    if (videoStream) {
        m_stats.resolutionWidth = videoStream->codecpar->width;
        m_stats.resolutionHeight = videoStream->codecpar->height;
        m_stats.framerate = av_q2d(videoStream->avg_frame_rate);
    }
}

void StreamManager::updateBufferStatus() {
    if (m_isShuttingDown) return;

    // 计算缓冲进度
    double estimatedBuffer = calculateEstimatedBufferMB();
    double progressPercent = std::min(100.0, (estimatedBuffer / m_config.preBufferSizeMB) * 100.0);

    bool bufferChanged = false;
    if (std::abs(m_stats.currentBufferMB - estimatedBuffer) > 0.01) {
        m_stats.currentBufferMB = estimatedBuffer;
        m_stats.bufferProgressPercent = progressPercent;
        bufferChanged = true;
    }

    // 检查是否需要缓冲
    bool needsBuffer = !hasEnoughBuffer();
    if (needsBuffer && !m_stats.isBuffering && m_stats.state == StreamState::Playing) {
        m_stats.isBuffering = true;
        setState(StreamState::Buffering);
    } else if (!needsBuffer && m_stats.isBuffering) {
        m_stats.isBuffering = false;
        if (m_stats.state == StreamState::Buffering) {
            setState(StreamState::Playing);
        }
    }

    if (bufferChanged) {
        emit bufferProgressChanged(progressPercent, estimatedBuffer);
    }
}

void StreamManager::onDemuxEOF() {
    qDebug() << "Stream EOF";
    if (m_stats.protocol != StreamProtocol::LocalFile) {
        // 网络流EOF可能意味着连接断开，尝试重连
        startReconnect();
    }
}

void StreamManager::onDemuxError(const QString& error) {
    qDebug() << "Stream error:" << error;
    m_stats.lastErrorMsg = error;

    if (m_stats.protocol == StreamProtocol::LocalFile) {
        setState(StreamState::Error);
    } else {
        // 网络流错误，尝试重连
        startReconnect();
    }
}

void StreamManager::onDemuxConnected() {
    qDebug() << "Stream connected successfully";
    m_backoff->onSuccess();
    setState(StreamState::Connected);

    // 检查是否需要缓冲
    if (m_config.preBufferSizeMB > 0) {
        setState(StreamState::Buffering);
    } else {
        emit readyToPlay();
    }

    updateStatsFromDemux();
}
