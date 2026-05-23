// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "status_client.h"
#include <QJsonDocument>
#include <QJsonObject>

StatusClient::StatusClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_heartbeatTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
{
    connect(m_socket, &QWebSocket::connected, this, &StatusClient::onConnected);
    connect(m_socket, &QWebSocket::disconnected, this, &StatusClient::onDisconnected);
    connect(m_socket, &QWebSocket::errorOccurred, this, &StatusClient::onError);
    connect(m_socket, &QWebSocket::textMessageReceived, this, &StatusClient::onTextMessageReceived);

    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &StatusClient::onReconnect);

    m_heartbeatTimer->setInterval(10000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &StatusClient::sendHeartbeat);
}

StatusClient::~StatusClient()
{
    m_manualDisconnect = true;
    m_socket->close();
}

void StatusClient::connectToServer(const QUrl &url)
{
    m_serverUrl = url;
    m_manualDisconnect = false;
    m_socket->open(url);
}

void StatusClient::disconnectFromServer()
{
    m_manualDisconnect = true;
    m_reconnectTimer->stop();
    m_heartbeatTimer->stop();
    m_socket->close();
}

bool StatusClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void StatusClient::sendHeartbeat()
{
    if (!isConnected()) return;
    QJsonObject msg;
    msg["type"] = "heartbeat";
    msg["client_ts"] = QDateTime::currentMSecsSinceEpoch();
    m_socket->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void StatusClient::onConnected()
{
    emit connected(m_userId);
}

void StatusClient::onDisconnected()
{
    m_heartbeatTimer->stop();
    emit disconnected();
    if (!m_manualDisconnect) {
        m_reconnectTimer->start(3000);
    }
}

void StatusClient::onError(QAbstractSocket::SocketError err)
{
    Q_UNUSED(err)
    emit connectionError(m_socket->errorString());
}

void StatusClient::onTextMessageReceived(const QString &message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;
    QJsonObject msg = doc.object();

    if (msg["type"].toString() == "welcome") {
        m_userId = msg["user_id"].toString();
        m_heartbeatIntervalMs = msg["heartbeat_interval_ms"].toInteger(10000);
        m_heartbeatTimer->start(m_heartbeatIntervalMs);
        m_reconnectTimer->stop();
        emit connected(m_userId);
    }
}

void StatusClient::onReconnect()
{
    if (!m_manualDisconnect) {
        m_socket->open(m_serverUrl);
    }
}
