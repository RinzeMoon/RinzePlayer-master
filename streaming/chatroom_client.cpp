// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatroom_client.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QtEndian>

ChatRoomClient::ChatRoomClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnectTimer(new QTimer(this))
    , m_pingTimer(new QTimer(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &ChatRoomClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChatRoomClient::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &ChatRoomClient::onError);
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatRoomClient::onReadyRead);

    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ChatRoomClient::onReconnectTimer);

    m_pingTimer->setInterval(5000);
    connect(m_pingTimer, &QTimer::timeout, this, &ChatRoomClient::measurePing);
}

ChatRoomClient::~ChatRoomClient()
{
    m_manualDisconnect = true;
    m_socket->disconnectFromHost();
}

void ChatRoomClient::connectToServer(const QString &host, quint16 port)
{
    m_host = host;
    m_port = port;
    m_manualDisconnect = false;
    m_reconnectAttempts = 0;
    m_socket->connectToHost(host, port);
}

void ChatRoomClient::disconnectFromServer()
{
    m_manualDisconnect = true;
    stopReconnect();
    m_socket->disconnectFromHost();
}

bool ChatRoomClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void ChatRoomClient::sendMessage(const QJsonObject &msg)
{
    if (!isConnected()) return;
    QJsonObject stamped = msg;
    stamped["client_ts"] = QDateTime::currentMSecsSinceEpoch();
    writeFrame(stamped);
}

void ChatRoomClient::sendRoomAction(const QString &action, const QJsonObject &data)
{
    QJsonObject msg;
    msg["ver"] = 1;
    msg["type"] = "room";
    msg["action"] = action;
    msg["data"] = data;
    sendMessage(msg);
}

void ChatRoomClient::sendSyncAction(const QString &action, const QJsonObject &data)
{
    QJsonObject msg;
    msg["ver"] = 1;
    msg["type"] = "sync";
    msg["action"] = action;
    msg["data"] = data;
    sendMessage(msg);
}

void ChatRoomClient::sendChatMessage(const QString &text, double playbackPos)
{
    QJsonObject data;
    data["text"] = text;
    data["playback_pos"] = playbackPos;
    QJsonObject msg;
    msg["ver"] = 1;
    msg["type"] = "chat";
    msg["action"] = "message";
    msg["data"] = data;
    sendMessage(msg);
}

void ChatRoomClient::writeFrame(const QJsonObject &msg)
{
    QJsonDocument doc(msg);
    QByteArray json = doc.toJson(QJsonDocument::Compact);
    quint32 len = qToBigEndian((quint32)json.size());
    m_socket->write(reinterpret_cast<const char *>(&len), 4);
    m_socket->write(json);
}

void ChatRoomClient::parseFrames()
{
    while (m_buffer.size() >= 4) {
        quint32 len;
        memcpy(&len, m_buffer.constData(), 4);
        len = qFromBigEndian(len);
        if ((quint32)m_buffer.size() < 4 + len)
            break;

        QByteArray json = m_buffer.mid(4, len);
        m_buffer.remove(0, 4 + len);

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(json, &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            dispatch(doc.object());
        }
    }
}

void ChatRoomClient::dispatch(const QJsonObject &msg)
{
    // Track seq
    if (msg.contains("seq")) {
        int seq = msg["seq"].toInt();
        if (seq > m_lastSeq + 1) {
            // Gap detected — request resend
            QJsonObject resend;
            resend["ver"] = 1;
            resend["type"] = "system";
            resend["action"] = "resend_request";
            QJsonObject data;
            data["from_seq"] = m_lastSeq + 1;
            resend["data"] = data;
            sendMessage(resend);
        }
        m_lastSeq = qMax(m_lastSeq, seq);
    }

    QString type = msg["type"].toString();
    QString action = msg["action"].toString();

    if (type == "room" && m_roomHandler) {
        m_roomHandler(msg);
    } else if (type == "sync" && m_syncHandler) {
        m_syncHandler(msg);
    } else if (type == "chat" && m_chatHandler) {
        m_chatHandler(msg);
    } else if (type == "system" && m_systemHandler) {
        m_systemHandler(msg);
    }
}

void ChatRoomClient::startReconnect()
{
    if (m_manualDisconnect) return;
    m_reconnectAttempts++;
    emit reconnecting(m_reconnectAttempts, MAX_RECONNECT_ATTEMPTS);
    emit connectionQuality(2, 999);
    if (m_reconnectAttempts > MAX_RECONNECT_ATTEMPTS) {
        emit connectionError("Max reconnect attempts reached");
        return;
    }
    int delay = RECONNECT_BASE_MS * (1 << (m_reconnectAttempts - 1));
    m_reconnectTimer->start(delay);
}

void ChatRoomClient::stopReconnect()
{
    m_reconnectTimer->stop();
    m_reconnectAttempts = 0;
    m_pingTimer->stop();
}

void ChatRoomClient::measurePing()
{
    if (!isConnected()) return;
    QJsonObject ping;
    ping["ver"] = 1;
    ping["type"] = "system";
    ping["action"] = "ping";
    ping["client_ts"] = QDateTime::currentMSecsSinceEpoch();
    writeFrame(ping);
}

// --- Slots ---

void ChatRoomClient::onConnected()
{
    m_reconnectAttempts = 0;
    m_buffer.clear();
    m_pingTimer->start();
    emit connected();
    emit connectionQuality(0, 0);  // good
}

void ChatRoomClient::onDisconnected()
{
    m_pingTimer->stop();
    emit disconnected();
    emit connectionQuality(2, 999); // bad
    if (!m_manualDisconnect) {
        startReconnect();
    }
}

void ChatRoomClient::onError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err)
    emit connectionError(m_socket->errorString());
}

void ChatRoomClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    parseFrames();
}

void ChatRoomClient::onReconnectTimer()
{
    if (!m_manualDisconnect) {
        m_socket->connectToHost(m_host, m_port);
    }
}
