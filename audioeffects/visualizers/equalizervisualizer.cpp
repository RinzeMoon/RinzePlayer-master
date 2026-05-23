// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "equalizervisualizer.h"
#include <cmath>

EqualizerVisualizer::EqualizerVisualizer(QWidget *parent)
    : EffectVisualizer(parent)
{
    // 频率点（对数刻度）
    m_freqPoints = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
}

EqualizerVisualizer::~EqualizerVisualizer()
{
}

void EqualizerVisualizer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 背景
    painter.fillRect(rect(), m_bgColor);

    // 网格
    drawGrid(painter);

    // 频率标签
    drawFrequencyLabels(painter);

    // 响应曲线
    drawResponseCurve(painter);
}

void EqualizerVisualizer::drawGrid(QPainter &painter)
{
    QPen gridPen(QColor(50, 60, 80), 1, Qt::DashLine);
    painter.setPen(gridPen);

    int w = width();
    int h = height();

    // 垂直线
    for (double freq : m_freqPoints) {
        double logFreq = log10(freq / 20.0) / log10(20000.0 / 20.0);
        int x = static_cast<int>(logFreq * w);
        painter.drawLine(x, 0, x, h);
    }

    // 水平线（dB刻度）
    for (int db : {-12, -6, 0, 6, 12}) {
        double y = h / 2.0 - (db / 12.0) * (h / 2.0 - 20);
        painter.drawLine(0, static_cast<int>(y), w, static_cast<int>(y));
    }

    // 0dB 线
    QPen zeroPen(QColor(100, 120, 150), 2);
    painter.setPen(zeroPen);
    painter.drawLine(0, h / 2, w, h / 2);
}

void EqualizerVisualizer::drawFrequencyLabels(QPainter &painter)
{
    painter.setPen(m_textColor);
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    int h = height();

    for (double freq : m_freqPoints) {
        double logFreq = log10(freq / 20.0) / log10(20000.0 / 20.0);
        int x = static_cast<int>(logFreq * width());

        QString label;
        if (freq >= 1000) {
            label = QString("%1k").arg(freq / 1000.0, 0, 'g', 2);
        } else {
            label = QString("%1").arg(static_cast<int>(freq));
        }

        QRect textRect(x - 20, h - 18, 40, 16);
        painter.drawText(textRect, Qt::AlignCenter, label);
    }
}

void EqualizerVisualizer::drawResponseCurve(QPainter &painter)
{
    if (!m_effect) return;

    int w = width();
    int h = height();
    int margin = 20;

    // 计算响应曲线
    QPainterPath path;
    bool first = true;

    for (int x = margin; x < w - margin; x++) {
        double logFreq = static_cast<double>(x - margin) / (w - 2 * margin);
        double freq = 20.0 * pow(20000.0 / 20.0, logFreq);
        double response = calculateResponse(freq);

        // 转换dB到Y坐标
        double y = h / 2.0 - (response / 12.0) * (h / 2.0 - margin);

        if (first) {
            path.moveTo(x, static_cast<int>(y));
            first = false;
        } else {
            path.lineTo(x, static_cast<int>(y));
        }
    }

    // 绘制填充区域
    QColor fillColor = m_lineColor;
    fillColor.setAlpha(50);
    painter.setBrush(fillColor);
    painter.setPen(Qt::NoPen);

    QPainterPath fillPath = path;
    fillPath.lineTo(w - margin, h / 2);
    fillPath.lineTo(margin, h / 2);
    fillPath.closeSubpath();
    painter.drawPath(fillPath);

    // 绘制曲线
    QPen curvePen(m_lineColor, 2);
    painter.setPen(curvePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

double EqualizerVisualizer::calculateResponse(double frequency)
{
    if (!m_effect) return 0.0;

    // 获取EQ参数
    double lowGain = m_effect->getParameter("lowGain");
    double lowFreq = m_effect->getParameter("lowFreq");
    double midGain = m_effect->getParameter("midGain");
    double midFreq = m_effect->getParameter("midFreq");
    double highGain = m_effect->getParameter("highGain");
    double highFreq = m_effect->getParameter("highFreq");
    double q = m_effect->getParameter("q");

    double totalResponse = 0.0;

    // 低频峰值响应
    double lowQ = q;
    double lowOctaves = 1.0;
    double lowWidth = lowFreq * (pow(2, lowOctaves / 2) - pow(2, -lowOctaves / 2));
    double lowDistance = log10(frequency / lowFreq);
    double lowWeight = exp(-(lowDistance * lowDistance) / (2 * lowQ * lowQ));
    totalResponse += lowGain * lowWeight;

    // 中频峰值响应
    double midQ = q;
    double midOctaves = 2.0;
    double midDistance = log10(frequency / midFreq);
    double midWeight = exp(-(midDistance * midDistance) / (2 * midQ * midQ));
    totalResponse += midGain * midWeight;

    // 高频峰值响应
    double highQ = q;
    double highOctaves = 1.5;
    double highDistance = log10(frequency / highFreq);
    double highWeight = exp(-(highDistance * highDistance) / (2 * highQ * highQ));
    totalResponse += highGain * highWeight;

    return qBound(-12.0, totalResponse, 12.0);
}

double EqualizerVisualizer::interpolate(double x, double x1, double y1, double x2, double y2)
{
    return y1 + (y2 - y1) * ((x - x1) / (x2 - x1));
}
