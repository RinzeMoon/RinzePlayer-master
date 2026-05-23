// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FFTANALYZER_H
#define FFTANALYZER_H

#include <QObject>
#include <QVector>
#include <cmath>
#include <algorithm>
#include <numeric>

struct Complex {
    float re, im;
    Complex(float r = 0, float i = 0) : re(r), im(i) {}
    Complex operator+(const Complex& o) const { return Complex(re+o.re, im+o.im); }
    Complex operator-(const Complex& o) const { return Complex(re-o.re, im-o.im); }
    Complex operator*(const Complex& o) const {
        return Complex(re*o.re - im*o.im, re*o.im + im*o.re);
    }
};

class FFTAnalyzer : public QObject {
    Q_OBJECT
public:
    explicit FFTAnalyzer(QObject *parent = nullptr);
    ~FFTAnalyzer();

    void init(int fftSize = 2048, int sampleRate = 44100);
    void analyze(const QVector<float>& audioData);

    const QVector<float>& spectrum() const { return m_spectrum; }
    const QVector<float>& smoothedSpectrum() const { return m_smoothedSpectrum; }
    float bassLevel() const { return m_bassLevel; }
    float midLevel() const { return m_midLevel; }
    float trebleLevel() const { return m_trebleLevel; }

signals:
    void spectrumUpdated();

private:
    void fft(QVector<Complex>& data, bool invert);
    void computeSpectrum();
    void smoothSpectrum();
    void computeFrequencyBands();

    int m_fftSize = 2048;
    int m_sampleRate = 44100;
    
    QVector<float> m_inputBuffer;
    QVector<float> m_window;
    QVector<Complex> m_fftComplex;
    QVector<float> m_spectrum;
    QVector<float> m_smoothedSpectrum;
    QVector<float> m_prevSpectrum;

    float m_bassLevel = 0.0f;
    float m_midLevel = 0.0f;
    float m_trebleLevel = 0.0f;

    float m_smoothingFactor = 0.0f;
};

#endif // FFTANALYZER_H
