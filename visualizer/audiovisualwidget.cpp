// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audiovisualwidget.h"
#include <QFile>
#include <QDebug>
#include <random>
#include <QMouseEvent>
#include <QToolTip>

AudioVisualWidget::AudioVisualWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    // 完美的 SizePolicy！
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    policy.setHorizontalStretch(1);
    policy.setVerticalStretch(0);
    setSizePolicy(policy);
    
    m_fftAnalyzer = new FFTAnalyzer(this);
    m_fftAnalyzer->init(2048, 44100);
    
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        updateParticles();
        update();
    });
    m_updateTimer->start(16);
    
    m_elapsedTimer.start();
    
    // 初始化 lastAudioTime 为很久以前！
    m_lastAudioTime = std::chrono::high_resolution_clock::now() - std::chrono::seconds(10);
}

AudioVisualWidget::~AudioVisualWidget()
{
    makeCurrent();
    
    if (m_spectrumVAO) glDeleteVertexArrays(1, &m_spectrumVAO);
    if (m_spectrumVBO) glDeleteBuffers(1, &m_spectrumVBO);
    if (m_particleVAO) glDeleteVertexArrays(1, &m_particleVAO);
    if (m_particleVBO) glDeleteBuffers(1, &m_particleVBO);
    
    delete m_spectrumProgram;
    delete m_particleProgram;
    
    doneCurrent();
}

void AudioVisualWidget::setVisualizationMode(VisualizationMode mode)
{
    m_mode = mode;
    update();
}

void AudioVisualWidget::setAudioData(const QVector<float>& audioData, int sampleRate)
{
    static int lastSampleRate = -1;
    if (lastSampleRate != sampleRate) {
        m_fftAnalyzer->init(2048, sampleRate);
        lastSampleRate = sampleRate;
    }
    m_fftAnalyzer->analyze(audioData);
    
    const QVector<float>& spectrum = m_fftAnalyzer->spectrum();
    
    // 更新最后音频的时间
    m_lastAudioTime = std::chrono::high_resolution_clock::now();
    
    // Debug：打印音频数据和频谱的最大值！
    static int cnt = 0;
    if (cnt++ % 30 == 0) { // 每秒打印一次
        float maxAudio = 0.0f;
        for (float s : audioData) if (fabs(s) > maxAudio) maxAudio = fabs(s);
        
        float maxSpec = 0.0f;
        for (int i = 0; i < 256; ++i) if (spectrum[i] > maxSpec) maxSpec = spectrum[i];
        
        qDebug() << "Audio max:" << maxAudio << " | Spectrum max:" << maxSpec;
    }
    
    // 直接复制 raw spectrum！
    for (int i = 0; i < 256 && i < spectrum.size(); ++i) {
        m_spectrumTarget[i] = spectrum[i];
    }
    
    for (int i = 0; i < 512 && i < audioData.size(); ++i) {
        m_audioWaveform[i] = audioData[i];
    }
    
    m_bassTarget = m_fftAnalyzer->bassLevel();
    m_midTarget = m_fftAnalyzer->midLevel();
    m_trebleTarget = m_fftAnalyzer->trebleLevel();
}

