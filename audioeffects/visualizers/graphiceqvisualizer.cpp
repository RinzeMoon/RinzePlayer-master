// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "graphiceqvisualizer.h"
#include <cmath>

GraphicEQVisualizer::GraphicEQVisualizer(QWidget *parent)
    : EffectVisualizer(parent)
{
    m_freqs = GraphicEQ::frequencies();
    m_barColor = QColor(100, 200, 255);
    m_barColorBright = QColor(0, 255, 200);
}

GraphicEQVisualizer::~GraphicEQVisualizer()
{
}

void GraphicEQVisualizer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 背景
    painter.fillRect(rect(), m_bgColor);

    // 网格
    drawGrid(painter);

    // 柱子
    drawBars(painter);

    // 频率标签
    drawLabels(painter);
}

void GraphicEQVisualizer::drawGrid(QPainter &painter)
{
    QPen gridPen(QColor(50, 60, 80), 1, Qt::DotLine);
    painter.setPen(gridPen);

    int w = width();
    int h = height();
    int centerY = h / 2;

    // 水平线 (dB刻度)
    for (int db : {-12, -6, 0, 6, 12}) {
        double y = centerY - (db / 12.0) * (h / 2 - 20);
        painter.drawLine(0, static_cast<int>(y), w, static_cast<int>(y));
    }

    // 0dB 线
    QPen zeroPen(QColor(100, 120, 150), 2);
    painter.setPen(zeroPen);
    painter.drawLine(0, centerY, w, centerY);
}

void GraphicEQVisualizer::drawBars(QPainter &painter)
{
    if (!m_effect) return;

    int w = width();
    int h = height();
    int centerY = h / 2;

    // 柱子布局
    int margin = 30;
    int totalWidth = w - 2 * margin;
    int barCount = 10;
    double barGap = 4.0;
    double barWidth = (totalWidth - barGap * (barCount - 1)) / barCount;

    for (int i = 0; i < barCount; ++i) {
        double gain = m_effect->getParameter(QString("band%1").arg(i));

        // 位置
        double x = margin + i * (barWidth + barGap);

        // 高度
        double barHeight = (gain / 12.0) * (h / 2 - 20);
        double y;

        // 渐变
        QLinearGradient gradient(x, centerY, x, centerY - barHeight);
        if (gain >= 0) {
            gradient.setColorAt(0, m_barColor);
            gradient.setColorAt(1, m_barColorBright);
            y = centerY - barHeight;
        } else {
            gradient.setColorAt(0, QColor(255, 100, 100));
            gradient.setColorAt(1, QColor(255, 50, 50));
            y = centerY;
        }

        // 画柱子
        painter.setBrush(gradient);
        painter.setPen(Qt::NoPen);
        QRectF barRect(x, y, barWidth, fabs(barHeight));
        painter.drawRoundedRect(barRect, 4, 4);
    }
}

void GraphicEQVisualizer::drawLabels(QPainter &painter)
{
    painter.setPen(m_textColor);
    QFont font = painter.font();
    font.setPointSize(7);
    font.setBold(true);
    painter.setFont(font);

    int w = width();
    int h = height();
    int margin = 30;
    int totalWidth = w - 2 * margin;
    int barCount = 10;
    double barGap = 4.0;
    double barWidth = (totalWidth - barGap * (barCount - 1)) / barCount;

    for (int i = 0; i < barCount; ++i) {
        double x = margin + i * (barWidth + barGap) + barWidth / 2;

        QString label;
        if (m_freqs[i] >= 1000) {
            label = QString("%1k").arg(m_freqs[i] / 1000.0, 0, 'g', 2);
        } else {
            label = QString("%1").arg(static_cast<int>(m_freqs[i]));
        }

        QRectF textRect(x - 20, h - 18, 40, 14);
        painter.drawText(textRect, Qt::AlignCenter, label);
    }
}
