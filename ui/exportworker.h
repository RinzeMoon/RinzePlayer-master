// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EXPORTWORKER_H
#define EXPORTWORKER_H

#include <QObject>
#include <QString>
#include <vector>
#include <QProcess>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QMutex>
#include <memory>
#include <thread>
#include <atomic>

#include "../renderer/renderdata.h"

AZ_EXTERN_C_BEGIN
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/motion_vector.h>
AZ_EXTERN_C_END

// 导出设置结构
struct ExportSettings {
    QString sourcePath;
    QString outputPath;
    bool includeAudio;
    bool exportMotionVectors;
    bool exportHeatmap;
    bool exportTrail;
    bool exportHistory;
    float motionVectorScale;
};

// 运动历史图像状态
struct MotionHistoryState {
    QImage historyImage;
    std::vector<std::vector<float>> historyBuffer;
    int decay;
    int threshold;
    
    MotionHistoryState() : decay(2), threshold(32) {}
    
    void init(int width, int height) {
        historyImage = QImage(width, height, QImage::Format_Grayscale8);
        historyImage.fill(0);
        historyBuffer.resize(height, std::vector<float>(width, 0));
    }
    
    void update(const std::vector<MotionVector>& mvs, int frameWidth, int frameHeight) {
        for (int y = 0; y < historyBuffer.size(); ++y) {
            for (int x = 0; x < historyBuffer[y].size(); ++x) {
                historyBuffer[y][x] = std::max(0.0f, historyBuffer[y][x] - decay);
            }
        }
        
        for (const auto& mv : mvs) {
            float length = std::sqrt(mv.motion_x * mv.motion_x + mv.motion_y * mv.motion_y);
            if (length >= 0.6f) {
                int startX = std::max(0, static_cast<int>(mv.dst_x - mv.w / 2));
                int startY = std::max(0, static_cast<int>(mv.dst_y - mv.h / 2));
                int endX = std::min(static_cast<int>(historyBuffer[0].size()), static_cast<int>(mv.dst_x + mv.w / 2));
                int endY = std::min(static_cast<int>(historyBuffer.size()), static_cast<int>(mv.dst_y + mv.h / 2));
                
                for (int y = startY; y < endY; ++y) {
                    for (int x = startX; x < endX; ++x) {
                        historyBuffer[y][x] = std::min(255.0f, historyBuffer[y][x] + 64.0f);
                    }
                }
            }
        }
        
        for (int y = 0; y < historyBuffer.size(); ++y) {
            for (int x = 0; x < historyBuffer[y].size(); ++x) {
                uchar* pixel = historyImage.scanLine(y) + x;
                *pixel = static_cast<uchar>(historyBuffer[y][x]);
            }
        }
    }
};

// 运动轨迹状态
struct MotionTrailState {
    struct TrailPoint {
        int x, y;
        float age;
        float motionX, motionY;
    };
    
    std::vector<std::vector<TrailPoint>> trails;
    int maxTrailLength;
    int gridSize;
    
    MotionTrailState() : maxTrailLength(30), gridSize(16) {}
    
    void init() {
        trails.clear();
    }
    
    void update(const std::vector<MotionVector>& mvs) {
        std::map<std::pair<int, int>, std::vector<MotionVector>> gridMap;
        
        for (const auto& mv : mvs) {
            int gridX = mv.dst_x / gridSize;
            int gridY = mv.dst_y / gridSize;
            gridMap[{gridX, gridY}].push_back(mv);
        }
        
        for (auto& trail : trails) {
            for (auto& point : trail) {
                point.age += 1.0f;
            }
            trail.erase(std::remove_if(trail.begin(), trail.end(),
                [this](const TrailPoint& p) { return p.age > maxTrailLength; }),
                trail.end());
        }
        
        for (const auto& entry : gridMap) {
            float avgX = 0, avgY = 0, avgMX = 0, avgMY = 0;
            for (const auto& mv : entry.second) {
                avgX += mv.dst_x;
                avgY += mv.dst_y;
                avgMX += mv.motion_x;
                avgMY += mv.motion_y;
            }
            avgX /= entry.second.size();
            avgY /= entry.second.size();
            avgMX /= entry.second.size();
            avgMY /= entry.second.size();
            
            float length = std::sqrt(avgMX * avgMX + avgMY * avgMY);
            if (length >= 0.6f) {
                size_t trailIdx = 0;
                for (; trailIdx < trails.size(); ++trailIdx) {
                    if (trails[trailIdx].empty()) break;
                    const auto& last = trails[trailIdx].back();
                    float dist = std::sqrt(std::pow(last.x - avgX, 2) + std::pow(last.y - avgY, 2));
                    if (dist < gridSize * 2) break;
                }
                if (trailIdx == trails.size()) {
                    trails.emplace_back();
                }
                trails[trailIdx].push_back({static_cast<int>(avgX), static_cast<int>(avgY), 0, avgMX, avgMY});
                if (trails[trailIdx].size() > maxTrailLength) {
                    trails[trailIdx].erase(trails[trailIdx].begin());
                }
            }
        }
        
        trails.erase(std::remove_if(trails.begin(), trails.end(),
            [](const std::vector<TrailPoint>& t) { return t.empty(); }),
            trails.end());
    }
};

