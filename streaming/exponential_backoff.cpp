// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "exponential_backoff.h"
#include <QDebug>

ExponentialBackoff::ExponentialBackoff(QObject* parent)
    : QObject(parent) {
    m_retryTimer = new QTimer(this);
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, [this]() {
        if (m_pendingCallback) {
            m_pendingCallback();
            m_pendingCallback = nullptr;
        }
        emit retryReady();
    });
}

ExponentialBackoff::~ExponentialBackoff() {
    cancelRetry();
}

void ExponentialBackoff::init(const StreamingConfig& config) {
    m_config = config;
    reset();
}

void ExponentialBackoff::reset() {
    m_retryCount = 0;
    m_currentIntervalMs = m_config.initialIntervalMs;
    cancelRetry();
}

void ExponentialBackoff::onFailure() {
    m_retryCount++;
    if (m_retryCount == 1) {
        m_currentIntervalMs = m_config.initialIntervalMs;
    } else {
        m_currentIntervalMs = std::min(
            m_currentIntervalMs * m_config.backoffFactor,
            m_config.maxIntervalMs
        );
    }
    qDebug() << "Retry attempt:" << m_retryCount 
             << "Next interval:" << m_currentIntervalMs << "ms";
}

void ExponentialBackoff::onSuccess() {
    reset();
}

double ExponentialBackoff::getCurrentWaitTimeMs() const {
    return m_currentIntervalMs;
}

int ExponentialBackoff::getRetryCount() const {
    return m_retryCount;
}

bool ExponentialBackoff::hasReachedMaxRetries() const {
    return m_retryCount >= m_config.maxRetryCount;
}

void ExponentialBackoff::scheduleNextRetry(std::function<void()> callback) {
    cancelRetry();
    m_pendingCallback = callback;
    m_retryTimer->start(static_cast<int>(m_currentIntervalMs));
}

void ExponentialBackoff::cancelRetry() {
    if (m_retryTimer->isActive()) {
        m_retryTimer->stop();
    }
    m_pendingCallback = nullptr;
}
