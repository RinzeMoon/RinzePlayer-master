// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DEBOUNCE_MANAGER_H
#define DEBOUNCE_MANAGER_H

#include "streaming_types.h"
#include <QString>
#include <QHash>
#include <QTimer>
#include <QObject>
#include <functional>

class DebounceManager : public QObject {
    Q_OBJECT
public:
    explicit DebounceManager(QObject* parent = nullptr);
    ~DebounceManager();

    // 初始化配置
    void init(const StreamingConfig& config);

    // 防抖执行操作 - 如果在防抖时间内有重复操作，最后一个才执行
    void debounce(const QString& actionName, std::function<void()> action);

    // 立即执行，取消之前的防抖
    void immediate(const QString& actionName, std::function<void()> action);

    // 重置某个操作的防抖状态
    void reset(const QString& actionName);

    // 重置所有
    void resetAll();

private:
    struct DebounceAction {
        QTimer* timer = nullptr;
        std::function<void()> pendingAction;
    };

    StreamingConfig m_config;
    QHash<QString, DebounceAction> m_actions;
};

#endif // DEBOUNCE_MANAGER_H
