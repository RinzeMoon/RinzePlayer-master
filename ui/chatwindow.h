// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QList>
#include "../streaming/room_session.h"

class ChatWindow : public QWidget {
    Q_OBJECT
public:
    explicit ChatWindow(QWidget *parent = nullptr);
    ~ChatWindow();

    void setRoomSession(RoomSession *session);

    void toggle();  // Toggle slide-in/out
    void slideIn();
    void slideOut();
    bool isOpen() const { return m_isOpen; }
    void setAnchorWidget(QWidget *w);
    void trackWindow();

    void addMessage(const ChatMessage &msg);
    void clearMessages();

signals:
    void sendMessage(const QString &text);
    void visibilityChanged(bool visible);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupUi();
    void onSendClicked();
    QString formatMessage(const ChatMessage &msg) const;

    RoomSession *m_session = nullptr;
    QWidget *m_anchor = nullptr;

    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_messageContainer = nullptr;
    QVBoxLayout *m_messageLayout = nullptr;
    QLineEdit *m_input = nullptr;
    QPushButton *m_sendBtn = nullptr;

    bool m_isOpen = false;
    int m_panelWidth = 280;
};

#endif // CHATWINDOW_H
