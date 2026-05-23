// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef STATUS_CLIENT_H
#define STATUS_CLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonObject>
#include <QUrl>

class StatusClient : public QObject {
    Q_OBJECT
public:
    explicit StatusClient(QObject *parent = nullptr);
    ~StatusClient();

    void connectToServer(const QUrl &url);
    void disconnectFromServer();
    bool isConnected() const;
    QString userId() const { return m_userId; }

signals:
    void connected(const QString &userId);
    void disconnected();
    void connectionError(const QString &error);

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError err);
    void onTextMessageReceived(const QString &message);
    void sendHeartbeat();
    void onReconnect();

private:
    QWebSocket *m_socket = nullptr;
    QTimer *m_heartbeatTimer = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    QUrl m_serverUrl;
    QString m_userId;
    qint64 m_heartbeatIntervalMs = 10000;
    bool m_manualDisconnect = false;
};

#endif // STATUS_CLIENT_H
