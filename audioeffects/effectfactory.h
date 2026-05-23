// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EFFECTFACTORY_H
#define EFFECTFACTORY_H

#include "audioeffect.h"
#include "effects/equalizer.h"
#include "effects/reverb.h"
#include "effects/graphiceq.h"
#include <memory>

class EffectFactory {
public:
    static std::shared_ptr<AudioEffect> createEffect(AudioEffect::EffectType type) {
        switch (type) {
            case AudioEffect::EffectType::EQ:
                return std::make_shared<Equalizer>();
            case AudioEffect::EffectType::Reverb:
                return std::make_shared<Reverb>();
            case static_cast<AudioEffect::EffectType>(1001): // Graphic EQ
                return std::make_shared<GraphicEQ>();
            default:
                return nullptr;
        }
    }

    static std::shared_ptr<AudioEffect> createEffectByName(const QString &name) {
        if (name == "Equalizer") {
            return std::make_shared<Equalizer>();
        } else if (name == "Reverb") {
            return std::make_shared<Reverb>();
        } else if (name == "Graphic EQ") {
            return std::make_shared<GraphicEQ>();
        }
        return nullptr;
    }

    static QVector<QString> availableEffects() {
        return {
            "Equalizer",
            "Graphic EQ",
            "Reverb"
        };
    }
};

#endif // EFFECTFACTORY_H
