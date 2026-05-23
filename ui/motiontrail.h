#ifndef MOTIONTRAIL_H
#define MOTIONTRAIL_H

#include <QWidget>
#include <vector>
#include <deque>
#include "../renderer/renderdata.h"

// 轨迹点结构
struct TrailPoint {
    QPoint pos;
    int age; // 0 = 最新，越大越旧
};

// 宏块轨迹
struct BlockTrail {
    int gridX;
    int gridY;
    std::deque<TrailPoint> points;
};

class MotionTrailWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MotionTrailWidget(QWidget *parent = nullptr);
    
    // 更新运动向量数据
    void updateMotionData(const std::vector<MotionVector>& mv, int videoWidth, int videoHeight);

private:
    int m_videoWidth = 0;
    int m_videoHeight = 0;
    std::vector<BlockTrail> m_trails; // 所有宏块的轨迹
    int m_maxTrailLength = 20; // 轨迹最大长度（帧数）
    
    // 颜色映射：年龄→颜色（红→蓝）
    QColor ageToColor(int age, int maxAge);

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // MOTIONTRAIL_H