// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef REVERB_H
#define REVERB_H

#include "../audioeffect.h"
#include <vector>
#include <memory>

class Reverb : public AudioEffect {
    Q_OBJECT
public:
    explicit Reverb(QObject *parent = nullptr);
    ~Reverb() override;

    EffectType type() const override { return EffectType::Reverb; }
    QString name() const override { return tr("Reverb"); }
    QString description() const override { return tr("Room reverb effect"); }

    bool init(int sampleRate, int channels) override;
    void process(float *data, int frames) override;
    void reset() override;

private:
    struct CombFilter {
        std::vector<float> buffer;
        int index = 0;
        float feedback = 0.0f;
        float filterstore = 0.0f;
        float damp1 = 0.0f;
        float damp2 = 0.0f;

        void init(int size) {
            buffer.resize(size);
            reset();
        }

        void reset() {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
            index = 0;
            filterstore = 0.0f;
        }

        float process(float in) {
            float out = buffer[index];
            filterstore = out * damp2 + filterstore * damp1;
            buffer[index] = in + filterstore * feedback;
            index = (index + 1) % static_cast<int>(buffer.size());
            return out;
        }
    };

    struct AllpassFilter {
        std::vector<float> buffer;
        int index = 0;
        float feedback = 0.5f;

        void init(int size) {
            buffer.resize(size);
            reset();
        }

        void reset() {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
            index = 0;
        }

        float process(float in) {
            float bufout = buffer[index];
            float out = bufout - in;
            buffer[index] = in + bufout * feedback;
            index = (index + 1) % static_cast<int>(buffer.size());
            return out;
        }
    };

    std::vector<CombFilter> m_combFilters;
    std::vector<AllpassFilter> m_allpassFilters;
    std::vector<float> m_inputBuffer;
    std::vector<float> m_outputBuffer;
};

#endif // REVERB_H
