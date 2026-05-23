// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audiomixer.h"
#include <QDebug>

AudioMixer::AudioMixer(QObject *parent)
    : QObject(parent) {
}

AudioMixer::~AudioMixer() {
    clearEffects();
}

bool AudioMixer::init(int sampleRate, int channels) {
    m_sampleRate = sampleRate;
    m_channels = channels;
    
    for (auto &effect : m_effects) {
        if (!effect->init(sampleRate, channels)) {
            qWarning() << "Failed to init effect:" << effect->name();
        }
    }
    return true;
}

void AudioMixer::process(float *data, int frames) {
    if (!m_enabled || m_effects.isEmpty()) {
        if (m_masterVolume != 1.0) {
            for (int i = 0; i < frames * m_channels; ++i) {
                data[i] *= static_cast<float>(m_masterVolume);
            }
        }
        return;
    }

    for (auto &effect : m_effects) {
        if (effect->enabled()) {
            effect->process(data, frames);
        }
    }

    if (m_masterVolume != 1.0) {
        for (int i = 0; i < frames * m_channels; ++i) {
            data[i] *= static_cast<float>(m_masterVolume);
        }
    }
}

void AudioMixer::addEffect(std::shared_ptr<AudioEffect> effect) {
    if (!effect) return;
    
    effect->init(m_sampleRate, m_channels);
    m_effects.append(effect);
    emit effectAdded(m_effects.size() - 1);
}

void AudioMixer::removeEffect(int index) {
    if (index < 0 || index >= m_effects.size()) return;
    
    m_effects.removeAt(index);
    emit effectRemoved(index);
}

void AudioMixer::moveEffect(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= m_effects.size()) return;
    if (toIndex < 0 || toIndex >= m_effects.size()) return;
    if (fromIndex == toIndex) return;
    
    auto effect = m_effects.takeAt(fromIndex);
    m_effects.insert(toIndex, effect);
    emit effectsReordered();
}

void AudioMixer::clearEffects() {
    for (int i = m_effects.size() - 1; i >= 0; --i) {
        removeEffect(i);
    }
}

std::shared_ptr<AudioEffect> AudioMixer::effect(int index) const {
    if (index < 0 || index >= m_effects.size()) return nullptr;
    return m_effects[index];
}

void AudioMixer::setMasterVolume(double volume) {
    double clampedVolume = qBound(0.0, volume, 2.0);
    if (m_masterVolume != clampedVolume) {
        m_masterVolume = clampedVolume;
        emit masterVolumeChanged(clampedVolume);
    }
}

void AudioMixer::resetAll() {
    for (auto &effect : m_effects) {
        effect->reset();
    }
}

void AudioMixer::setEnabled(bool enabled) {
    if (m_enabled != enabled) {
        m_enabled = enabled;
        emit enabledChanged(enabled);
    }
}
