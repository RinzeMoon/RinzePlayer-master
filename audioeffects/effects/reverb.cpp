// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "reverb.h"
#include <cmath>

namespace {
    const int combTuningL[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    const int allpassTuningL[4] = {556, 441, 341, 225};
}

Reverb::Reverb(QObject *parent)
    : AudioEffect(parent) {
    addParameter("roomSize", tr("Room Size"), 0.0, 1.0, 0.5);
    addParameter("damping", tr("Damping"), 0.0, 1.0, 0.5);
    addParameter("wet", tr("Wet Level"), 0.0, 1.0, 0.3);
    addParameter("dry", tr("Dry Level"), 0.0, 1.0, 0.7);
    addParameter("width", tr("Stereo Width"), 0.0, 1.0, 0.5);

    m_combFilters.resize(8);
    m_allpassFilters.resize(4);
}

Reverb::~Reverb() {
}

bool Reverb::init(int sampleRate, int channels) {
    m_sampleRate = sampleRate;
    m_channels = channels;

    float scale = static_cast<float>(sampleRate) / 44100.0f;
    for (int i = 0; i < 8; ++i) {
        m_combFilters[i].init(static_cast<int>(combTuningL[i] * scale));
    }
    for (int i = 0; i < 4; ++i) {
        m_allpassFilters[i].init(static_cast<int>(allpassTuningL[i] * scale));
        m_allpassFilters[i].feedback = 0.5f;
    }

    reset();
    return true;
}

void Reverb::reset() {
    for (auto &comb : m_combFilters) {
        comb.reset();
    }
    for (auto &allpass : m_allpassFilters) {
        allpass.reset();
    }
}

void Reverb::process(float *data, int frames) {
    float roomSize = static_cast<float>(getParameter("roomSize"));
    float damping = static_cast<float>(getParameter("damping"));
    float wet = static_cast<float>(getParameter("wet"));
    float dry = static_cast<float>(getParameter("dry"));
    float width = static_cast<float>(getParameter("width"));

    float feedback = roomSize * 0.28f + 0.7f;
    float damp1 = damping * 0.4f;
    float damp2 = 1.0f - damp1;

    for (int i = 0; i < 8; ++i) {
        m_combFilters[i].feedback = feedback;
        m_combFilters[i].damp1 = damp1;
        m_combFilters[i].damp2 = damp2;
    }

    for (int i = 0; i < frames; ++i) {
        float inL = 0.0f;
        float inR = 0.0f;
        
        if (m_channels >= 1) inL = data[i * m_channels + 0];
        if (m_channels >= 2) inR = data[i * m_channels + 1];
        
        float mono = (inL + inR) * 0.5f;

        float outL = 0.0f;
        float outR = 0.0f;

        for (int c = 0; c < 8; ++c) {
            float combOut = m_combFilters[c].process(mono);
            outL += combOut;
            if (c < 4) outR += combOut;
        }

        for (auto &allpass : m_allpassFilters) {
            outL = allpass.process(outL);
            outR = allpass.process(outR);
        }

        float wet1 = wet * (width + 1.0f) / 2.0f;
        float wet2 = wet * (1.0f - width) / 2.0f;

        float finalL = inL * dry + outL * wet1 + outR * wet2;
        float finalR = inR * dry + outR * wet1 + outL * wet2;

        if (m_channels >= 1) data[i * m_channels + 0] = finalL;
        if (m_channels >= 2) data[i * m_channels + 1] = finalR;
    }
}
