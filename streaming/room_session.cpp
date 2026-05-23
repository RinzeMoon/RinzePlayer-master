// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "room_session.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

RoomSession::RoomSession(QObject *parent) : QObject(parent)
    , m_reconnectTimer(new QTimer(this))
{
    m_reconnectTimer->setInterval(1000);
    connect(m_reconnectTimer, &QTimer::timeout, this, &RoomSession::onReconnectCountdown);
}

RoomSession::~RoomSession() { if (m_inRoom) leaveRoom(); }

void RoomSession::setChatRoomClient(ChatRoomClient *client)
{
    m_chatRoom = client;
    if (!m_chatRoom) return;
    m_chatRoom->setRoomHandler([this](const QJsonObject &msg) { onRoomMessage(msg); });
    m_chatRoom->setSyncHandler([this](const QJsonObject &msg) { onSyncMessage(msg); });
    m_chatRoom->setChatHandler([this](const QJsonObject &msg) { onChatMessage(msg); });
    m_chatRoom->setSystemHandler([this](const QJsonObject &msg) { onSystemMessage(msg); });
}

// ---- Room ----
void RoomSession::createRoom(const QString &userName, const QString &password)
{
    m_role = "host";
    QJsonObject d; d["user_name"] = userName;
    if (!password.isEmpty()) d["password"] = password;
    m_chatRoom->sendRoomAction("create", d);
}
void RoomSession::joinRoom(const QString &roomCode, const QString &userName, const QString &password)
{
    m_role = "guest"; m_roomCode = roomCode;
    QJsonObject d; d["room_code"] = roomCode; d["user_name"] = userName;
    if (!password.isEmpty()) d["password"] = password;
    m_chatRoom->sendRoomAction("join", d);
}
void RoomSession::leaveRoom()
{
    m_chatRoom->sendRoomAction("leave");
    if (m_inRoom) autoExport();
    m_inRoom = false; m_reconnectTimer->stop();
}

// ---- Sync (Host only) ----
void RoomSession::sendOpen(const QString &url)
{
    QJsonObject d; d["url"] = url;
    m_chatRoom->sendSyncAction("open", d);
}

void RoomSession::sendReady()
{
    m_chatRoom->sendRoomAction("ready");
}

// ---- Chat ----
void RoomSession::sendChatMessage(const QString &text, double playbackPos)
{
    m_chatRoom->sendChatMessage(text, playbackPos);
    // 本地也记录 (Server 广播排除发送者, 必须自己记)
    ChatMessage m{m_role, text, QDateTime::currentMSecsSinceEpoch(), playbackPos};
    m_messages.append(m);
    emit chatMessageReceived(m);
}

// ---- Message dispatch ----
void RoomSession::onRoomMessage(const QJsonObject &msg)
{
    QString action = msg["action"].toString();
    QJsonObject data = msg["data"].toObject();

    if (action == "created") {
        m_roomCode = data["room_code"].toString(); m_inRoom = true;
        emit roomCreated(m_roomCode);
    } else if (action == "joined") {
        m_peerName = data["host_name"].toString(); m_inRoom = true;
        emit joinedRoom();
    } else if (action == "guest_joined") {
        m_peerName = data["guest_name"].toString(); m_reconnectTimer->stop();
        emit peerJoined(m_peerName);
    } else if (action == "peer_left") {
        m_peerName.clear(); startReconnectWindow();
        emit peerLeft(data["reason"].toString());
    } else if (action == "closed") {
        m_reconnectTimer->stop(); m_inRoom = false; autoExport();
        emit roomClosed(data["reason"].toString());
    }
}

void RoomSession::onSyncMessage(const QJsonObject &msg)
{
    // v3: 所有 sync 消息透传给 MainWindow, 由 SyncManager 统一处理
    emit syncMessageReceived(msg);
}

void RoomSession::onChatMessage(const QJsonObject &msg)
{
    QJsonObject data = msg["data"].toObject();
    ChatMessage m{data["sender"].toString(), data["text"].toString(),
                   (qint64)data["wall_ts"].toDouble(), data["playback_pos"].toDouble()};
    m_messages.append(m); emit chatMessageReceived(m);
}

void RoomSession::onSystemMessage(const QJsonObject &msg) { Q_UNUSED(msg); }

// ---- Reconnect ----
void RoomSession::startReconnectWindow()
{
    m_reconnectRemaining = RECONNECT_WINDOW_SEC;
    emit reconnectTick(m_reconnectRemaining);
    m_reconnectTimer->start();
}
void RoomSession::onReconnectCountdown()
{
    m_reconnectRemaining--;
    if (m_reconnectRemaining > 0) { emit reconnectTick(m_reconnectRemaining); return; }
    m_reconnectTimer->stop(); m_inRoom = false; autoExport();
    emit roomClosed("timeout");
}

// ---- Export ----
void RoomSession::autoExport()
{
    if (m_messages.isEmpty()) return;
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                      + "/RinzePlayer/chatlogs"; QDir().mkpath(baseDir);
    QString baseName = baseDir + "/" + m_roomCode + "_"
                       + QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");
    QJsonObject r; r["room_code"] = m_roomCode; r["role"] = m_role;
    QJsonArray a;
    for (const auto &m : m_messages) {
        QJsonObject o; o["sender"]=m.sender; o["text"]=m.text;
        o["wall_ts"]=m.wallTs; o["playback_pos"]=m.playbackPos; a.append(o);
    }
    r["messages"] = a;
    QFile f(baseName + ".json");
    if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(r).toJson()); f.close(); }
    emit exportFinished(baseName + ".json"); m_messages.clear();
}
