// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHATROOM_CLIENT_H
#define CHATROOM_CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QByteArray>
#include <functional>

class ChatRoomClient : public QObject {
    Q_OBJECT
public:
    explicit ChatRoomClient(QObject *parent = nullptr);
    ~ChatRoomClient();

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    void sendMessage(const QJsonObject &msg);
    void sendRoomAction(const QString &action, const QJsonObject &data = {});
    void sendSyncAction(const QString &action, const QJsonObject &data = {});
    void sendChatMessage(const QString &text, double playbackPos);

    int lastSeq() const { return m_lastSeq; }

    using MessageHandler = std::function<void(const QJsonObject &)>;
    void setRoomHandler(MessageHandler h) { m_roomHandler = h; }
    void setSyncHandler(MessageHandler h) { m_syncHandler = h; }
    void setChatHandler(MessageHandler h) { m_chatHandler = h; }
    void setSystemHandler(MessageHandler h) { m_systemHandler = h; }

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &error);
    void latencyUpdated(int pingMs);
    void connectionQuality(int level, int pingMs); // 0=good 1=weak 2=bad
    void reconnecting(int attempt, int maxAttempts);

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError err);
    void onReadyRead();
    void onReconnectTimer();

private:
    void writeFrame(const QJsonObject &msg);
    void parseFrames();
    void dispatch(const QJsonObject &msg);
    void startReconnect();
    void stopReconnect();
    void measurePing();

    QTcpSocket *m_socket = nullptr;
    QByteArray m_buffer;
    QString m_host;
    quint16 m_port = 0;
    int m_lastSeq = 0;
    int m_reconnectAttempts = 0;
    bool m_manualDisconnect = false;

    MessageHandler m_roomHandler;
    MessageHandler m_syncHandler;
    MessageHandler m_chatHandler;
    MessageHandler m_systemHandler;

    QTimer *m_reconnectTimer = nullptr;
    QTimer *m_pingTimer = nullptr;

    static constexpr int MAX_RECONNECT_ATTEMPTS = 5;
    static constexpr int RECONNECT_BASE_MS = 1000;
};

#endif // CHATROOM_CLIENT_H
