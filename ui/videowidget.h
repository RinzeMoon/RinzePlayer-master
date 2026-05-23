// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include "renderer/renderdata.h"
#include "streaming/streaming_types.h"
#include "motionheatmap.h"
#include "motiontrail.h"
#include "motionhistory.h"
#include "hovercontrolbar.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QSize>

class VideoWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

    void setShowSubtitle(bool val);
    // 更新缓冲区信息
    void updateBufferInfo(int videoPktSize, int videoFrmSize, int audioPktSize, int audioFrmSize);
    // 更新同步信息
    void updateSyncInfo(double syncDifference);
    // 更新内存信息
    void updateMemoryInfo(double memoryMB);
    void setXY(float newX, float newY);
    void setX(float newX);
    void setY(float newY);
    void addX() { setX(tx() + 1); }
    void subX() { setX(tx() - 1); }
    void addY() { setY(ty() + 1); }
    void subY() { setY(ty() - 1); }
    void addXY(float deltaX, float deltaY) { setXY(tx() + deltaX, ty() + deltaY); }

    void setScale(int newScale);
    void addScale() { setScale(scale() + 2); }
    void subScale() { setScale(scale() - 2); }

    void setHorizontalMirror(bool val);
    void toggleHorizontalMirror() { setHorizontalMirror(!m_horizontalMirror); }
    void setVerticalMirror(bool val);
    void toggleVerticalMirror() { setVerticalMirror(!m_verticalMirror); }

    void setAngle(float angle);
    void addAngle() { setAngle(this->angle() + 1.f); }
    void subAngle() { setAngle(this->angle() - 1.f); }
    void rotate90Left() { setAngle(this->angle() - 90.f); }
    void rotate90Right() { setAngle(this->angle() + 90.f); }
    void resetTransform();

    // Video filters
    void setBrightness(float val);
    void setContrast(float val);
    void setSaturation(float val);
    void resetFilters();

    // FPS display
    void setShowFPS(bool val);
    bool showFPS() const { return m_showFPS; }
    
    // 运动向量可视化
    void setShowMotionVectors(bool val);
    bool showMotionVectors() const { return m_showMotionVectors; }
    void setMotionVectorScale(float scale);
    float motionVectorScale() const { return m_motionVectorScale; }
    
    // 运动能量热力图
    void toggleMotionHeatmap(); // 开关热力图窗口
    
    // 运动轨迹图
    void toggleMotionTrail(); // 开关轨迹图窗口
    
    // 运动历史图像
    void toggleMotionHistory(); // 开关MHI窗口

    // 小窗口模式（画中画用）
    void setMiniMode(bool isMini);

    // Color space
    void setColorSpace(int colorSpace);
    int colorSpace() const { return m_colorSpace; }
    
    // Reset video info sent flag
    void resetVideoInfoSent() { m_videoInfoSent = false; }

    int scale() const { return m_scale; }
    float angle() const { return m_angle; }
    float tx() const { return m_tx; }
    float ty() const { return m_ty; }
    float brightness() const { return m_brightness; }
    float contrast() const { return m_contrast; }
    float saturation() const { return m_saturation; }
    
    // 获取悬浮控制栏
    HoverControlBar* hoverControlBar() { return m_hoverControlBar; }

signals:
    void videoInfoChanged(const QString& pixelFormat, const QString& colorRange, const QString& colorSpace);

public slots:
    void updateRenderData(VideoDoubleBuf *vidData, SubtitleDoubleBuf *subData);
    void forceClearSubtitle();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void initVideoTex(VideoRenderData *renderData);
    void initSubtitleTex(SubRenderData *subRenderData);
    bool updateTex(VideoRenderData::PixFormat fmt);
    bool updateSubTex(SubRenderData &renData);
    void initTex(GLuint &tex, const QSize &size, const std::array<unsigned int, 3> &para, uint8_t *fill = nullptr);
    QMatrix4x4 getTransformMat();
    void bindAllTexturesForDraw();
    void clearSubtitleTex();

    bool m_miniMode = false; // 小窗口模式（画中画用）

    QOpenGLShaderProgram m_program;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;

    QSize m_frameSize{};
    QSize m_subtitleSize{};
    AVPixelFormat m_AVPixelFormat = AV_PIX_FMT_NONE;
    GLuint m_texArr[4]{0, 0, 0, 0};
    GLuint m_subTex = 0;
    std::array<unsigned int, 3> GLParaArr[4]{};
    QSize componentSizeArr[4]{};
    const uint8_t *dataArr[4]{};
    int linesizeArr[4]{};
    int alignment = 1;

    float m_tx{0.f}, m_ty{0.f};
    float m_angle{0.f};
    float m_scaleX{1.f};
    float m_scaleY{1.f};
    int m_scale{100};

    bool m_needInitVideoTex = true;
    bool m_needInitSubtitleTex = true;
    bool m_lastShowSubtitle = true;
    bool m_showSubtitle = true;
    bool m_forceClearSubtitle{false};

    bool m_horizontalMirror{false};
    bool m_verticalMirror{false};

    float m_brightness = 0.0f;
    float m_contrast = 1.0f;
    float m_saturation = 1.0f;
    int m_colorSpace = 1; // Default: BT.709 (0 = BT.601, 1 = BT.709, 2 = BT.2020)

    bool m_showFPS = false;
    int m_frameCount = 0;
    qint64 m_lastFPSUpdate = 0;
    float m_currentFPS = 0.0;
    int m_videoWidth = 0;
    int m_videoHeight = 0;
    // Buffer信息
    int m_videoPktQueueSize = 0;
    int m_videoFrmQueueSize = 0;
    int m_audioPktQueueSize = 0;
    int m_audioFrmQueueSize = 0;
    // 同步信息
    double m_syncDifference = 0.0;
    // 内存信息
    double m_memoryUsageMB = 0.0;
    
    // 是否已经发送了视频信息
    bool m_videoInfoSent = false;

    VideoDoubleBuf *m_vidData = nullptr;
    SubtitleDoubleBuf *m_subData = nullptr;
    
    // 运动向量可视化
    bool m_showMotionVectors = false;
    float m_motionVectorScale = 0.05f; // 调整比例让箭头更明显
    std::vector<MotionVector> m_currentMVs; // 当前帧的运动向量
    std::vector<MotionVector> m_smoothedMVs; // 平滑后的运动向量（用于绘制）
    
    // 运动能量热力图
    MotionHeatmapWidget *m_heatmapWidget = nullptr;
    
    // 运动轨迹图
    MotionTrailWidget *m_trailWidget = nullptr;
    
    // 运动历史图像
    MotionHistoryWidget *m_historyWidget = nullptr;

    // 保存当前帧的数据用于计算meta块平均颜色
    VideoRenderData::PixFormat m_pixFormat = VideoRenderData::NONE;
    const uint8_t *m_mvDataArr[4]{nullptr, nullptr, nullptr, nullptr};
    int m_mvLinesizeArr[4]{0, 0, 0, 0};
    QSize m_mvComponentSizeArr[4]{};
    
    void updateMotionVectorData(VideoRenderData* renderData);
    void drawSimpleMotionVectors(QPainter& painter); // 用QPainter画简单彩色箭头
    void clearMotionVectorData(); // 清理运动向量相关数据，防止访问已释放内存
    
    // 计算meta块平均颜色
    QColor calculateMetaBlockAverageColor(int dst_x, int dst_y, int w, int h);
    
    // 悬浮控制栏
    HoverControlBar* m_hoverControlBar = nullptr;
};

#endif // VIDEOWIDGET_H
