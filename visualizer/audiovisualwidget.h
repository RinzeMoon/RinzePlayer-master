// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUDIOVISUALWIDGET_H
#define AUDIOVISUALWIDGET_H

#include "fftanalyzer.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QTimer>
#include <QElapsedTimer>
#include <QSizePolicy>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <cmath>

class AudioVisualWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
public:
    enum VisualizationMode {
        SpectrumBars,    // 频谱柱状图（文章中最常见的）
        Waveform,        // 波形图（文章中提到的）
        ParticleSystem,  // 粒子系统
        Combined         // 组合模式
    };

    explicit AudioVisualWidget(QWidget *parent = nullptr);
    ~AudioVisualWidget();

    void setVisualizationMode(VisualizationMode mode);
    VisualizationMode visualizationMode() const { return m_mode; }

public slots:
    void setAudioData(const QVector<float>& audioData, int sampleRate);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void initParticleSystem();
    void updateParticles();
    void renderSpectrumWave();
    void renderParticles();
    void renderCombined();

    FFTAnalyzer* m_fftAnalyzer = nullptr;
    QTimer* m_updateTimer = nullptr;
    QElapsedTimer m_elapsedTimer;

    VisualizationMode m_mode = SpectrumBars;

    QOpenGLShaderProgram* m_spectrumProgram = nullptr;
    QOpenGLShaderProgram* m_particleProgram = nullptr;

    GLuint m_spectrumVAO = 0;
    GLuint m_spectrumVBO = 0;

    GLuint m_particleVAO = 0;
    GLuint m_particleVBO = 0;

    struct Particle {
        float x, y;
        float vx, vy;
        float life;
        float size;
    };

    QVector<Particle> m_particles;
    QVector<float> m_particleData;

    float m_spectrumData[256] = {0.0f};
    float m_spectrumTarget[256] = {0.0f}; // 用于平滑
    float m_audioWaveform[512] = {0.0f};
    float m_bassLevel = 0.0f;
    float m_midLevel = 0.0f;
    float m_trebleLevel = 0.0f;
    float m_bassTarget = 0.0f;
    float m_midTarget = 0.0f;
    float m_trebleTarget = 0.0f;
    float m_time = 0.0f;
    
    std::chrono::time_point<std::chrono::high_resolution_clock> m_lastAudioTime;
    bool m_initialized = false;
    int m_hoveredBarIndex = -1; // 当前hover的柱子
    float m_waveformScale = 2.0f; // 波形水平缩放系数（默认更大一些）
};

#endif // AUDIOVISUALWIDGET_H
