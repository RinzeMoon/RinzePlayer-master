// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatwindow.h"
#include <QDateTime>
#include <QTimer>
#include <QKeyEvent>
#include <QApplication>
#include <QScrollBar>

ChatWindow::ChatWindow(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setupUi();
    setVisible(false);
}

ChatWindow::~ChatWindow() {}

void ChatWindow::setupUi()
{
    setFixedWidth(m_panelWidth);
    setStyleSheet("ChatWindow { background: rgba(24,24,28,0.95); border-left: 1px solid #3a3a3e; }");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *title = new QLabel("Chat");
    title->setStyleSheet("color: #e0e0e0; font-size: 14px; font-weight: bold;");
    layout->addWidget(title);

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; } QScrollBar:vertical { width: 6px; }");
    m_messageContainer = new QWidget;
    m_messageContainer->setStyleSheet("background: transparent;");
    m_messageLayout = new QVBoxLayout(m_messageContainer);
    m_messageLayout->setContentsMargins(0, 0, 0, 0);
    m_messageLayout->addStretch();
    m_scrollArea->setWidget(m_messageContainer);
    layout->addWidget(m_scrollArea, 1);

    m_input = new QLineEdit;
    m_input->setPlaceholderText("输入消息...");
    m_input->setStyleSheet(
        "QLineEdit { background: #1e1e22; color: #e0e0e0; border: 1px solid #3a3a3e; "
        "border-radius: 4px; padding: 6px; }");
    layout->addWidget(m_input);

    m_sendBtn = new QPushButton("发送");
    m_sendBtn->setStyleSheet(
        "QPushButton { background: #2d6ff7; color: white; border: none; "
        "border-radius: 4px; padding: 6px 16px; } QPushButton:hover { background: #4d8fff; }");
    m_sendBtn->setFixedHeight(30);
    layout->addWidget(m_sendBtn);

    connect(m_sendBtn, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
    connect(m_input, &QLineEdit::returnPressed, this, &ChatWindow::onSendClicked);

    resize(m_panelWidth, 500);
}

void ChatWindow::setRoomSession(RoomSession *session)
{
    m_session = session;
    if (m_session) {
        connect(m_session, &RoomSession::chatMessageReceived, this, &ChatWindow::addMessage);
    }
}

void ChatWindow::toggle()
{
    if (m_isOpen) slideOut();
    else slideIn();
}

void ChatWindow::setAnchorWidget(QWidget *w) { m_anchor = w; }

void ChatWindow::trackWindow() {
    if (!m_anchor) return;
    QRect g = m_anchor->geometry();
    setGeometry(g.right() - m_panelWidth - 2, g.top() + 2,
                m_panelWidth, g.height() - 4);
}

void ChatWindow::slideIn()
{
    if (m_isOpen) return;
    m_isOpen = true;
    trackWindow();
    raise();
    setVisible(true);
    m_input->setFocus();
    emit visibilityChanged(true);
}

void ChatWindow::slideOut()
{
    if (!m_isOpen) return;
    m_isOpen = false;
    setVisible(false);
    emit visibilityChanged(false);
}

void ChatWindow::addMessage(const ChatMessage &msg)
{
    QLabel *label = new QLabel(formatMessage(msg));
    label->setWordWrap(true);
    label->setStyleSheet(
        QString("color: %1; font-size: 12px; padding: 4px 0;")
            .arg(msg.sender == "host" ? "#f0a040" : "#60a0f0"));
    // Remove old stretch, add label, re-add stretch
    if (m_messageLayout->count() > 0) {
        QLayoutItem *stretch = m_messageLayout->takeAt(m_messageLayout->count() - 1);
        delete stretch;
    }
    m_messageLayout->addWidget(label);
    m_messageLayout->addStretch();

    // Auto-scroll to bottom
    QTimer::singleShot(50, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(m_scrollArea->verticalScrollBar()->maximum());
    });
}

void ChatWindow::clearMessages()
{
    while (m_messageLayout->count() > 1) {
        QLayoutItem *item = m_messageLayout->takeAt(0);
        if (item->widget()) delete item->widget();
        delete item;
    }
}

QString ChatWindow::formatMessage(const ChatMessage &msg) const
{
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(msg.wallTs);
    QString timeStr = dt.toString("HH:mm:ss");
    QString senderTag = msg.sender == "host" ? "Host" : "Guest";
    int mins = (int)(msg.playbackPos / 60);
    int secs = (int)(msg.playbackPos) % 60;
    return QString("[%1] %2\n> %3").arg(timeStr, senderTag, msg.text);
}

void ChatWindow::onSendClicked()
{
    QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;
    emit sendMessage(text);
    m_input->clear();
    m_input->setFocus();
}

void ChatWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        slideOut();
        return;
    }
    if (event->key() == Qt::Key_QuoteLeft && !m_input->hasFocus()) {
        slideOut();
        return;
    }
    QWidget::keyPressEvent(event);
}
