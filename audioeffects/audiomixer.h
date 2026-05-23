// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUDIOMIXER_H
#define AUDIOMIXER_H

#include "audioeffect.h"
#include <QObject>
#include <QVector>
#include <memory>

class AudioMixer : public QObject {
    Q_OBJECT
public:
    explicit AudioMixer(QObject *parent = nullptr);
    ~AudioMixer();

    // 初始化混音器
    bool init(int sampleRate, int channels);

    // 处理音频数据
    void process(float *data, int frames);

    // 效果器管理
    void addEffect(std::shared_ptr<AudioEffect> effect);
    void removeEffect(int index);
    void moveEffect(int fromIndex, int toIndex);
    void clearEffects();

    QVector<std::shared_ptr<AudioEffect>> effects() const { return m_effects; }
    std::shared_ptr<AudioEffect> effect(int index) const;
    int effectCount() const { return m_effects.size(); }

    // 全局音量
    double masterVolume() const { return m_masterVolume; }
    void setMasterVolume(double volume);

    // 重置所有效果器
    void resetAll();

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

signals:
    void effectAdded(int index);
    void effectRemoved(int index);
    void effectsReordered();
    void masterVolumeChanged(double volume);
    void enabledChanged(bool enabled);

private:
    QVector<std::shared_ptr<AudioEffect>> m_effects;
    double m_masterVolume = 1.0;
    bool m_enabled = true;
    int m_sampleRate = 44100;
    int m_channels = 2;
};

#endif // AUDIOMIXER_H
