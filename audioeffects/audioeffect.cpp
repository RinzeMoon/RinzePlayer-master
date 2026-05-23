// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audioeffect.h"

AudioEffect::AudioEffect(QObject *parent)
    : QObject(parent) {
}

AudioEffect::~AudioEffect() {
}

void AudioEffect::addParameter(const QString &id, const QString &name,
                                 double minValue, double maxValue, double defaultValue, int type) {
    Parameter param;
    param.id = id;
    param.name = name;
    param.minValue = minValue;
    param.maxValue = maxValue;
    param.defaultValue = defaultValue;
    param.currentValue = defaultValue;
    param.type = type;
    m_parameters.append(param);
}

void AudioEffect::setParameter(const QString &id, double value) {
    for (auto &param : m_parameters) {
        if (param.id == id) {
            double clampedValue = qBound(param.minValue, value, param.maxValue);
            if (param.currentValue != clampedValue) {
                param.currentValue = clampedValue;
                emit parameterChanged(id, clampedValue);
                onParameterChanged(id, clampedValue);
            }
            break;
        }
    }
}

double AudioEffect::getParameter(const QString &id) const {
    for (const auto &param : m_parameters) {
        if (param.id == id) {
            return param.currentValue;
        }
    }
    return 0.0;
}

void AudioEffect::setEnabled(bool enabled) {
    if (m_enabled != enabled) {
        m_enabled = enabled;
        emit enabledChanged(enabled);
    }
}
