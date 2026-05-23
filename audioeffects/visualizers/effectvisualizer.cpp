// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "effectvisualizer.h"

EffectVisualizer::EffectVisualizer(QWidget *parent)
    : QWidget(parent)
    , m_effect(nullptr)
    , m_updateTimer(nullptr)
    , m_bgColor(20, 25, 35)
    , m_lineColor(0, 200, 150)
    , m_textColor(200, 200, 200)
{
    setMinimumHeight(120);
    setMaximumHeight(150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        update();
    });
    m_updateTimer->start(33); // ~30fps
}

EffectVisualizer::~EffectVisualizer()
{
}

void EffectVisualizer::setEffect(std::shared_ptr<AudioEffect> effect)
{
    m_effect = effect;
    update();
}

void EffectVisualizer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}
