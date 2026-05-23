#include "motiontrail.h"
#include <QPainter>
#include <QPen>

MotionTrailWidget::MotionTrailWidget(QWidget *parent)
    : QWidget(parent)
{
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setMinimumSize(400, 300);
}

QColor MotionTrailWidget::ageToColor(int age, int maxAge)
{
    // 颜色渐变：红色（新）→黄色→绿色→青色→蓝色（旧）
    float ratio = float(age) / float(maxAge);
    
    if (ratio < 0.25f) {
        // 红→黄
        float r = 255;
        float g = 255 * (ratio / 0.25f);
        float b = 0;
        return QColor(int(r), int(g), int(b), 180);
    } else if (ratio < 0.5f) {
        // 黄→绿
        float r = 255 * (1 - (ratio - 0.25f) / 0.25f);
        float g = 255;
        float b = 0;
        return QColor(int(r), int(g), int(b), 180);
    } else if (ratio < 0.75f) {
        // 绿→青
        float r = 0;
        float g = 255;
        float b = 255 * ((ratio - 0.5f) / 0.25f);
        return QColor(int(r), int(g), int(b), 180);
    } else {
        // 青→蓝
        float r = 0;
        float g = 255 * (1 - (ratio - 0.75f) / 0.25f);
        float b = 255;
        return QColor(int(r), int(g), int(b), 180);
    }
}

void MotionTrailWidget::updateMotionData(const std::vector<MotionVector>& mv, int videoWidth, int videoHeight)
{
    m_videoWidth = videoWidth;
    m_videoHeight = videoHeight;
    
    // 所有轨迹年龄+1
    for (auto& trail : m_trails) {
        for (auto& point : trail.points) {
            point.age++;
        }
    }
    
    // 更新当前帧的轨迹
    for (const auto& vec : mv) {
        int gridX = vec.dst_x / 16; // 16x16 网格
        int gridY = vec.dst_y / 16;
        
        // 找到或创建该宏块的轨迹
        BlockTrail* trail = nullptr;
        for (auto& t : m_trails) {
            if (t.gridX == gridX && t.gridY == gridY) {
                trail = &t;
                break;
            }
        }
        if (!trail) {
            m_trails.emplace_back();
            trail = &m_trails.back();
            trail->gridX = gridX;
            trail->gridY = gridY;
        }
        
        // 添加新轨迹点
        TrailPoint newPoint;
        newPoint.pos = QPoint(vec.dst_x + vec.w/2, vec.dst_y + vec.h/2);
        newPoint.age = 0;
        trail->points.push_front(newPoint);
        
        // 限制轨迹长度
        if (trail->points.size() > m_maxTrailLength) {
            trail->points.pop_back();
        }
    }
    
    // 清理过长的轨迹
    m_trails.erase(std::remove_if(m_trails.begin(), m_trails.end(), 
        [this](const BlockTrail& t) {
            return t.points.empty() || t.points.front().age > m_maxTrailLength;
        }),
        m_trails.end());
    
    update();
}

void MotionTrailWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    if (m_videoWidth <= 0 || m_videoHeight <= 0) {
        return;
    }
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 计算缩放
    float scaleX = float(width()) / float(m_videoWidth);
    float scaleY = float(height()) / float(m_videoHeight);
    
    // 绘制轨迹
    for (const auto& trail : m_trails) {
        if (trail.points.size() < 2) {
            continue;
        }
        
        // 连接轨迹点
        for (size_t i = 0; i < trail.points.size() - 1; i++) {
            const TrailPoint& p1 = trail.points[i];
            const TrailPoint& p2 = trail.points[i + 1];
            
            QPointF screenP1(p1.pos.x() * scaleX, p1.pos.y() * scaleY);
            QPointF screenP2(p2.pos.x() * scaleX, p2.pos.y() * scaleY);
            
            QColor color = ageToColor(p1.age, m_maxTrailLength);
            QPen pen(color, 2);
            painter.setPen(pen);
            
            painter.drawLine(screenP1, screenP2);
        }
    }
}