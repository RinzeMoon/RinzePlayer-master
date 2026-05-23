// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef READYOVERLAY_H
#define READYOVERLAY_H

#include <QWidget>
#include <QPushButton>
#include <QTimer>

class ReadyOverlay : public QWidget {
    Q_OBJECT
public:
    explicit ReadyOverlay(QWidget *parent = nullptr);

    enum State { Hidden, Waiting, Done };
    void setState(State s);

signals:
    void readyClicked();

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    State m_state = Hidden;
    qint64 m_doneTime = 0;
    QTimer *m_fadeTimer = nullptr;
    float m_animT = 1.0f;
    int m_spinAngle = 0;
    QTimer *m_spinTimer = nullptr;
    QPushButton *m_btn = nullptr;
};

#endif // READYOVERLAY_H
