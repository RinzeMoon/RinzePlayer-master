// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "danmakuoverlay.h"
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>
#include <algorithm>

DanmakuOverlay::DanmakuOverlay(QWidget *parent)
    : QWidget(parent)
    , m_tickTimer(new QTimer(this))
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("background: transparent;");

    m_tickTimer->setInterval(50); // ~20fps
    connect(m_tickTimer, &QTimer::timeout, this, &DanmakuOverlay::tick);
    m_tickTimer->start();
}

DanmakuOverlay::~DanmakuOverlay() {}

void DanmakuOverlay::pushMessage(const QString &sender, const QString &text)
{
    if (!m_enabled) return;
    int track = allocateTrack();
    m_items.append({sender, text, track, QDateTime::currentMSecsSinceEpoch(), false});

    // Limit visible items
    while (m_items.size() > 5) {
        m_items.removeFirst();
    }
}

void DanmakuOverlay::pushSystem(const QString &text)
{
    if (!m_enabled) return;
    m_items.append({"system", text, m_trackCount / 2, QDateTime::currentMSecsSinceEpoch(), false});
    while (m_items.size() > 5) m_items.removeFirst();
}

void DanmakuOverlay::clearAll()
{
    m_items.clear();
    update();
}

int DanmakuOverlay::allocateTrack() const
{
    // Find track with no active item or the oldest one
    for (int t = 0; t < m_trackCount; ++t) {
        bool occupied = std::any_of(m_items.begin(), m_items.end(),
            [t](const DanmakuItem &item) { return item.track == t && !item.fadingOut; });
        if (!occupied) return t;
    }
    // All full — reuse the oldest one's track
    if (!m_items.isEmpty()) return m_items.first().track;
    return 0;
}

void DanmakuOverlay::tick()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool needUpdate = false;

    for (int i = m_items.size() - 1; i >= 0; --i) {
        qint64 elapsed = now - m_items[i].startMs;
        if (elapsed > m_visibleMs + m_fadeMs) {
            m_items.removeAt(i);
            needUpdate = true;
        } else if (elapsed > m_visibleMs && !m_items[i].fadingOut) {
            m_items[i].fadingOut = true;
            needUpdate = true;
        }
    }

    if (needUpdate) update();
}

void DanmakuOverlay::paintEvent(QPaintEvent *)
{
    if (!m_enabled || m_items.isEmpty()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    QFont font("Microsoft YaHei", 13);
    painter.setFont(font);

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (const auto &item : m_items) {
        qint64 elapsed = now - item.startMs;
        int alpha = 255;

        if (item.fadingOut) {
            int fadeElapsed = elapsed - m_visibleMs;
            alpha = 255 * (1.0 - (double)fadeElapsed / m_fadeMs);
            alpha = qBound(20, alpha, 255);
        } else if (elapsed < 200) {
            // Fade in
            alpha = 255 * ((double)elapsed / 200.0);
            alpha = qBound(40, alpha, 255);
        }

        QColor color;
        if (item.sender == "host") color = m_hostColor;
        else if (item.sender == "guest") color = m_guestColor;
        else color = m_systemColor;
        color.setAlpha(alpha);

        // Draw text with shadow/outline
        int y = item.track * m_trackHeight + 60;
        int x = width() / 2;

        QPainterPath path;
        QFontMetrics fm(font);
        QString displayText = QString("[%1] %2").arg(
            item.sender == "host" ? "Host" : item.sender == "guest" ? "Guest" : "System",
            item.text);
        QRect textRect = fm.boundingRect(displayText);
        int textX = x - textRect.width() / 2;

        // Shadow
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, alpha / 2));
        painter.drawRoundedRect(textRect.adjusted(textX - 12, y - 4, 12, 4), 6, 6);

        // Outline
        QPen outlinePen(QColor(0, 0, 0, alpha), 2);
        painter.setPen(outlinePen);
        painter.setBrush(color);
        painter.drawText(QPoint(textX + 1, y + fm.ascent() + 1), displayText);

        // Main text
        QPen textPen(color);
        painter.setPen(textPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawText(QPoint(textX, y + fm.ascent()), displayText);
    }
}

void DanmakuOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}
