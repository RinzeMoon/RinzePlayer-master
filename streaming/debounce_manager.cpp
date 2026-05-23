// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "debounce_manager.h"
#include <QDebug>

DebounceManager::DebounceManager(QObject* parent)
    : QObject(parent) {
}

DebounceManager::~DebounceManager() {
    resetAll();
}

void DebounceManager::init(const StreamingConfig& config) {
    m_config = config;
}

void DebounceManager::debounce(const QString& actionName, std::function<void()> action) {
    // 如果已存在，取消之前的
    auto it = m_actions.find(actionName);
    if (it != m_actions.end()) {
        if (it.value().timer) {
            it.value().timer->stop();
            it.value().timer->deleteLater();
        }
        m_actions.erase(it);
    }

    DebounceAction newAction;
    newAction.pendingAction = action;
    newAction.timer = new QTimer(this);
    connect(newAction.timer, &QTimer::timeout, this, [this, actionName]() {
        auto i = m_actions.find(actionName);
        if (i != m_actions.end()) {
            if (i.value().pendingAction) {
                i.value().pendingAction();
            }
            if (i.value().timer) {
                i.value().timer->deleteLater();
            }
            m_actions.erase(i);
        }
    });

    newAction.timer->start(m_config.debounceTimeMs);
    m_actions[actionName] = newAction;
}

void DebounceManager::immediate(const QString& actionName, std::function<void()> action) {
    reset(actionName);
    if (action) {
        action();
    }
}

void DebounceManager::reset(const QString& actionName) {
    auto it = m_actions.find(actionName);
    if (it != m_actions.end()) {
        if (it.value().timer) {
            it.value().timer->stop();
            it.value().timer->deleteLater();
        }
        m_actions.erase(it);
    }
}

void DebounceManager::resetAll() {
    for (auto it = m_actions.begin(); it != m_actions.end(); ++it) {
        if (it.value().timer) {
            it.value().timer->stop();
            it.value().timer->deleteLater();
        }
    }
    m_actions.clear();
}
