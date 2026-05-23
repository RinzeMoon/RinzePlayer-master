#ifndef MOTIONHEATMAP_H
#define MOTIONHEATMAP_H

#include <QWidget>
#include <QPainter>
#include <vector>
#include "../renderer/renderdata.h"

class MotionHeatmapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MotionHeatmapWidget(QWidget *parent = nullptr);
    
    // 更新运动向量数据和帧类型
    void updateMotionData(const std::vector<MotionVector>& mv, int videoWidth, int videoHeight, int frameType);
    
    // 帧类型：0=I, 1=P, 2=B
    enum FrameType { I_FRAME = 0, P_FRAME = 1, B_FRAME = 2 };

private:
    int m_frameType = 0;
    int m_displayFrameType = 0; // 实际显示的帧类型
    int m_frameCounter = 0;      // 帧计数器
    
protected:
    void paintEvent(QPaintEvent *event) override;
    
private:
    // 运动向量数据
    std::vector<MotionVector> m_motionVectors;
    
    // 视频尺寸
    int m_videoWidth = 0;
    int m_videoHeight = 0;
    
    // 将运动长度映射到颜色
    QColor lengthToColor(float length, float maxLength);
    
    // 获取最大运动长度
    float getMaxLength();
};

#endif // MOTIONHEATMAP_H