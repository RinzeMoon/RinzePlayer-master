// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "readyoverlay.h"
#include <QPainter>
#include <QDateTime>
#include <QPushButton>

ReadyOverlay::ReadyOverlay(QWidget *parent)
    : QWidget(parent)
    , m_fadeTimer(new QTimer(this))
    , m_spinTimer(new QTimer(this))
{
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("background: transparent;");
    setVisible(false);

    m_btn = new QPushButton("我准备好了", this);
    m_btn->setStyleSheet(
        "QPushButton { background: #2d6ff7; color: white; border: none; "
        "border-radius: 8px; padding: 12px 36px; font-size: 16px; font-weight: bold; } "
        "QPushButton:hover { background: #4d8fff; } "
        "QPushButton:disabled { background: #3a3a3e; color: #666; }");
    m_btn->setVisible(false);
    connect(m_btn, &QPushButton::clicked, this, [this]() {
        m_btn->setEnabled(false);
        m_btn->setText("已准备 ✓");
        emit readyClicked();
    });

    m_fadeTimer->setInterval(50);
    connect(m_fadeTimer, &QTimer::timeout, this, [this]() {
        if (m_state == Done) {
            float elapsed = (QDateTime::currentMSecsSinceEpoch() - m_doneTime) / 1200.0f;
            m_animT = 1.0f - qBound(0.0f, elapsed, 1.0f);
            if (m_animT <= 0.0f) { m_state = Hidden; m_fadeTimer->stop(); setVisible(false); }
        }
        update();
    });
}

void ReadyOverlay::setState(State s)
{
    m_state = s;
    if (s == Done) {
        m_doneTime = QDateTime::currentMSecsSinceEpoch(); m_animT = 1.0f;
        m_fadeTimer->start(); m_btn->setVisible(false); setVisible(true);
    } else if (s == Waiting) {
        m_fadeTimer->stop();
        m_btn->setVisible(true); m_btn->setEnabled(true);
        m_btn->setText("我准备好了");
        setVisible(true);
    } else {
        m_fadeTimer->stop(); m_btn->setVisible(false); setVisible(false);
    }
    update();
}

void ReadyOverlay::resizeEvent(QResizeEvent *) {
    if (m_btn) {
        m_btn->resize(180, 44);
        m_btn->move((width() - m_btn->width()) / 2, height() / 2 + 50);
    }
}

void ReadyOverlay::paintEvent(QPaintEvent *)
{
    if (m_state == Hidden) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    float alpha = (m_state == Done) ? m_animT : 1.0f;
    if (alpha <= 0.0f) return;
    int cx = width() / 2;

    QFont f("Microsoft YaHei", 22, QFont::DemiBold);
    p.setFont(f);
    bool done = (m_state == Done);
    QColor tc = done ? QColor(0, 255, 136) : QColor(255, 255, 255);
    tc.setAlphaF(alpha); p.setPen(tc);
    QString title = done ? "准备完成" : "准备就绪, 等待对方...";
    p.drawText(cx - QFontMetrics(f).horizontalAdvance(title) / 2, height() / 2 + 10, title);
}
