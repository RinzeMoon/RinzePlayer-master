// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GRAPHICEQVISUALIZER_H
#define GRAPHICEQVISUALIZER_H

#include "effectvisualizer.h"
#include "../effects/graphiceq.h"
#include <QVector>

class GraphicEQVisualizer : public EffectVisualizer
{
    Q_OBJECT
public:
    explicit GraphicEQVisualizer(QWidget *parent = nullptr);
    ~GraphicEQVisualizer() override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawGrid(QPainter &painter);
    void drawBars(QPainter &painter);
    void drawLabels(QPainter &painter);

    QVector<double> m_freqs;
    QColor m_barColor;
    QColor m_barColorBright;
};

#endif // GRAPHICEQVISUALIZER_H