void AudioVisualWidget::initializeGL()
{
    initializeOpenGLFunctions();
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE); // 重要！Core Profile 必须启用！
    
    // ==== 经典频谱柱状图着色器（文章中最常见的）
    const char* spectrumVert = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        out vec2 vTexCoord;
        void main() { vTexCoord = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }
    )";
    const char* spectrumFrag = R"(
        #version 330 core
        in vec2 vTexCoord;
        out vec4 FragColor;
        uniform float uSpectrum[128]; // 128个频带
        uniform float uBassLevel;
        uniform float uMidLevel;
        uniform float uTrebleLevel;
        
        void main() {
            vec2 st = vTexCoord;
            
            // 经典频谱柱状图 - 文章中最常见的风格！
            float barVisible = 0.0;
            int numBands = 64; // 64个柱子
            
            for(int i = 0; i < numBands; i++) {
                float barX = float(i) / float(numBands) + 0.5 / float(numBands);
                float barWidth = 0.8 / float(numBands);
                
                // 用频谱数据
                float bandEnergy = uSpectrum[i * 2];
                float barHeight = 0.05 + bandEnergy * 0.4;
                
                // 检查当前像素是否在柱子里
                float dx = abs(st.x - barX);
                if(dx < barWidth * 0.5 && st.y > 1.0 - barHeight && st.y < 1.0) {
                    barVisible = 1.0;
                }
            }
            
            // 背景纯黑，柱子彩色渐变
            if(barVisible > 0.5) {
                float r = 0.0 + 0.8 * st.x;
                float g = 0.3 + 0.7 * (1.0 - st.y);
                float b = 1.0 - 0.5 * st.x;
                FragColor = vec4(r, g, b, 1.0);
            } else {
                FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            }
        }
    )";
    
    m_spectrumProgram = new QOpenGLShaderProgram(this);
    m_spectrumProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, spectrumVert);
    m_spectrumProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, spectrumFrag);
    if (m_spectrumProgram->link()) {
        qDebug() << "✅ Spectrum shader linked successfully!";
    } else {
        qDebug() << "❌ Spectrum shader link error:" << m_spectrumProgram->log();
    }
    
    // ==== 粒子着色器（硬编码！）
    const char* particleVert = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        out vec4 vColor;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            gl_PointSize = 50.0;
            vColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
    )";
    const char* particleFrag = R"(
        #version 330 core
        in vec4 vColor;
        out vec4 FragColor;
        void main() {
            float dist = distance(gl_PointCoord, vec2(0.5, 0.5));
            if (dist > 0.5) discard;
            FragColor = vColor;
        }
    )";
    
    m_particleProgram = new QOpenGLShaderProgram(this);
    m_particleProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, particleVert);
    m_particleProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, particleFrag);
    if (m_particleProgram->link()) {
        qDebug() << "✅ Particle shader linked successfully!";
    } else {
        qDebug() << "❌ Particle shader link error:" << m_particleProgram->log();
    }
    
    float quadVertices[] = {
        -1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f
    };
    
    glGenVertexArrays(1, &m_spectrumVAO);
    glGenBuffers(1, &m_spectrumVBO);
    glBindVertexArray(m_spectrumVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_spectrumVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    
    initParticleSystem();
    
    m_initialized = true;
}

void AudioVisualWidget::initParticleSystem()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distPos(-1.0f, 1.0f);
    std::uniform_real_distribution<float> distVel(-0.2f, 0.2f);
    std::uniform_real_distribution<float> distSize(5.0f, 20.0f); // 更大！
    
    const int particleCount = 1000; // 更多粒子！
    m_particles.resize(particleCount);
    m_particleData.resize(particleCount * 6);
    
    for (int i = 0; i < particleCount; ++i) {
        m_particles[i].x = distPos(gen);
        m_particles[i].y = distPos(gen);
        m_particles[i].vx = distVel(gen);
        m_particles[i].vy = distVel(gen);
        m_particles[i].life = 1.0f;
        m_particles[i].size = distSize(gen);
        
        int idx = i * 6;
        m_particleData[idx + 0] = m_particles[i].x;
        m_particleData[idx + 1] = m_particles[i].y;
        m_particleData[idx + 2] = m_particles[i].vx;
        m_particleData[idx + 3] = m_particles[i].vy;
        m_particleData[idx + 4] = m_particles[i].life;
        m_particleData[idx + 5] = m_particles[i].size;
    }
    
    glGenVertexArrays(1, &m_particleVAO);
    glGenBuffers(1, &m_particleVBO);
    glBindVertexArray(m_particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_particleVBO);
    glBufferData(GL_ARRAY_BUFFER, m_particleData.size() * sizeof(float), m_particleData.data(), GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(3);
    
    glBindVertexArray(0);
}

