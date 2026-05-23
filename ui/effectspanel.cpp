// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "effectspanel.h"
#include <QCheckBox>
#include "../audioeffects/visualizers/equalizervisualizer.h"
#include "../audioeffects/visualizers/reverbvisualizer.h"
#include "../audioeffects/visualizers/graphiceqvisualizer.h"

EffectsPanel::EffectsPanel(AudioMixer *mixer, QWidget *parent)
    : QWidget(parent), m_mixer(mixer) {
    setupUI();
    updateEffectChain();
}

EffectsPanel::~EffectsPanel() {
}

void EffectsPanel::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    
    // 标题
    QLabel *titleLabel = new QLabel(tr("Audio Effects"));
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    m_mainLayout->addWidget(titleLabel);
    
    // 添加效果器区域
    QHBoxLayout *addLayout = new QHBoxLayout();
    m_effectSelector = new QComboBox();
    
    for (const auto &name : EffectFactory::availableEffects()) {
        m_effectSelector->addItem(name);
    }
    
    m_addButton = new QPushButton(tr("Add Effect"));
    connect(m_addButton, &QPushButton::clicked, this, &EffectsPanel::addEffect);
    
    addLayout->addWidget(m_effectSelector);
    addLayout->addWidget(m_addButton);
    m_mainLayout->addLayout(addLayout);
    
    // 效果器链显示区域
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    QWidget *chainWidget = new QWidget();
    m_effectChainLayout = new QVBoxLayout(chainWidget);
    m_effectChainLayout->addStretch();
    m_scrollArea->setWidget(chainWidget);
    
    m_mainLayout->addWidget(m_scrollArea);
}

void EffectsPanel::addEffect() {
    QString effectName = m_effectSelector->currentText();
    auto effect = EffectFactory::createEffectByName(effectName);
    if (effect) {
        m_mixer->addEffect(effect);
        updateEffectChain();
    }
}

void EffectsPanel::removeEffect(int index) {
    m_mixer->removeEffect(index);
    updateEffectChain();
}

void EffectsPanel::updateEffectChain() {
    // 清除现有widget
    QLayoutItem *item;
    while ((item = m_effectChainLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    
    // 添加效果器widget
    for (int i = 0; i < m_mixer->effectCount(); ++i) {
        auto effect = m_mixer->effect(i);
        auto effectWidget = createEffectWidget(effect, i);
        m_effectChainLayout->insertWidget(m_effectChainLayout->count() - 1, effectWidget);
    }
    
    m_effectChainLayout->addStretch();
}

QWidget* EffectsPanel::createEffectWidget(std::shared_ptr<AudioEffect> effect, int index)
{
    QGroupBox *groupBox = new QGroupBox(effect->name());
    QVBoxLayout *groupLayout = new QVBoxLayout(groupBox);

    // 可视化区域
    EffectVisualizer *visualizer = nullptr;
    if (effect->type() == AudioEffect::EffectType::EQ) {
        EqualizerVisualizer *eqVis = new EqualizerVisualizer(groupBox);
        eqVis->setEffect(effect);
        visualizer = eqVis;
    } else if (effect->type() == static_cast<AudioEffect::EffectType>(1001)) { // Graphic EQ
        GraphicEQVisualizer *gfxVis = new GraphicEQVisualizer(groupBox);
        gfxVis->setEffect(effect);
        visualizer = gfxVis;
    } else if (effect->type() == AudioEffect::EffectType::Reverb) {
        ReverbVisualizer *revVis = new ReverbVisualizer(groupBox);
        revVis->setEffect(effect);
        visualizer = revVis;
    }

    if (visualizer) {
        groupLayout->addWidget(visualizer);
    }

    // 启用/禁用复选框
    QCheckBox *enabledCheck = new QCheckBox(tr("Enabled"));
    enabledCheck->setChecked(effect->enabled());
    connect(enabledCheck, &QCheckBox::toggled, [effect](bool checked) {
        effect->setEnabled(checked);
    });
    groupLayout->addWidget(enabledCheck);

    // 参数控制
    for (const auto &param : effect->parameters()) {
        QHBoxLayout *paramLayout = new QHBoxLayout();

        QLabel *label = new QLabel(param.name);
        paramLayout->addWidget(label);

        QSlider *slider = new QSlider(Qt::Horizontal);
        slider->setMinimum(static_cast<int>(param.minValue * 100));
        slider->setMaximum(static_cast<int>(param.maxValue * 100));
        slider->setValue(static_cast<int>(param.currentValue * 100));

        QLabel *valueLabel = new QLabel(QString::number(param.currentValue, 'f', 2));

        QString paramId = param.id;
        connect(slider, &QSlider::valueChanged, [effect, paramId, valueLabel](int value) {
            double realValue = value / 100.0;
            effect->setParameter(paramId, realValue);
            valueLabel->setText(QString::number(realValue, 'f', 2));
        });

        paramLayout->addWidget(slider);
        paramLayout->addWidget(valueLabel);
        groupLayout->addLayout(paramLayout);
    }

    // 删除按钮
    QPushButton *deleteButton = new QPushButton(tr("Remove"));
    connect(deleteButton, &QPushButton::clicked, [this, index]() {
        removeEffect(index);
    });
    groupLayout->addWidget(deleteButton);

    return groupBox;
}
