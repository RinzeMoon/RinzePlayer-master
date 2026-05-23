#include "motionheatmap.h"
#include <cmath>
#include <QPainter>
#include <QBrush>
#include <QRectF>

MotionHeatmapWidget::MotionHeatmapWidget(QWidget *parent)
    : QWidget(parent)
{
    // 设置背景颜色
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setMinimumSize(400, 300);
}

void MotionHeatmapWidget::updateMotionData(const std::vector<MotionVector>& mv, int videoWidth, int videoHeight, int frameType)
{
    m_motionVectors = mv;
    m_videoWidth = videoWidth;
    m_videoHeight = videoHeight;
    m_frameType = frameType;
    
    // 每5帧才更新一次显示
    m_frameCounter++;
    if (m_frameCounter % 5 == 0) {
        m_displayFrameType = frameType;
    }
    
    update();
}

QColor MotionHeatmapWidget::lengthToColor(float length, float maxLength)
{
    if (maxLength <= 0) {
        return QColor(0, 0, 255, 100); // 默认蓝色
    }
    
    // 归一化到 0-1
    float normalized = length / maxLength;
    
    // 使用蓝 -> 红 的渐变（0蓝 0.5绿 1红）
    if (normalized < 0.5f) {
        // 蓝到绿
        float ratio = normalized * 2.0f;
        int r = static_cast<int>(0 * (1 - ratio) + 0 * ratio);
        int g = static_cast<int>(0 * (1 - ratio) + 255 * ratio);
        int b = static_cast<int>(255 * (1 - ratio) + 0 * ratio);
        return QColor(r, g, b, 180);
    } else {
        // 绿到红
        float ratio = (normalized - 0.5f) * 2.0f;
        int r = static_cast<int>(0 * (1 - ratio) + 255 * ratio);
        int g = static_cast<int>(255 * (1 - ratio) + 0 * ratio);
        int b = static_cast<int>(0 * (1 - ratio) + 0 * ratio);
        return QColor(r, g, b, 180);
    }
}

float MotionHeatmapWidget::getMaxLength()
{
    float maxLen = 0.0f;
    for (const auto& mv : m_motionVectors) {
        float len = std::sqrt(static_cast<float>(mv.motion_x * mv.motion_x) + 
                              static_cast<float>(mv.motion_y * mv.motion_y));
        if (len > maxLen) {
            maxLen = len;
        }
    }
    return maxLen;
}

void MotionHeatmapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    if (m_videoWidth <= 0 || m_videoHeight <= 0) {
        return;
    }
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 获取窗口尺寸
    int widgetW = width();
    int widgetH = height();
    
    // 计算缩放比例
    float scaleX = static_cast<float>(widgetW) / m_videoWidth;
    float scaleY = static_cast<float>(widgetH) / m_videoHeight;
    
    // 小网格块大小（视频坐标系中的像素大小）
    const int gridSize = 8;
    
    // 创建能量图（小网格）
    struct GridEnergy {
        float sumLen = 0.0f;
        int count = 0;
    };
    
    int gridW = (m_videoWidth + gridSize - 1) / gridSize;  // 向上取整
    int gridH = (m_videoHeight + gridSize - 1) / gridSize;
    std::vector<std::vector<GridEnergy>> energyGrid(gridH, std::vector<GridEnergy>(gridW));
    
    // 填充能量图
    float maxLength = 0.0f;
    if (!m_motionVectors.empty()) {
        maxLength = getMaxLength();
        
        for (const auto& mv : m_motionVectors) {
            float len = std::sqrt(static_cast<float>(mv.motion_x * mv.motion_x) + 
                                  static_cast<float>(mv.motion_y * mv.motion_y));
            
            // 找到这个运动向量覆盖的小网格区域
            int startGridX = std::max(0, mv.dst_x / gridSize);
            int startGridY = std::max(0, mv.dst_y / gridSize);
            int endGridX = std::min(gridW - 1, (mv.dst_x + mv.w - 1) / gridSize);
            int endGridY = std::min(gridH - 1, (mv.dst_y + mv.h - 1) / gridSize);
            
            // 将这个运动向量的长度累加到覆盖的小网格
            for (int gy = startGridY; gy <= endGridY; ++gy) {
                for (int gx = startGridX; gx <= endGridX; ++gx) {
                    energyGrid[gy][gx].sumLen += len;
                    energyGrid[gy][gx].count++;
                }
            }
        }
    }
    
    // 绘制每个小网格块
    for (int gy = 0; gy < gridH; ++gy) {
        for (int gx = 0; gx < gridW; ++gx) {
            const auto& cell = energyGrid[gy][gx];
            float avgLen = 0.0f;
            
            if (cell.count > 0) {
                avgLen = cell.sumLen / cell.count;
            }
            
            // 计算颜色
            QColor color = lengthToColor(avgLen, maxLength);
            
            // 计算小网格块在窗口中的位置
            float x = static_cast<float>(gx * gridSize) * scaleX;
            float y = static_cast<float>(gy * gridSize) * scaleY;
            float w = static_cast<float>(gridSize) * scaleX;
            float h = static_cast<float>(gridSize) * scaleY;
            
            // 绘制热力块
            painter.setBrush(QBrush(color));
            painter.setPen(Qt::NoPen);
            painter.drawRect(QRectF(x, y, w, h));
        }
    }
    
    // 在左上角绘制帧类型文字
    QString frameLabel;
    QColor textColor;
    QColor bgColor;
    switch (m_displayFrameType) {
        case 0: // I 帧
            frameLabel = "I";
            textColor = QColor(255, 255, 255);
            bgColor = QColor(0, 200, 0, 200);
            break;
        case 1: // P 帧
            frameLabel = "P";
            textColor = QColor(255, 255, 255);
            bgColor = QColor(0, 100, 200, 200);
            break;
        case 2: // B 帧
            frameLabel = "B";
            textColor = QColor(255, 255, 255);
            bgColor = QColor(200, 100, 0, 200);
            break;
        default:
            frameLabel = "?";
            textColor = QColor(255, 255, 255);
            bgColor = QColor(128, 128, 128, 200);
    }
    
    // 绘制文字背景
    QFont font("Arial", 20, QFont::Bold);
    QFontMetrics fm(font);
    QString fullLabel = "Frame: " + frameLabel;
    QRect textRect = fm.boundingRect(fullLabel);
    int margin = 10;
    int bgW = textRect.width() + margin * 2;
    int bgH = textRect.height() + margin * 2;
    
    painter.setBrush(QBrush(bgColor));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(5, 5, bgW, bgH, 5, 5);
    
    // 绘制文字
    painter.setFont(font);
    painter.setPen(textColor);
    painter.drawText(QPoint(5 + margin, 5 + fm.ascent() + margin), fullLabel);
}