// 运动热力图状态
struct MotionHeatmapState {
    std::vector<std::vector<float>> heatmapBuffer;
    int width, height;
    float decay;
    
    MotionHeatmapState() : width(0), height(0), decay(0.95f) {}
    
    void init(int w, int h) {
        width = w;
        height = h;
        heatmapBuffer.resize(height, std::vector<float>(width, 0));
    }
    
    void update(const std::vector<MotionVector>& mvs) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                heatmapBuffer[y][x] *= decay;
            }
        }
        
        for (const auto& mv : mvs) {
            float length = std::sqrt(mv.motion_x * mv.motion_x + mv.motion_y * mv.motion_y);
            if (length >= 0.6f) {
                int startX = std::max(0, mv.dst_x - mv.w / 2);
                int startY = std::max(0, mv.dst_y - mv.h / 2);
                int endX = std::min(width, mv.dst_x + mv.w / 2);
                int endY = std::min(height, mv.dst_y + mv.h / 2);
                
                float intensity = std::min(1.0f, length * 0.05f);
                for (int y = startY; y < endY; ++y) {
                    for (int x = startX; x < endX; ++x) {
                        heatmapBuffer[y][x] = std::min(1.0f, heatmapBuffer[y][x] + intensity);
                    }
                }
            }
        }
    }
    
    QColor getColor(float value) {
        if (value <= 0) return QColor(0, 0, 0, 0);
        if (value < 0.25f) {
            return QColor(0, 0, static_cast<int>(value * 4 * 255), 180);
        } else if (value < 0.5f) {
            float t = (value - 0.25f) * 4;
            return QColor(0, static_cast<int>(t * 255), 255, 180);
        } else if (value < 0.75f) {
            float t = (value - 0.5f) * 4;
            return QColor(static_cast<int>(t * 255), 255, static_cast<int>((1 - t) * 255), 180);
        } else {
            float t = (value - 0.75f) * 4;
            return QColor(255, static_cast<int>((1 - t) * 255), 0, 180);
        }
    }
};

class ExportWorker : public QObject {
    Q_OBJECT
public:
    explicit ExportWorker(QObject* parent = nullptr);
    ~ExportWorker();

    void setSettings(const ExportSettings& settings);
    void cancelExport();

public slots:
    void process();

signals:
    void progressUpdated(int percent, const QString& text);
    void exportFinished(bool success, const QString& message);
    void finished();

private:
    ExportSettings m_settings;
    std::atomic<bool> m_isExporting;
    std::atomic<bool> m_shouldCancel;
    std::atomic<int> m_frameCount;
    int m_totalFrames;
    
    MotionHistoryState m_historyState;
    MotionTrailState m_trailState;
    MotionHeatmapState m_heatmapState;
    
    bool decodeAndRender(const QString& frameDir);
    QImage convertAVFrameToQImage(AVFrame* frame);
    void renderVisualizations(QImage& image, const std::vector<MotionVector>& mvs, int frameType, int width, int height);
    void renderMotionVectors(QPainter& painter, const std::vector<MotionVector>& mvs, int width, int height);
    void renderHeatmap(QPainter& painter, int width, int height);
    void renderTrail(QPainter& painter, int width, int height);
    void renderHistory(QPainter& painter, int width, int height);
    QString findFFmpeg();
};

#endif // EXPORTWORKER_H
