#ifndef MOTIONHISTORY_H
#define MOTIONHISTORY_H

#include <QWidget>
#include <QImage>
#include <vector>
#include "../renderer/renderdata.h"

class MotionHistoryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MotionHistoryWidget(QWidget *parent = nullptr);
    
    // 更新运动向量数据
    void updateMotionData(const std::vector<MotionVector>& mv, int videoWidth, int videoHeight);

private:
    int m_videoWidth = 0;
    int m_videoHeight = 0;
    QImage m_historyImage; // MHI 图像
    int m_decayRate = 5;   // 每帧衰减值
    bool m_usePseudoColor = true; // 是否使用伪彩色

    // 灰度图转伪彩色
    QRgb grayToPseudoColor(int gray);

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // MOTIONHISTORY_H