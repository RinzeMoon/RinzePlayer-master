// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EQUALIZERVISUALIZER_H
#define EQUALIZERVISUALIZER_H

#include "effectvisualizer.h"
#include "../effects/equalizer.h"
#include <QVector>
#include <QPainterPath>

class EqualizerVisualizer : public EffectVisualizer
{
    Q_OBJECT
public:
    explicit EqualizerVisualizer(QWidget *parent = nullptr);
    ~EqualizerVisualizer() override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawFrequencyLabels(QPainter &painter);
    void drawGrid(QPainter &painter);
    void drawResponseCurve(QPainter &painter);
    double calculateResponse(double frequency);
    double interpolate(double x, double x1, double y1, double x2, double y2);

    QVector<double> m_freqPoints;
    QVector<double> m_responseCurve;
};

#endif // EQUALIZERVISUALIZER_H
