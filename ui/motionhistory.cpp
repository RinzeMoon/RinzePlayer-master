#include "motionhistory.h"
#include <QPainter>

MotionHistoryWidget::MotionHistoryWidget(QWidget *parent)
    : QWidget(parent)
{
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setMinimumSize(400, 300);
}

QRgb MotionHistoryWidget::grayToPseudoColor(int gray)
{
    // 伪彩色：冷→暖
    // 0（黑）→蓝→青→绿→黄→红→255（白）
    float ratio = float(gray) / 255.0f;
    
    if (ratio < 0.2f) {
        // 黑→蓝
        int r = 0;
        int g = 0;
        int b = int(255 * (ratio / 0.2f));
        return qRgb(r, g, b);
    } else if (ratio < 0.4f) {
        // 蓝→青
        int r = 0;
        int g = int(255 * ((ratio - 0.2f) / 0.2f));
        int b = 255;
        return qRgb(r, g, b);
    } else if (ratio < 0.6f) {
        // 青→绿
        int r = 0;
        int g = 255;
        int b = int(255 * (1 - (ratio - 0.4f) / 0.2f));
        return qRgb(r, g, b);
    } else if (ratio < 0.8f) {
        // 绿→黄
        int r = int(255 * ((ratio - 0.6f) / 0.2f));
        int g = 255;
        int b = 0;
        return qRgb(r, g, b);
    } else {
        // 黄→红→白
        float subRatio = (ratio - 0.8f) / 0.2f;
        if (subRatio < 0.5f) {
            // 黄→红
            int r = int(255 * (1 + subRatio));
            int g = 255;
            int b = 0;
            return qRgb(r, g, b);
        } else {
            // 红→白
            int r = 255;
            int g = 255;
            int b = int(255 * (subRatio - 0.5f) * 2);
            return qRgb(r, g, b);
        }
    }
}

void MotionHistoryWidget::updateMotionData(const std::vector<MotionVector>& mv, int videoWidth, int videoHeight)
{
    m_videoWidth = videoWidth;
    m_videoHeight = videoHeight;
    
    // 初始化或重置图像
    if (m_historyImage.isNull() || 
        m_historyImage.width() != videoWidth || 
        m_historyImage.height() != videoHeight) {
        m_historyImage = QImage(videoWidth, videoHeight, QImage::Format_Grayscale8);
        m_historyImage.fill(Qt::black);
    }
    
    // 衰减现有像素
    for (int y = 0; y < m_historyImage.height(); y++) {
        uchar* scanLine = m_historyImage.scanLine(y);
        for (int x = 0; x < m_historyImage.width(); x++) {
            scanLine[x] = uchar(std::max(0, int(scanLine[x]) - m_decayRate));
        }
    }
    
    // 更新运动区域
    for (const auto& vec : mv) {
        int x1 = vec.dst_x;
        int y1 = vec.dst_y;
        int x2 = x1 + vec.w;
        int y2 = y1 + vec.h;
        
        x1 = std::max(0, x1);
        y1 = std::max(0, y1);
        x2 = std::min(m_historyImage.width(), x2);
        y2 = std::min(m_historyImage.height(), y2);
        
        for (int y = y1; y < y2; y++) {
            uchar* scanLine = m_historyImage.scanLine(y);
            for (int x = x1; x < x2; x++) {
                scanLine[x] = 255; // 有运动的地方设为最大亮度
            }
        }
    }
    
    update();
}

void MotionHistoryWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    if (m_historyImage.isNull()) {
        return;
    }
    
    QPainter painter(this);
    
    if (m_usePseudoColor) {
        // 伪彩色模式
        QImage colorImage(m_historyImage.size(), QImage::Format_RGB32);
        
        for (int y = 0; y < m_historyImage.height(); y++) {
            const uchar* srcLine = m_historyImage.scanLine(y);
            QRgb* dstLine = reinterpret_cast<QRgb*>(colorImage.scanLine(y));
            
            for (int x = 0; x < m_historyImage.width(); x++) {
                dstLine[x] = grayToPseudoColor(srcLine[x]);
            }
        }
        
        // 缩放绘制
        QImage scaled = colorImage.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawImage(0, 0, scaled);
    } else {
        // 灰度模式
        QImage scaled = m_historyImage.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawImage(0, 0, scaled);
    }
}