// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fftanalyzer.h"
#include <QDebug>

FFTAnalyzer::FFTAnalyzer(QObject *parent)
    : QObject(parent)
{
}

FFTAnalyzer::~FFTAnalyzer()
{
}

void FFTAnalyzer::init(int fftSize, int sampleRate)
{
    m_sampleRate = sampleRate;
    
    // 确保是 2 的幂
    int n = 1;
    while (n < fftSize) n <<= 1;
    m_fftSize = n;
    
    m_inputBuffer.resize(m_fftSize, 0.0f);
    m_fftComplex.resize(m_fftSize);
    m_spectrum.resize(m_fftSize / 2);
    m_smoothedSpectrum.resize(m_fftSize / 2);
    m_prevSpectrum.resize(m_fftSize / 2, 0.0f);
    
    // 创建 Hann 窗口
    m_window.resize(m_fftSize);
    for (int i = 0; i < m_fftSize; ++i) {
        float x = static_cast<float>(i) / static_cast<float>(m_fftSize - 1);
        m_window[i] = 0.5f * (1.0f - cosf(2.0f * static_cast<float>(M_PI) * x));
    }
}

void FFTAnalyzer::fft(QVector<Complex>& data, bool invert)
{
    int n = data.size();
    
    // 位反转排序
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j >= bit; bit >>= 1) {
            j -= bit;
        }
        j += bit;
        if (i < j) {
            qSwap(data[i], data[j]);
        }
    }
    
    // FFT
    for (int len = 2; len <= n; len <<= 1) {
        float ang = 2 * static_cast<float>(M_PI) / len * (invert ? -1 : 1);
        Complex wlen(cosf(ang), sinf(ang));
        for (int i = 0; i < n; i += len) {
            Complex w(1);
            for (int j = 0; j < len / 2; j++) {
                Complex u = data[i + j];
                Complex v = data[i + j + len / 2] * w;
                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;
                w = w * wlen;
            }
        }
    }
    
    // 逆变换归一化
    if (invert) {
        float invN = 1.0f / static_cast<float>(n);
        for (int i = 0; i < n; i++) {
            data[i].re *= invN;
            data[i].im *= invN;
        }
    }
}

void FFTAnalyzer::analyze(const QVector<float>& audioData)
{
    // 复制音频数据到复数缓冲区
    int samplesToCopy = qMin(audioData.size(), m_fftSize);
    for (int i = 0; i < samplesToCopy; ++i) {
        m_fftComplex[i] = Complex(audioData[i] * m_window[i], 0.0f);
    }
    for (int i = samplesToCopy; i < m_fftSize; ++i) {
        m_fftComplex[i] = Complex(0.0f, 0.0f);
    }
    
    // 计算 FFT
    fft(m_fftComplex, false);
    
    // 转换到频谱
    computeSpectrum();
    smoothSpectrum();
    computeFrequencyBands();
    
    emit spectrumUpdated();
}

void FFTAnalyzer::computeSpectrum()
{
    // 计算幅度谱 - 直接原始幅度！
    
    for (int i = 0; i < m_fftSize / 2; ++i) {
        float re = m_fftComplex[i].re;
        float im = m_fftComplex[i].im;
        float magnitude = sqrtf(re * re + im * im);
        
        // 直接用 magnitude，放大200倍！
        m_spectrum[i] = magnitude * 200.0f;
    }
    
    // 找当前最大
    float maxVal = 0.0001f;
    for (float val : m_spectrum) {
        if (val > maxVal) maxVal = val;
    }
    
    // 归一化到0-1
    for (int i = 0; i < m_spectrum.size(); ++i) {
        m_spectrum[i] = qBound(0.0f, m_spectrum[i] / maxVal, 1.0f);
    }
}

void FFTAnalyzer::smoothSpectrum()
{
    for (int i = 0; i < m_spectrum.size(); ++i) {
        m_smoothedSpectrum[i] = m_smoothingFactor * m_prevSpectrum[i] +
                               (1.0f - m_smoothingFactor) * m_spectrum[i];
        m_prevSpectrum[i] = m_smoothedSpectrum[i];
    }
}

void FFTAnalyzer::computeFrequencyBands()
{
    int nyquist = m_sampleRate / 2;
    int bins = m_fftSize / 2;
    float binWidth = static_cast<float>(nyquist) / static_cast<float>(bins);
    
    int bassEnd = qMin(bins - 1, static_cast<int>(250.0f / binWidth));
    int midStart = bassEnd + 1;
    int midEnd = qMin(bins - 1, static_cast<int>(4000.0f / binWidth));
    int trebleStart = midEnd + 1;
    
    float bassSum = 0.0f, midSum = 0.0f, trebleSum = 0.0f;
    int bassCount = 0, midCount = 0, trebleCount = 0;
    
    for (int i = 0; i <= bassEnd; ++i) {
        bassSum += m_smoothedSpectrum[i];
        bassCount++;
    }
    for (int i = midStart; i <= midEnd; ++i) {
        midSum += m_smoothedSpectrum[i];
        midCount++;
    }
    for (int i = trebleStart; i < bins; ++i) {
        trebleSum += m_smoothedSpectrum[i];
        trebleCount++;
    }
    
    m_bassLevel = (bassCount > 0) ? bassSum / bassCount : 0.0f;
    m_midLevel = (midCount > 0) ? midSum / midCount : 0.0f;
    m_trebleLevel = (trebleCount > 0) ? trebleSum / trebleCount : 0.0f;
    
    m_bassLevel = qBound(0.0f, m_bassLevel, 1.0f);
    m_midLevel = qBound(0.0f, m_midLevel, 1.0f);
    m_trebleLevel = qBound(0.0f, m_trebleLevel, 1.0f);
}
