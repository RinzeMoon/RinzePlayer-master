// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "roomdialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QIntValidator>
#include <QMessageBox>
#include <QKeyEvent>

RoomDialog::RoomDialog(Mode mode, QWidget *parent)
    : QDialog(parent)
{
    setupUi(mode);

    if (mode == Create) {
        setWindowTitle("创建双人观影房间");
    } else {
        setWindowTitle("加入双人观影房间");
    }
    setFixedSize(380, 340);
    setStyleSheet("QDialog { background: #1e1e22; color: #e0e0e0; }"
                  "QLineEdit { background: #2a2a2e; color: #e0e0e0; border: 1px solid #3a3a3e; "
                  "border-radius: 4px; padding: 6px; }"
                  "QLabel { color: #c0c0c0; }");
}

void RoomDialog::setupUi(Mode mode)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Room code display area (Create mode only)
    auto *title = new QLabel(mode == Create ? "创建房间" : "加入房间");
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #e0e0e0;");
    mainLayout->addWidget(title);

    // User info
    auto *userGroup = new QGroupBox("用户信息");
    userGroup->setStyleSheet("QGroupBox { color: #a0a0a0; border: 1px solid #3a3a3e; "
                             "border-radius: 4px; margin-top: 8px; padding-top: 16px; }");
    auto *userForm = new QFormLayout(userGroup);
    m_userNameEdit = new QLineEdit;
    m_userNameEdit->setPlaceholderText("输入你的昵称");
    userForm->addRow("昵称:", m_userNameEdit);
    mainLayout->addWidget(userGroup);

    // Room info
    auto *roomGroup = new QGroupBox(mode == Create ? "房间设置" : "房间信息");
    roomGroup->setStyleSheet(userGroup->styleSheet());
    auto *roomForm = new QFormLayout(roomGroup);

    if (mode == Join) {
        m_roomCodeEdit = new QLineEdit;
        m_roomCodeEdit->setPlaceholderText("6位房间号");
        m_roomCodeEdit->setMaxLength(6);
        roomForm->addRow("房间号:", m_roomCodeEdit);
    }

    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("可选，留空则无密码");
    roomForm->addRow("密码:", m_passwordEdit);
    mainLayout->addWidget(roomGroup);

    // Server info
    auto *serverGroup = new QGroupBox("服务器");
    serverGroup->setStyleSheet(userGroup->styleSheet());
    auto *serverForm = new QFormLayout(serverGroup);
    m_hostEdit = new QLineEdit("127.0.0.1");
    serverForm->addRow("地址:", m_hostEdit);
    m_portEdit = new QLineEdit(QString::number(DEFAULT_CHATROOM_PORT));
    m_portEdit->setValidator(new QIntValidator(1, 65535));
    serverForm->addRow("端口:", m_portEdit);
    mainLayout->addWidget(serverGroup);

    // Buttons
    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    m_cancelBtn = new QPushButton("取消");
    m_cancelBtn->setStyleSheet("QPushButton { background: #3a3a3e; color: #e0e0e0; border: none; "
                               "border-radius: 4px; padding: 8px 20px; } QPushButton:hover { background: #4a4a4e; }");
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_cancelBtn);

    m_okBtn = new QPushButton(mode == Create ? "创建" : "加入");
    m_okBtn->setDefault(true);
    m_okBtn->setStyleSheet("QPushButton { background: #2d6ff7; color: white; border: none; "
                           "border-radius: 4px; padding: 8px 24px; } QPushButton:hover { background: #4d8fff; }");
    connect(m_okBtn, &QPushButton::clicked, this, &RoomDialog::onAccept);
    btnLayout->addWidget(m_okBtn);
    mainLayout->addLayout(btnLayout);

    m_userNameEdit->setFocus();
}

void RoomDialog::onAccept()
{
    if (m_userNameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入昵称");
        m_userNameEdit->setFocus();
        return;
    }
    if (m_roomCodeEdit && m_roomCodeEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入房间号");
        m_roomCodeEdit->setFocus();
        return;
    }
    if (m_hostEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入服务器地址");
        m_hostEdit->setFocus();
        return;
    }
    accept();
}

QString RoomDialog::userName() const { return m_userNameEdit->text().trimmed(); }
QString RoomDialog::roomCode() const { return m_roomCodeEdit ? m_roomCodeEdit->text().trimmed() : QString(); }
QString RoomDialog::password() const { return m_passwordEdit->text(); }
QString RoomDialog::serverHost() const { return m_hostEdit->text().trimmed(); }
quint16 RoomDialog::serverPort() const { return m_portEdit->text().toUShort(); }