void AudioVisualWidget::updateParticles()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distPos(-1.0f, 1.0f);
    std::uniform_real_distribution<float> distVel(-0.3f, 0.3f);
    std::uniform_real_distribution<float> distSize(3.0f, 12.0f);
    
    float energy = m_bassLevel * 0.5f + m_midLevel * 0.3f + m_trebleLevel * 0.2f;
    
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = now - m_lastAudioTime;
    float ms = elapsed.count() * 1000.0f;
    
    for (int i = 0; i < m_particles.size(); ++i) {
        // 只有有新音频数据时才动！没有就完全不动！
        if(ms < 150.0f) { // 150ms内有数据才动！
            float moveFactor = 1.0f + energy * 2.0f;
            
            m_particles[i].x += m_particles[i].vx * 0.01f * moveFactor;
            m_particles[i].y += m_particles[i].vy * 0.01f * moveFactor;
            
            m_particles[i].life -= 0.005f;
            
            if (m_particles[i].life <= 0.0f || 
                m_particles[i].x < -1.2f || m_particles[i].x > 1.2f ||
                m_particles[i].y < -1.2f || m_particles[i].y > 1.2f) {
                m_particles[i].x = distPos(gen);
                m_particles[i].y = distPos(gen);
                m_particles[i].vx = distVel(gen);
                m_particles[i].vy = distVel(gen);
                m_particles[i].life = 1.0f;
                m_particles[i].size = distSize(gen) * (1.0f + energy);
            }
        }
        
        int idx = i * 6;
        m_particleData[idx + 0] = m_particles[i].x;
        m_particleData[idx + 1] = m_particles[i].y;
        m_particleData[idx + 2] = m_particles[i].vx;
        m_particleData[idx + 3] = m_particles[i].vy;
        m_particleData[idx + 4] = m_particles[i].life;
        m_particleData[idx + 5] = m_particles[i].size;
    }
}

void AudioVisualWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void AudioVisualWidget::paintGL()
{
    if (!m_initialized) return;
    
    m_time = m_elapsedTimer.elapsed() / 1000.0f;
    
    // === 检查是否超过100ms没收到音频！
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = now - m_lastAudioTime;
    float ms = elapsed.count() * 1000.0f;
    
    float smoothing = 0.2f;
    
    if (ms > 100.0f) {
        // 超过100ms没数据，开始衰减！
        float decay = 0.9f;
        for (int i = 0; i < 256; i++) {
            m_spectrumData[i] *= decay;
            m_spectrumTarget[i] = m_spectrumData[i];
        }
        
        m_bassLevel *= decay;
        m_midLevel *= decay;
        m_trebleLevel *= decay;
        m_bassTarget = m_bassLevel;
        m_midTarget = m_midLevel;
        m_trebleTarget = m_trebleLevel;
        
        for (int i = 0; i < 512; i++) {
            m_audioWaveform[i] *= decay;
        }
    } else {
        // 有数据，正常平滑！
        for (int i = 0; i < 256; i++) {
            m_spectrumData[i] = m_spectrumData[i] * (1 - smoothing) + m_spectrumTarget[i] * smoothing;
        }
        
        m_bassLevel = m_bassLevel * (1 - smoothing) + m_bassTarget * smoothing;
        m_midLevel = m_midLevel * (1 - smoothing) + m_midTarget * smoothing;
        m_trebleLevel = m_trebleLevel * (1 - smoothing) + m_trebleTarget * smoothing;
    }
    
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (m_mode == ParticleSystem) {
        updateParticles();
        
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        
        for (int i = 0; i < m_particles.size(); ++i) {
            float x = (m_particles[i].x + 1.0f) * 0.5f * width();
            float y = (1.0f - m_particles[i].y) * 0.5f * height();
            
            float bandIdx = float(i) / float(m_particles.size());
            float r, g, b;
            float size;
            
            if(bandIdx < 0.33f) {
                r = 0.8f + 0.2f * m_bassLevel;
                g = 0.2f + 0.1f * m_midLevel;
                b = 0.2f + 0.1f * m_trebleLevel;
                size = (5.0f + m_bassLevel * 10.0f) * 0.8f;
            } else if(bandIdx < 0.66f) {
                r = 0.2f + 0.1f * m_bassLevel;
                g = 0.8f + 0.2f * m_midLevel;
                b = 0.2f + 0.1f * m_trebleLevel;
                size = (4.0f + m_midLevel * 8.0f) * 0.8f;
            } else {
                r = 0.2f + 0.1f * m_bassLevel;
                g = 0.2f + 0.1f * m_midLevel;
                b = 0.8f + 0.2f * m_trebleLevel;
                size = (3.0f + m_trebleLevel * 6.0f) * 0.8f;
            }
            
            int alpha = static_cast<int>(m_particles[i].life * 255);
            
            painter.setBrush(QColor(
                static_cast<int>(r * 255),
                static_cast<int>(g * 255),
                static_cast<int>(b * 255),
                alpha));
            
            painter.drawEllipse(QPointF(x, y), size, size);
        }
        
        painter.end();
    } else if (m_mode == Waveform) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(100, 200, 255), 2));
        
        painter.drawLine(0, height() / 2, width(), height() / 2);
        
        QPainterPath path;
        path.moveTo(0, height() / 2);
        
        for (int i = 0; i < 512; ++i) {
            float x = (float(i) / 512.0f) * width() * m_waveformScale; // 使用滚轮控制的缩放系数！
            float y = height() / 2 + m_audioWaveform[i] * height() * 5.0f; // 大幅增加波形高度！
            
            if (i == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }
        }
        
        painter.setPen(QPen(QColor(100, 200, 255), 2));
        painter.drawPath(path);
        painter.end();
    } else if (m_mode == SpectrumBars) {
        // === EQ风格频谱图（画标签！！！
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        int labelHeight = 30; // 标签高度
        int graphHeight = height() - labelHeight - 10;
        
        // 1. 画背景和边框
        painter.setPen(QPen(QColor(80, 80, 100), 1));
        painter.setBrush(QColor(20, 20, 30));
        painter.drawRect(5, 5, width() - 10, graphHeight);
        
        // 2. 画网格线
        painter.setPen(QPen(QColor(40, 40, 50), 1));
        for (int i = 0; i <= 4; i++) {
            float y = 5 + (graphHeight * (float(i) / 4.0f));
            painter.drawLine(5, y, width() - 5, y);
        }
        
        // 3. 画频谱条
        int numBands = 128;
        float barWidth = float(width() - 20) / numBands;
        float barGap = 1;
        
        for (int i = 0; i < numBands; i++) {
            float x = 8 + i * barWidth;
            float spectrumVal = m_spectrumData[i];
            float barHeight = spectrumVal * graphHeight * 0.9f;
            
            // 颜色渐变：低音红，高音蓝
            float hue = (float(i) / numBands) * 0.7f;
            QColor color;
            
            if (i == m_hoveredBarIndex) {
                // 高亮：更亮更饱和！
                color.setHsvF(hue, 1.0f, 1.0f, 1.0f);
            } else {
                color.setHsvF(hue, 0.8f, 0.8f, 1.0f);
            }
            
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawRect(x, graphHeight + 5 - barHeight, barWidth - barGap, barHeight);
        }
        
        // 4. 画Hz标签
        painter.setPen(QColor(180, 180, 200));
        QFont font = painter.font();
        font.setPointSize(9);
        painter.setFont(font);
        
        float freqLabels[] = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
        int numLabels = sizeof(freqLabels) / sizeof(float);
        float nyquist = 22050.0f; // 假设采样率
        
        for (int i = 0; i < numLabels; i++) {
            float f = freqLabels[i];
            if (f > nyquist) f = nyquist;
            
            float logMin = log10(20.0f);
            float logMax = log10(nyquist);
            float logF = log10(f);
            float normX = (logF - logMin) / (logMax - logMin);
            
            float x = 5 + normX * (width() - 10);
            
            QString text;
            if (f >= 1000) {
                text = QString("%1k").arg(f / 1000.0f, 0, 'f', 0);
            } else {
                text = QString("%1").arg(f, 0, 'f', 0);
            }
            
            QFontMetrics fm(font);
            int textWidth = fm.horizontalAdvance(text);
            
            // 对齐标签
            float textX = x - textWidth / 2;
            float textY = height() - 5;
            
            painter.drawText(QPointF(textX, textY), text);
        }
        
        // 画垂直线
        painter.setPen(QPen(QColor(60, 60, 80), 1));
        for (int i = 0; i < numLabels; i++) {
            float f = freqLabels[i];
            if (f > nyquist) f = nyquist;
            float logMin = log10(20.0f);
            float logMax = log10(nyquist);
            float logF = log10(f);
            float normX = (logF - logMin) / (logMax - logMin);
            float x = 5 + normX * (width() - 10);
            painter.drawLine(x, 5, x, graphHeight + 5);
        }
        
        painter.end();
    } else {
        renderSpectrumWave();
        // 组合模式暂时不做
    }
}

