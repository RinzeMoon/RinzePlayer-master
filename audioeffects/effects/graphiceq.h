// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GRAPHICEQ_H
#define GRAPHICEQ_H

#include "../audioeffect.h"
#include <QVector>

class GraphicEQ : public AudioEffect
{
    Q_OBJECT
public:
    explicit GraphicEQ(QObject *parent = nullptr);
    ~GraphicEQ() override;

    EffectType type() const override { return static_cast<EffectType>(1001); } // 新类型
    QString name() const override { return "Graphic EQ"; }
    QString description() const override { return "10-band graphic equalizer"; }

    bool init(int sampleRate, int channels) override;
    void process(float *data, int frames) override;
    void reset() override;

    // 10个频率点
    static QVector<double> frequencies();

protected:
    void onParameterChanged(const QString &id, double value) override;

private:
    struct BiquadFilter {
        float b0, b1, b2, a1, a2;
        float z1[2], z2[2];

        void reset() {
            z1[0] = z1[1] = 0.0f;
            z2[0] = z2[1] = 0.0f;
        }

        float process(float in, int ch) {
            float out = in * b0 + z1[ch];
            z1[ch] = in * b1 + z2[ch] - a1 * out;
            z2[ch] = in * b2 - a2 * out;
            return out;
        }
    };

    void designFilters();

    QVector<BiquadFilter> m_filters;
    QVector<double> m_freqs;
    bool m_dirty;
};

#endif // GRAPHICEQ_H
