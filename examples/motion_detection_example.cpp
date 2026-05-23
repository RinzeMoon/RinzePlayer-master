// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file motion_detection_example.cpp
 * @brief 运动检测功能使用示例
 * 
 * 本文件展示了如何在播放器中集成 MotionDetector 类来实现实时运动检测。
 */

#include &lt;QCoreApplication&gt;
#include &lt;QDebug&gt;
#include "../ui/motiondetector.h"
#include "../renderer/renderdata.h"

// 模拟数据 - 用于演示
namespace {
    // 生成模拟运动矢量
    std::vector&lt;MotionVector&gt; generateMockMotionVectors(int width, int height) {
        std::vector&lt;MotionVector&gt; mvs;
        
        // 在图像中心区域创建运动
        int centerX = width / 2;
        int centerY = height / 2;
        int mbSize = 16;
        
        for (int y = centerY - 64; y &lt; centerY + 64; y += mbSize) {
            for (int x = centerX - 64; x &lt; centerX + 64; x += mbSize) {
                MotionVector mv;
                mv.dst_x = static_cast&lt;int16_t&gt;(x);
                mv.dst_y = static_cast&lt;int16_t&gt;(y);
                mv.motion_x = static_cast&lt;int16_t&gt;((x - centerX) / 2);  // quarter-pel
                mv.motion_y = static_cast&lt;int16_t&gt;((y - centerY) / 2);
                mv.w = 16;
                mv.h = 16;
                mv.source = -1;  // 前向
                mv.flags = 0;
                mvs.push_back(mv);
            }
        }
        
        return mvs;
    }
}

/**
 * @brief 示例 1: 基本使用方法
 */
void example_basic_usage() {
    qDebug() &lt;&lt; "=== 示例 1: 基本使用方法 ===";
    
    // 1. 创建运动检测器
    MotionDetector detector(
        1.5f,   // 运动幅度阈值（像素）
        500,    // 最小轮廓面积（像素）
        16      // 宏块大小
    );
    
    // 2. 准备模拟数据
    int frameWidth = 1920;
    int frameHeight = 1080;
    
    // 创建模拟的BGR图像（黑色背景）
    cv::Mat bgrImage(frameHeight, frameWidth, CV_8UC3, cv::Scalar(0, 0, 0));
    
    // 生成模拟运动矢量
    auto motionVectors = generateMockMotionVectors(frameWidth, frameHeight);
    
    // 3. 执行检测
    MotionDetectionResult result = detector.detect(
        motionVectors,
        frameWidth,
        frameHeight,
        bgrImage
    );
    
    // 4. 输出结果
    qDebug() &lt;&lt; "检测到" &lt;&lt; result.rects.size() &lt;&lt; "个运动区域";
    for (size_t i = 0; i &lt; result.rects.size(); ++i) {
        const auto&amp; rect = result.rects[i];
        qDebug() &lt;&lt; "  区域" &lt;&lt; i &lt;&lt; ":" &lt;&lt; rect.x &lt;&lt; "," &lt;&lt; rect.y 
                 &lt;&lt; " 大小:" &lt;&lt; rect.width &lt;&lt; "x" &lt;&lt; rect.height;
    }
    
    // 5. 可选: 保存结果图像
    // cv::imwrite("motion_detection_result.jpg", result.annotatedImage);
}

/**
 * @brief 示例 2: 在 VideoWidget 中集成（伪代码）
 * 
 * 展示如何在现有的 VideoWidget 中集成运动检测功能。
 */
void example_integration_with_videowidget() {
    qDebug() &lt;&lt; "\n=== 示例 2: VideoWidget 集成示例 ===";
    
    /*
     * 在 videowidget.h 中添加:
     * 
     * #include "motiondetector.h"
     * 
     * private:
     *     MotionDetector* m_motionDetector;
     *     bool m_showMotionDetection;
     */
    
    /*
     * 在 videowidget.cpp 的构造函数中初始化:
     * 
     * VideoWidget::VideoWidget(QWidget* parent)
     *     : QOpenGLWidget(parent)
     *     , m_motionDetector(new MotionDetector(1.5f, 500, 16))
     *     , m_showMotionDetection(false)
     * {
     *     // ... 其他初始化代码 ...
     * }
     */
    
    /*
     * 在 updateMotionVectorData 或 paintGL 中使用:
     * 
     * void VideoWidget::updateMotionVectorData(VideoRenderData* renderData) {
     *     // ... 原有代码 ...
     * 
     *     // 如果启用了运动检测
     *     if (m_showMotionDetection &amp;&amp; renderData-&gt;hasMotionVectors) {
     *         // 1. 将 YUV 转换为 BGR
     *         cv::Mat bgrImage = MotionDetector::yuv420pToBgr(
     *             renderData-&gt;dataArr[0],  // Y 平面
     *             renderData-&gt;dataArr[1],  // U 平面
     *             renderData-&gt;dataArr[2],  // V 平面
     *             renderData-&gt;frmItem.frm-&gt;width,
     *             renderData-&gt;frmItem.frm-&gt;height,
     *             renderData-&gt;linesizeArr[0],
     *             renderData-&gt;linesizeArr[1],
     *             renderData-&gt;linesizeArr[2]
     *         );
     * 
     *         // 或者直接从 AVFrame 转换
     *         // cv::Mat bgrImage = MotionDetector::avFrameToBgr(renderData-&gt;frmItem.frm);
     * 
     *         if (!bgrImage.empty()) {
     *             // 2. 执行运动检测
     *             MotionDetectionResult result = m_motionDetector-&gt;detect(
     *                 renderData-&gt;motionVectors,
     *                 renderData-&gt;frmItem.frm-&gt;width,
     *                 renderData-&gt;frmItem.frm-&gt;height,
     *                 bgrImage
     *             );
     * 
     *             // 3. 处理结果 - 这里可以保存结果用于后续绘制
     *             // m_lastMotionRects = result.rects;
     * 
     *             qDebug() &lt;&lt; "检测到" &lt;&lt; result.rects.size() &lt;&lt; "个运动区域";
     *         }
     *     }
     * }
     */
    
    qDebug() &lt;&lt; "请参考代码注释了解集成方法";
}

/**
 * @brief 示例 3: 参数调优
 */
void example_parameter_tuning() {
    qDebug() &lt;&lt; "\n=== 示例 3: 参数调优 ===";
    
    MotionDetector detector;
    
    // 场景 1: 快速移动的小物体
    qDebug() &lt;&lt; "场景 1: 快速移动的小物体";
    detector.setThreshold(0.5f);    // 降低阈值，检测更敏感
    detector.setMinArea(100);       // 允许更小的区域
    detector.setMacroblockSize(16);
    
    // 场景 2: 慢速移动的大物体
    qDebug() &lt;&lt; "场景 2: 慢速移动的大物体";
    detector.setThreshold(2.0f);    // 提高阈值，减少噪声
    detector.setMinArea(1000);      // 只检测大区域
    
    // 场景 3: 高清视频（可能使用更小的宏块）
    qDebug() &lt;&lt; "场景 3: 高清视频";
    detector.setMacroblockSize(8);  // 使用更小的宏块大小（如果编码器支持）
}

/**
 * @brief 主函数
 */
int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    
    qDebug() &lt;&lt; "运动检测功能使用示例\n";
    
    example_basic_usage();
    example_integration_with_videowidget();
    example_parameter_tuning();
    
    qDebug() &lt;&lt; "\n示例运行完成！";
    return 0;
}