void AudioVisualWidget::renderSpectrumWave()
{
    if (!m_spectrumProgram) return;
    
    m_spectrumProgram->bind();
    
    m_spectrumProgram->setUniformValue("uBassLevel", m_bassLevel);
    m_spectrumProgram->setUniformValue("uMidLevel", m_midLevel);
    m_spectrumProgram->setUniformValue("uTrebleLevel", m_trebleLevel);
    
    GLint spectrumLoc = m_spectrumProgram->uniformLocation("uSpectrum");
    glUniform1fv(spectrumLoc, 128, m_spectrumData);
    
    glBindVertexArray(m_spectrumVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
    
    m_spectrumProgram->release();
}

void AudioVisualWidget::renderParticles()
{
    if (!m_particleProgram) return;
    
    // ==== 测试版本：画3个大的测试点！（只需要aPos！）
    static float testPoints[] = {
        -0.8f,  0.0f,
         0.0f,  0.5f,
         0.8f, -0.5f
    };
    static GLuint testVBO = 0;
    static GLuint testVAO = 0;
    
    if (testVAO == 0) {
        glGenVertexArrays(1, &testVAO);
        glGenBuffers(1, &testVBO);
        glBindVertexArray(testVAO);
        glBindBuffer(GL_ARRAY_BUFFER, testVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(testPoints), testPoints, GL_STATIC_DRAW);
        
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }
    
    m_particleProgram->bind();
    
    glBindVertexArray(testVAO);
    glDrawArrays(GL_POINTS, 0, 3);
    glBindVertexArray(0);
    
    m_particleProgram->release();
}

void AudioVisualWidget::renderCombined()
{
    renderSpectrumWave();
    renderParticles();
}

void AudioVisualWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_mode != SpectrumBars) {
        QToolTip::hideText();
        m_hoveredBarIndex = -1;
        return;
    }
    
    int labelHeight = 30;
    int graphHeight = height() - labelHeight - 10;
    int numBands = 128;
    float barWidth = float(width() - 20) / numBands;
    
    QPoint pos = event->pos();
    
    if (pos.y() >= 5 && pos.y() <= 5 + graphHeight &&
        pos.x() >= 8 && pos.x() <= 8 + numBands * barWidth) {
        
        int barIndex = int((pos.x() - 8) / barWidth);
        if (barIndex >= 0 && barIndex < numBands) {
            
            m_hoveredBarIndex = barIndex;
            
            float spectrumVal = m_spectrumData[barIndex];
            float db = 20.0f * log10f(std::max(spectrumVal, 0.0001f));
            
            float nyquist = 22050.0f;
            float freqMin = 20.0f;
            float freqMax = nyquist;
            
            float logMin = log10f(freqMin);
            float logMax = log10f(freqMax);
            
            float normFreq = float(barIndex) / float(numBands - 1);
            float freq = powf(10, logMin + normFreq * (logMax - logMin));
            
            QString freqText;
            if (freq >= 1000.0f) {
                freqText = QString("%1 kHz").arg(freq / 1000.0f, 0, 'f', 1);
            } else {
                freqText = QString("%1 Hz").arg(freq, 0, 'f', 0);
            }
            
            QString dbText = QString("%.1f dB").arg(db);
            
            QString tooltip = QString("%1\n%2").arg(freqText).arg(dbText);
            
            // 计算tooltip宽度，把它放在鼠标左侧！
            QFontMetrics fm(font());
            int textWidth = fm.horizontalAdvance(freqText);
            int textWidth2 = fm.horizontalAdvance(dbText);
            int maxWidth = std::max(textWidth, textWidth2) + 30;
            
            QPoint globalPos = mapToGlobal(pos);
            QPoint tooltipPos = globalPos - QPoint(maxWidth + 10, 0);
            
            QToolTip::showText(tooltipPos, tooltip, this);
            update();
        }
    } else {
        QToolTip::hideText();
        m_hoveredBarIndex = -1;
    }
}

void AudioVisualWidget::wheelEvent(QWheelEvent* event) {
    if (m_mode != Waveform) {
        event->ignore();
        return;
    }
    
    // 向上滚放大，向下滚缩小
    float delta = event->angleDelta().y() / 120.0f;
    float scaleFactor = 1.1f;
    
    if (delta > 0) {
        m_waveformScale *= scaleFactor;
    } else {
        m_waveformScale /= scaleFactor;
    }
    
    // 限制缩放范围：0.1 到 10.0
    m_waveformScale = std::max(0.1f, std::min(10.0f, m_waveformScale));
    update();
    event->accept();
}
