// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "equalizer.h"
#include <cmath>

Equalizer::Equalizer(QObject *parent)
    : AudioEffect(parent) {
    addParameter("lowGain", tr("Low Gain (dB)"), -12.0, 12.0, 0.0);
    addParameter("lowFreq", tr("Low Frequency (Hz)"), 20.0, 200.0, 100.0);
    addParameter("midGain", tr("Mid Gain (dB)"), -12.0, 12.0, 0.0);
    addParameter("midFreq", tr("Mid Frequency (Hz)"), 200.0, 5000.0, 1000.0);
    addParameter("highGain", tr("High Gain (dB)"), -12.0, 12.0, 0.0);
    addParameter("highFreq", tr("High Frequency (Hz)"), 2000.0, 20000.0, 8000.0);
    addParameter("q", tr("Q Factor"), 0.1, 10.0, 1.0);

    m_filters.resize(3);
}

Equalizer::~Equalizer() {
}

bool Equalizer::init(int sampleRate, int channels) {
    m_sampleRate = sampleRate;
    m_channels = channels;
    reset();
    m_dirty = true;
    return true;
}

void Equalizer::reset() {
    for (auto &filter : m_filters) {
        filter.reset();
    }
}

void Equalizer::onParameterChanged(const QString &id, double value) {
    Q_UNUSED(id);
    Q_UNUSED(value);
    m_dirty = true;
}

void Equalizer::designPeakingFilter(BiquadFilter &filter, double freq, double gain, double q) {
    double w0 = 2.0 * M_PI * freq / m_sampleRate;
    double cosw0 = cos(w0);
    double sinw0 = sin(w0);
    double alpha = sinw0 / (2.0 * q);

    double A = pow(10.0, gain / 40.0);

    filter.b0 = static_cast<float>(1.0 + alpha * A);
    filter.b1 = static_cast<float>(-2.0 * cosw0);
    filter.b2 = static_cast<float>(1.0 - alpha * A);
    double a0 = 1.0 + alpha / A;
    filter.a1 = static_cast<float>(-2.0 * cosw0 / a0);
    filter.a2 = static_cast<float>((1.0 - alpha / A) / a0);
    filter.b0 /= static_cast<float>(a0);
    filter.b1 /= static_cast<float>(a0);
    filter.b2 /= static_cast<float>(a0);
}

void Equalizer::process(float *data, int frames) {
    if (m_dirty) {
        designPeakingFilter(m_filters[0], getParameter("lowFreq"), getParameter("lowGain"), getParameter("q"));
        designPeakingFilter(m_filters[1], getParameter("midFreq"), getParameter("midGain"), getParameter("q"));
        designPeakingFilter(m_filters[2], getParameter("highFreq"), getParameter("highGain"), getParameter("q"));
        m_dirty = false;
    }

    for (int i = 0; i < frames; ++i) {
        for (int ch = 0; ch < m_channels; ++ch) {
            int idx = i * m_channels + ch;
            float sample = data[idx];
            
            for (auto &filter : m_filters) {
                sample = filter.process(sample, ch);
            }
            
            data[idx] = sample;
        }
    }
}
