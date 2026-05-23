// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUDIOEFFECT_H
#define AUDIOEFFECT_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVector>

class AudioEffect : public QObject {
    Q_OBJECT
public:
    enum class EffectType {
        EQ,
        Reverb,
        Compressor,
        Delay,
        Chorus,
        Distortion,
        Limiter,
        NoiseGate,
        PitchShift,
        Tremolo
    };

    struct Parameter {
        QString id;
        QString name;
        double minValue;
        double maxValue;
        double defaultValue;
        double currentValue;
        int type; // 0: float, 1: int, 2: bool
    };

    explicit AudioEffect(QObject *parent = nullptr);
    virtual ~AudioEffect();

    virtual EffectType type() const = 0;
    virtual QString name() const = 0;
    virtual QString description() const = 0;

    // 初始化效果器
    virtual bool init(int sampleRate, int channels) = 0;

    // 处理音频数据
    virtual void process(float *data, int frames) = 0;

    // 重置效果器状态
    virtual void reset() = 0;

    // 参数管理
    QVector<Parameter> parameters() const { return m_parameters; }
    void setParameter(const QString &id, double value);
    double getParameter(const QString &id) const;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

signals:
    void parameterChanged(const QString &id, double value);
    void enabledChanged(bool enabled);

protected:
    void addParameter(const QString &id, const QString &name,
                      double minValue, double maxValue, double defaultValue, int type = 0);
    virtual void onParameterChanged(const QString &id, double value) { Q_UNUSED(id); Q_UNUSED(value); }

    QVector<Parameter> m_parameters;
    bool m_enabled = true;
    int m_sampleRate = 44100;
    int m_channels = 2;
};

#endif // AUDIOEFFECT_H
