// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef REPORTGENERATOR_H
#define REPORTGENERATOR_H

#include <QObject>
#include <QString>
#include <vector>
#include <QImage>
#include <QPainter>
#include <QMutex>
#include <memory>
#include <atomic>
#include <QDir>

#include "../renderer/renderdata.h"

AZ_EXTERN_C_BEGIN
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/motion_vector.h>
AZ_EXTERN_C_END

struct ReportData {
    QString videoName;
    double durationSec;
    int width;
    int height;
    double fps;
    int totalFrames;
    QString codecName;

    std::vector<float> energyPerFrame;
    std::vector<MotionVector> allMotionVectors;
    std::vector<std::vector<float>> spatialEnergyGrid;
    std::vector<int> qpPerFrame;
    std::vector<std::vector<int>> qpGrid;

    int frameTypeCounts[3];
    int encodingModeCounts[4];

    std::vector<std::pair<int, QImage>> keyFrames;

    // 新增：方向统计
    int directionCounts[16];
    float directionMagnitude[16];
    float directionPercentages[16];
    float directionAvgLength[16];

    // 新增：总能量统计
    float totalEnergy;
    float maxFrameEnergy;
};

class ReportGenerator : public QObject {
    Q_OBJECT
public:
    explicit ReportGenerator(QObject *parent = nullptr);
    ~ReportGenerator();

    void setSettings(const QString &videoPath, const QString &outputPath);
    void cancel();

public slots:
    void process();

signals:
    void progressUpdated(int progress, const QString &text);
    void finished(bool success, const QString &message);

private:
    bool collectData(ReportData &data);
    QImage convertAVFrameToQImage(AVFrame *frame);
    QImage generateEnergyCurve(const ReportData &data);
    QImage generateSpatialHeatmap(const ReportData &data);
    QImage generateDirectionRose(const ReportData &data);
    QImage generateQPCurve(const ReportData &data);
    QString fillTemplate(const ReportData &data, const QString tempDir);
    bool htmlToPdf(const QString &htmlContent, const QString &pdfPath, const QString &baseDir);

    QString m_videoPath;
    QString m_outputPath;
    std::atomic<bool> m_shouldCancel;
};

#endif // REPORTGENERATOR_H
