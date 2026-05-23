// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sync_manager.h"
#include "mediacontroller.h"
#include <QJsonObject>
#include <QDateTime>
#include <QTimer>
#include <QDebug>

SyncManager::SyncManager(QObject *parent)
    : QObject(parent)
{
}

void SyncManager::onLocalSeek(double position)
{
    qDebug() << "[Sync] onLocalSeek pos=" << position << "active=" << m_active << "chat=" << (m_chatRoom != nullptr);
    if (!m_active || !m_chatRoom) return;
    QJsonObject d; d["position"] = position;
    m_chatRoom->sendSyncAction("seek_request", d);
}

void SyncManager::onLocalPause(double position)
{
    if (!m_active || !m_chatRoom) return;
    QJsonObject d; d["position"] = position;
    m_chatRoom->sendSyncAction("pause", d);
}

void SyncManager::onLocalPlay(double position)
{
    if (!m_active || !m_chatRoom) return;
    QJsonObject d; d["position"] = position;
    m_chatRoom->sendSyncAction("play", d);
}

void SyncManager::reportCorrection(double expected)
{
    if (!m_active || !m_chatRoom || !m_controller) return;
    double actual = m_controller->getCurrentTime();
    if (qAbs(actual - expected) < 0.5) return;  // 落点够近, 不报
    QJsonObject d; d["actual"] = actual;
    m_chatRoom->sendSyncAction("correction", d);
    qDebug() << "[Sync] correction: expected" << expected << "actual" << actual;
}

void SyncManager::handleServerMsg(const QJsonObject &msg)
{
    QString action = msg["action"].toString();
    QJsonObject data = msg["data"].toObject();
    qDebug() << "[Sync] handleServerMsg action=" << action << "active=" << m_active << "opened=" << (m_controller ? m_controller->opened() : false);

    // ---- seq 去重 ----
    int seq = data["seq"].toInt(0);
    if (seq > 0 && seq <= m_lastSeq) {
        qDebug() << "[Sync] skip old seq" << seq << "(last" << m_lastSeq << ")";
        return;
    }
    if (seq > 0) m_lastSeq = seq;

    double pos = data["position"].toDouble();

    // ---- preload: 预加载 URL + 暂停等待 ----
    if (action == "preload") {
        QString url = data["url"].toString();
        bool paused = data["paused"].toBool(false);
        // 锁定期: 暂停本地播放
        if (paused && m_controller && m_controller->opened() && !m_controller->paused()) {
            emit remotePause();
        }
        if (m_role == "guest" && !url.isEmpty()) {
            // MainWindow 会处理打开
        }
        return;
    }

    // ---- start: 双方就绪, 激活同步 ----
    if (action == "start") {
        m_active = true;
        return;
    }

    // ---- seek: 跳转 ----
    if (action == "seek") {
        QString src = data["source_id"].toString();
        if (!src.isEmpty() && m_chatRoom && src == m_role) return;
        if (!m_active || !m_controller || !m_controller->opened()) return;
        m_controller->executeRemoteSeek(pos);
        return;
    }

    // ---- pause / play ----
    if (action == "pause") {
        if (!m_active || !m_controller || !m_controller->opened()) return;
        if (!m_controller->paused()) emit remotePause();
        return;
    }
    if (action == "play") {
        if (!m_active || !m_controller || !m_controller->opened()) return;
        if (m_controller->paused()) emit remotePlay();
        return;
    }
}
