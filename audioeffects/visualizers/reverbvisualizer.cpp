// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "reverbvisualizer.h"
#include <cmath>

ReverbVisualizer::ReverbVisualizer(QWidget *parent)
    : EffectVisualizer(parent)
    , m_sampleRate(44100)
    , m_phase(0.0f)
{
    m_waveform.resize(512);
    m_timer.start();
}

ReverbVisualizer::~ReverbVisualizer()
{
}

void ReverbVisualizer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 背景
    painter.fillRect(rect(), m_bgColor);

    // 更新波形
    updateWaveform();

    // 绘制衰减曲线
    drawDecayCurve(painter);

    // 绘制波形
    drawWaveform(painter);
}

void ReverbVisualizer::updateWaveform()
{
    float time = m_timer.elapsed() / 1000.0f;

    // 生成一个简单的脉冲响应可视化
    for (int i = 0; i < m_waveform.size(); i++) {
        float t = static_cast<float>(i) / m_waveform.size();
        float decay = exp(-t * 4.0f);

        // 添加一些反射
        float reflection1 = (t > 0.1f && t < 0.3f) ? exp(-(t - 0.2f) * 10.0f) * 0.5f : 0.0f;
        float reflection2 = (t > 0.3f && t < 0.6f) ? exp(-(t - 0.45f) * 8.0f) * 0.3f : 0.0f;
        float reflection3 = (t > 0.6f) ? exp(-(t - 0.75f) * 6.0f) * 0.15f : 0.0f;

        // 一些震荡
        float osc = sin(time * 2.0f + t * 20.0f) * 0.1f;

        m_waveform[i] = decay + reflection1 + reflection2 + reflection3 + osc;
        m_waveform[i] *= 0.5f; // 缩放
    }
}

void ReverbVisualizer::drawDecayCurve(QPainter &painter)
{
    int w = width();
    int h = height();
    int margin = 20;

    if (!m_effect) return;

    double roomSize = m_effect->getParameter("roomSize");
    double damping = m_effect->getParameter("damping");

    // 衰减时间（基于房间大小）
    double decayTime = 1.0 + roomSize * 3.0;

    QPainterPath path;
    path.moveTo(margin, margin);

    for (int x = margin; x < w - margin; x++) {
        double t = static_cast<double>(x - margin) / (w - 2 * margin);
        double decay = exp(-t * (5.0 - damping * 3.0));
        double y = margin + (1.0 - decay) * (h - 2 * margin);
        path.lineTo(x, static_cast<int>(y));
    }

    // 填充
    QColor fillColor = m_lineColor;
    fillColor.setAlpha(40);
    painter.setBrush(fillColor);
    painter.setPen(Qt::NoPen);

    QPainterPath fillPath = path;
    fillPath.lineTo(w - margin, h - margin);
    fillPath.lineTo(margin, h - margin);
    fillPath.closeSubpath();
    painter.drawPath(fillPath);

    // 绘制曲线
    QPen curvePen(m_lineColor, 2);
    painter.setPen(curvePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

void ReverbVisualizer::drawWaveform(QPainter &painter)
{
    int w = width();
    int h = height();
    int margin = 20;
    int centerY = h / 2;

    QPainterPath path;
    bool first = true;

    for (int i = 0; i < m_waveform.size(); i++) {
        double x = margin + (static_cast<double>(i) / m_waveform.size()) * (w - 2 * margin);
        double y = centerY - m_waveform[i] * (h / 2 - margin);

        if (first) {
            path.moveTo(static_cast<int>(x), static_cast<int>(y));
            first = false;
        } else {
            path.lineTo(static_cast<int>(x), static_cast<int>(y));
        }
    }

    QColor waveColor(255, 150, 50);
    QPen wavePen(waveColor, 1.5f);
    painter.setPen(wavePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}
