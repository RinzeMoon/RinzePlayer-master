// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SYNC_MANAGER_H
#define SYNC_MANAGER_H

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include "../streaming/chatroom_client.h"

class MediaController;

class SyncManager : public QObject {
    Q_OBJECT
public:
    explicit SyncManager(QObject *parent = nullptr);

    void setChatRoomClient(ChatRoomClient *c) { m_chatRoom = c; }
    void setMediaController(MediaController *c) { m_controller = c; }
    void setRole(const QString &r) { m_role = r; }

    // Host 发送操作
    void onLocalSeek(double position);
    void onLocalPause(double position);
    void onLocalPlay(double position);

    // 处理 Server 广播 (preload / start / seek / pause / play)
    void handleServerMsg(const QJsonObject &msg);

    bool isSyncing() const { return m_active; }
    void setActive(bool a) { m_active = a; }

signals:
    void remotePlay();
    void remotePause();

private:
    void reportCorrection(double expected);
    ChatRoomClient *m_chatRoom = nullptr;
    MediaController *m_controller = nullptr;
    QString m_role;
    bool m_active = false;
    int m_lastSeq = 0;
};

#endif // SYNC_MANAGER_H
