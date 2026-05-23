// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EFFECTVISUALIZER_H
#define EFFECTVISUALIZER_H

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include "../audioeffect.h"

class EffectVisualizer : public QWidget
{
    Q_OBJECT
public:
    explicit EffectVisualizer(QWidget *parent = nullptr);
    virtual ~EffectVisualizer();

    void setEffect(std::shared_ptr<AudioEffect> effect);
    std::shared_ptr<AudioEffect> effect() const { return m_effect; }

protected:
    void paintEvent(QPaintEvent *event) override = 0;
    void resizeEvent(QResizeEvent *event) override;

    std::shared_ptr<AudioEffect> m_effect;
    QTimer *m_updateTimer;
    QColor m_bgColor;
    QColor m_lineColor;
    QColor m_textColor;
};

#endif // EFFECTVISUALIZER_H
