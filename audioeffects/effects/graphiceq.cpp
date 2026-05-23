// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "graphiceq.h"
#include <cmath>

// 10段标准 ISO 频率点
static const double ISO_FREQS[] = {
    31.25, 62.5, 125, 250, 500,
    1000, 2000, 4000, 8000, 16000
};

QVector<double> GraphicEQ::frequencies()
{
    QVector<double> result;
    for (int i = 0; i < 10; ++i)
        result.append(ISO_FREQS[i]);
    return result;
}

GraphicEQ::GraphicEQ(QObject *parent)
    : AudioEffect(parent), m_dirty(true)
{
    m_freqs = frequencies();
    m_filters.resize(10);

    // 添加10个参数
    for (int i = 0; i < 10; ++i) {
        QString id = QString("band%1").arg(i);
        QString name;
        if (m_freqs[i] >= 1000) {
            name = QString("%1kHz").arg(m_freqs[i] / 1000.0, 0, 'g', 3);
        } else {
            name = QString("%1Hz").arg(m_freqs[i], 0, 'g', 4);
        }
        addParameter(id, name, -12.0, 12.0, 0.0);
    }
}

GraphicEQ::~GraphicEQ()
{
}

bool GraphicEQ::init(int sampleRate, int channels)
{
    m_sampleRate = sampleRate;
    m_channels = channels;
    reset();
    designFilters();
    return true;
}

void GraphicEQ::reset()
{
    for (auto &f : m_filters)
        f.reset();
}

void GraphicEQ::onParameterChanged(const QString &id, double value)
{
    Q_UNUSED(id);
    Q_UNUSED(value);
    m_dirty = true;
}

void GraphicEQ::designFilters()
{
    // 用二阶峰值滤波器
    double Q = 1.414; // sqrt(2)，标准的 octave Q

    for (int i = 0; i < 10; ++i) {
        double freq = m_freqs[i];
        double gain = getParameter(QString("band%1").arg(i));

        double w0 = 2.0 * M_PI * freq / m_sampleRate;
        double cosw0 = cos(w0);
        double sinw0 = sin(w0);
        double alpha = sinw0 / (2.0 * Q);

        double A = pow(10.0, gain / 40.0);

        m_filters[i].b0 = static_cast<float>(1.0 + alpha * A);
        m_filters[i].b1 = static_cast<float>(-2.0 * cosw0);
        m_filters[i].b2 = static_cast<float>(1.0 - alpha * A);
        double a0 = 1.0 + alpha / A;
        m_filters[i].a1 = static_cast<float>(-2.0 * cosw0 / a0);
        m_filters[i].a2 = static_cast<float>((1.0 - alpha / A) / a0);
        m_filters[i].b0 /= static_cast<float>(a0);
        m_filters[i].b1 /= static_cast<float>(a0);
        m_filters[i].b2 /= static_cast<float>(a0);
    }

    m_dirty = false;
}

void GraphicEQ::process(float *data, int frames)
{
    if (m_dirty) designFilters();

    for (int i = 0; i < frames; ++i) {
        for (int ch = 0; ch < m_channels; ++ch) {
            int idx = i * m_channels + ch;
            float sample = data[idx];

            // 依次通过10个滤波器
            for (auto &f : m_filters) {
                sample = f.process(sample, ch);
            }

            data[idx] = sample;
        }
    }
}
