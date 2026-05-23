// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ROOM_SESSION_H
#define ROOM_SESSION_H

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include "chatroom_client.h"

struct ChatMessage {
    QString sender, text;
    qint64 wallTs;
    double playbackPos;
};

class RoomSession : public QObject {
    Q_OBJECT
public:
    explicit RoomSession(QObject *parent = nullptr);
    ~RoomSession();

    void setChatRoomClient(ChatRoomClient *client);
    void setMediaController(class MediaController *ctrl) { m_controller = ctrl; }

    // ---- Room 操作 ----
    void createRoom(const QString &userName, const QString &password = "");
    void joinRoom(const QString &roomCode, const QString &userName, const QString &password = "");
    void leaveRoom();

    // ---- 上报 (由 MediaController 调用, Host 专用) ----
    void sendOpen(const QString &url);       // 打开视频
    void sendReady();                        // 视频加载完成

    // ---- 聊天 ----
    void sendChatMessage(const QString &text, double playbackPos);

    // ---- 访问器 ----
    QString roomCode() const { return m_roomCode; }
    QString role() const { return m_role; }
    QString peerName() const { return m_peerName; }
    bool inRoom() const { return m_inRoom; }
    QList<ChatMessage> messages() const { return m_messages; }

    QString exportFormat() const { return m_exportFormat; }
    void setExportFormat(const QString &fmt) { m_exportFormat = fmt; }

signals:
    void roomCreated(const QString &code);
    void joinedRoom();
    void peerJoined(const QString &name);
    void peerLeft(const QString &reason);
    void roomClosed(const QString &reason);

    void syncMessageReceived(const QJsonObject &msg);  // 透传所有 sync 消息
    void chatMessageReceived(const ChatMessage &msg);
    void reconnectTick(int remainingSeconds);
    void exportFinished(const QString &filePath);

private:
    void onRoomMessage(const QJsonObject &msg);
    void onSyncMessage(const QJsonObject &msg);
    void onChatMessage(const QJsonObject &msg);
    void onSystemMessage(const QJsonObject &msg);

    void startReconnectWindow();
    void onReconnectCountdown();
    void autoExport();

    ChatRoomClient *m_chatRoom = nullptr;
    MediaController *m_controller = nullptr;

    QString m_roomCode;
    QString m_role;
    QString m_peerName;
    QString m_lastSentUrl;
    bool m_inRoom = false;

    QList<ChatMessage> m_messages;
    QTimer *m_reconnectTimer = nullptr;
    int m_reconnectRemaining = 0;
    QString m_exportFormat = "both";

    static constexpr int RECONNECT_WINDOW_SEC = 30;
};

#endif // ROOM_SESSION_H
