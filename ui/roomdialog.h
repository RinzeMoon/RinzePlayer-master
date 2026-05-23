// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ROOMDIALOG_H
#define ROOMDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class RoomDialog : public QDialog {
    Q_OBJECT
public:
    enum Mode { Create, Join };

    explicit RoomDialog(Mode mode, QWidget *parent = nullptr);

    QString userName() const;
    QString roomCode() const;
    QString password() const;
    QString serverHost() const;
    quint16 serverPort() const;

    static constexpr quint16 DEFAULT_CHATROOM_PORT = 9200;

private slots:
    void onAccept();

private:
    void setupUi(Mode mode);

    QLineEdit *m_userNameEdit = nullptr;
    QLineEdit *m_roomCodeEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_hostEdit = nullptr;
    QLineEdit *m_portEdit = nullptr;
    QPushButton *m_okBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
};

#endif // ROOMDIALOG_H
