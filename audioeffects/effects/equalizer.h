// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EQUALIZER_H
#define EQUALIZER_H

#include "../audioeffect.h"
#include <vector>

class Equalizer : public AudioEffect {
    Q_OBJECT
public:
    explicit Equalizer(QObject *parent = nullptr);
    ~Equalizer() override;

    EffectType type() const override { return EffectType::EQ; }
    QString name() const override { return tr("Equalizer"); }
    QString description() const override { return tr("3-band parametric equalizer"); }

    bool init(int sampleRate, int channels) override;
    void process(float *data, int frames) override;
    void reset() override;

protected:
    void onParameterChanged(const QString &id, double value) override;

private:
    struct BiquadFilter {
        float b0, b1, b2, a1, a2;
        float z1[2], z2[2]; // delay lines for 2 channels

        BiquadFilter() {
            reset();
        }

        void reset() {
            b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
            a1 = 0.0f; a2 = 0.0f;
            z1[0] = z1[1] = 0.0f;
            z2[0] = z2[1] = 0.0f;
        }

        float process(float in, int channel) {
            float out = in * b0 + z1[channel];
            z1[channel] = in * b1 + z2[channel] - a1 * out;
            z2[channel] = in * b2 - a2 * out;
            return out;
        }
    };

    void designPeakingFilter(BiquadFilter &filter, double freq, double gain, double q);

    std::vector<BiquadFilter> m_filters;
    bool m_dirty = true;
};

#endif // EQUALIZER_H
