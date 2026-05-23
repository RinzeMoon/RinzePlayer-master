// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef REVERBVISUALIZER_H
#define REVERBVISUALIZER_H

#include "effectvisualizer.h"
#include <QVector>
#include <QElapsedTimer>
#include <QPainterPath>

class ReverbVisualizer : public EffectVisualizer
{
    Q_OBJECT
public:
    explicit ReverbVisualizer(QWidget *parent = nullptr);
    ~ReverbVisualizer() override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawWaveform(QPainter &painter);
    void drawDecayCurve(QPainter &painter);
    void updateWaveform();

    QVector<float> m_waveform;
    QElapsedTimer m_timer;
    int m_sampleRate;
    float m_phase;
};

#endif // REVERBVISUALIZER_H
