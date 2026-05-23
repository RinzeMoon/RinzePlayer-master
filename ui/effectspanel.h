// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EFFECTSPANEL_H
#define EFFECTSPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QScrollArea>
#include <QGroupBox>
#include "../audioeffects/audiomixer.h"
#include "../audioeffects/effectfactory.h"
#include "../audioeffects/visualizers/effectvisualizer.h"

class EffectsPanel : public QWidget {
    Q_OBJECT
public:
    explicit EffectsPanel(AudioMixer *mixer, QWidget *parent = nullptr);
    ~EffectsPanel() override;

private slots:
    void addEffect();
    void removeEffect(int index);
    void updateEffectChain();

private:
    void setupUI();
    QWidget* createEffectWidget(std::shared_ptr<AudioEffect> effect, int index);

    AudioMixer *m_mixer = nullptr;
    QVBoxLayout *m_mainLayout = nullptr;
    QVBoxLayout *m_effectChainLayout = nullptr;
    QComboBox *m_effectSelector = nullptr;
    QPushButton *m_addButton = nullptr;
    QScrollArea *m_scrollArea = nullptr;
};

#endif // EFFECTSPANEL_H
