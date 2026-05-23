// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "reportgenerator.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QPainterPath>
#include <cmath>
#include <QDateTime>
#include <QUuid>
#include <QTextStream>
#include <QWebEngineView>
#include <QEventLoop>
#include <QTimer>

ReportGenerator::ReportGenerator(QObject *parent)
    : QObject(parent), m_shouldCancel(false)
{
}

ReportGenerator::~ReportGenerator()
{
}

void ReportGenerator::setSettings(const QString &videoPath, const QString &outputPath)
{
    m_videoPath = videoPath;
    m_outputPath = outputPath;
}

void ReportGenerator::cancel()
{
    m_shouldCancel = true;
}

void ReportGenerator::process()
{
    m_shouldCancel = false;

    emit progressUpdated(0, "正在准备分析...");

    ReportData data;
    if (!collectData(data))
    {
        emit finished(false, "数据收集失败！");
        return;
    }

    if (m_shouldCancel)
    {
        emit finished(false, "已取消！");
        return;
    }

    emit progressUpdated(40, "正在生成图表...");
    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        emit finished(false, "无法创建临时目录！");
        return;
    }
    QString tempPath = tempDir.path();

    QImage energyCurve = generateEnergyCurve(data);
    energyCurve.save(tempPath + "/energy_curve.png");

    QImage spatialHeatmap = generateSpatialHeatmap(data);
    spatialHeatmap.save(tempPath + "/spatial_heatmap.png");

    QImage directionRose = generateDirectionRose(data);
    directionRose.save(tempPath + "/direction_rose.png");

    QImage qpCurve = generateQPCurve(data);
    qpCurve.save(tempPath + "/qp_curve.png");

    // 保存关键帧
    for (size_t i = 0; i < data.keyFrames.size(); i++)
    {
        data.keyFrames[i].second.save(tempPath + QString("/keyframe_%1.png").arg(i));
    }

    if (m_shouldCancel)
    {
        emit finished(false, "已取消！");
        return;
    }

    emit progressUpdated(70, "正在生成报告...");
    QString htmlContent = fillTemplate(data, tempPath);

    emit progressUpdated(85, "正在导出为PDF...");
    bool success = htmlToPdf(htmlContent, m_outputPath, tempPath);

    if (success)
    {
        emit progressUpdated(100, "完成！");
        emit finished(true, QString("报告已保存到: %1").arg(m_outputPath));
    }
    else
    {
        emit finished(false, "PDF生成失败！");
    }
}

bool ReportGenerator::collectData(ReportData &data)
{
    qDebug() << "开始收集数据...";
    qDebug() << "视频路径：" << m_videoPath;
    
    QFileInfo fileInfo(m_videoPath);
    if (!fileInfo.exists())
    {
        qDebug() << "错误：视频文件不存在！";
        emit progressUpdated(0, "错误：视频文件不存在！");
        return false;
    }
    qDebug() << "文件大小：" << fileInfo.size() << "bytes";

    AVFormatContext *fmtCtx = nullptr;
    
    // 使用项目中的标准方法打开文件
    qDebug() << "使用路径打开文件:" << m_videoPath;
    int ret = avformat_open_input(&fmtCtx, m_videoPath.toUtf8().constData(), nullptr, nullptr);

    if (ret < 0)
    {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "无法打开输入文件！错误：" << errStr;
        emit progressUpdated(0, QString("错误：无法打开视频文件 - %1").arg(errStr));
        return false;
    }
    qDebug() << "成功打开文件";

    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0)
    {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "无法找到流信息！错误：" << errStr;
        avformat_close_input(&fmtCtx);
        return false;
    }

    int videoStreamIdx = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; i++)
    {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIdx = i;
            break;
        }
    }

    if (videoStreamIdx < 0)
    {
        qDebug() << "未找到视频流！";
        avformat_close_input(&fmtCtx);
        return false;
    }

    AVCodecParameters *codecPar = fmtCtx->streams[videoStreamIdx]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec)
    {
        qDebug() << "未找到合适的解码器！";
        avformat_close_input(&fmtCtx);
        return false;
    }

    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx)
    {
        qDebug() << "无法分配解码器上下文！";
        avformat_close_input(&fmtCtx);
        return false;
    }

    ret = avcodec_parameters_to_context(codecCtx, codecPar);
    if (ret < 0)
    {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "无法复制解码器参数！错误：" << errStr;
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return false;
    }

    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "flags2", "+export_mvs", 0);
    av_dict_set(&opts, "threads", "auto", 0);

    ret = avcodec_open2(codecCtx, codec, &opts);
    av_dict_free(&opts);
    if (ret < 0)
    {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "无法打开解码器！错误：" << errStr;
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return false;
    }

    QFileInfo fi(m_videoPath);
    data.videoName = fi.fileName();
    data.durationSec = fmtCtx->duration / AV_TIME_BASE;
    data.width = codecCtx->width;
    data.height = codecCtx->height;
    data.fps = av_q2d(fmtCtx->streams[videoStreamIdx]->r_frame_rate);
    data.totalFrames = fmtCtx->streams[videoStreamIdx]->nb_frames > 0 ? fmtCtx->streams[videoStreamIdx]->nb_frames : 0;
    data.codecName = avcodec_get_name(codecPar->codec_id);

    memset(data.frameTypeCounts, 0, sizeof(data.frameTypeCounts));
    memset(data.encodingModeCounts, 0, sizeof(data.encodingModeCounts));

    int gridSize = 16;
    int gridWidth = (data.width + gridSize - 1) / gridSize;
    int gridHeight = (data.height + gridSize - 1) / gridSize;
    data.spatialEnergyGrid.resize(gridHeight, std::vector<float>(gridWidth, 0.0f));

    // 初始化方向统计
    const int bins = 16;
    for (int i = 0; i < bins; i++)
    {
        data.directionCounts[i] = 0;
        data.directionMagnitude[i] = 0.0f;
    }

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    int frameIndex = 0;
    int totalIFrameCount = 0;
    const int maxKeyFrames = 5;
    float nextSelectionPoint = 0.0f;
    int selectedIFrameCount = 0;
    float selectionInterval = 1.0f / maxKeyFrames;
    float totalEnergy = 0.0f;
    float maxFrameEnergy = 0.0f;

    // 第一次遍历：收集所有统计数据
    while (av_read_frame(fmtCtx, pkt) >= 0 && !m_shouldCancel)
    {
        if (pkt->stream_index == videoStreamIdx)
        {
            ret = avcodec_send_packet(codecCtx, pkt);
            if (ret < 0) break;

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(codecCtx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                else if (ret < 0) break;

                int frameType = 0;
                if (frame->pict_type == AV_PICTURE_TYPE_I) frameType = 0;
                else if (frame->pict_type == AV_PICTURE_TYPE_P) frameType = 1;
                else if (frame->pict_type == AV_PICTURE_TYPE_B) frameType = 2;
                data.frameTypeCounts[frameType]++;

                float frameEnergy = 0.0f;
                AVFrameSideData *sd = av_frame_get_side_data(frame, AV_FRAME_DATA_MOTION_VECTORS);
                if (sd)
                {
                    const AVMotionVector *avMvs = reinterpret_cast<const AVMotionVector*>(sd->data);
                    int mvCount = sd->size / sizeof(AVMotionVector);
                    for (int i = 0; i < mvCount; i++)
                    {
                        MotionVector mv;
                        mv.dst_x = avMvs[i].dst_x;
                        mv.dst_y = avMvs[i].dst_y;
                        mv.motion_x = avMvs[i].motion_x;
                        mv.motion_y = avMvs[i].motion_y;
                        mv.w = avMvs[i].w;
                        mv.h = avMvs[i].h;
                        mv.source = avMvs[i].source;
                        mv.flags = avMvs[i].flags;
                        data.allMotionVectors.push_back(mv);

                        float mag = std::sqrt((float)mv.motion_x * mv.motion_x + (float)mv.motion_y * mv.motion_y);
                        frameEnergy += mag;

                        // 空间热力图
                        int gx = std::max(0, std::min(gridWidth - 1, mv.dst_x / gridSize));
                        int gy = std::max(0, std::min(gridHeight - 1, mv.dst_y / gridSize));
                        data.spatialEnergyGrid[gy][gx] += mag;

                        // 方向统计
                        float angle = std::atan2((float)mv.motion_y, (float)mv.motion_x);
                        float deg = angle * 180.0f / M_PI;
                        if (deg < 0) deg += 360.0f;
                        int bin = (int)((deg / 360.0f) * bins) % bins;
                        data.directionCounts[bin]++;
                        data.directionMagnitude[bin] += mag;
                        
                        // 调试：前100个MV打印
                        static int debugPrintCount = 0;
                        if (debugPrintCount < 100) {
                            qDebug() << "MV" << debugPrintCount << "motion_x" << mv.motion_x << "motion_y" << mv.motion_y 
                                     << "deg" << deg << "bin" << bin;
                            debugPrintCount++;
                        }
                    }
                }
                data.energyPerFrame.push_back(frameEnergy);
                totalEnergy += frameEnergy;
                if (frameEnergy > maxFrameEnergy)
                {
                    maxFrameEnergy = frameEnergy;
                }

                // 选择关键帧（均匀分布）
                if (frame->pict_type == AV_PICTURE_TYPE_I)
                {
                    totalIFrameCount++;
                    float currentPosition = (data.totalFrames > 0) ? (float)frameIndex / data.totalFrames : 0.0f;
                    
                    if (selectedIFrameCount < maxKeyFrames && currentPosition >= nextSelectionPoint)
                    {
                        try
                        {
                            QImage img = convertAVFrameToQImage(frame);
                            if (!img.isNull())
                            {
                                QImage scaledImg = img.scaledToWidth(240, Qt::SmoothTransformation);
                                if (!scaledImg.isNull())
                                {
                                    data.keyFrames.emplace_back(frameIndex, scaledImg);
                                    qDebug() << "Selected key frame at index:" << frameIndex << "position:" << currentPosition;
                                    selectedIFrameCount++;
                                    nextSelectionPoint += selectionInterval;
                                }
                            }
                        }
                        catch (...)
                        {
                            qDebug() << "Error saving key frame at index:" << frameIndex;
                        }
                    }
                }

                if (frameIndex % 30 == 0)
                {
                    int progress = 10 + (int)((frameIndex * 25.0f) / (data.totalFrames > 0 ? data.totalFrames : 1000));
                    if (progress > 35) progress = 35;
                    emit progressUpdated(progress, QString("正在分析帧 %1...").arg(frameIndex));
                }
                frameIndex++;
            }
        }
        av_packet_unref(pkt);
    }

    qDebug() << "Total I-frames:" << totalIFrameCount << "Selected key frames:" << data.keyFrames.size();

    // 如果关键帧不够，尝试保存前面的帧
    if (data.keyFrames.size() < maxKeyFrames && frameIndex > 0)
    {
        qDebug() << "Not enough key frames, using the ones we have:" << data.keyFrames.size();
    }

    if (data.totalFrames == 0)
    {
        data.totalFrames = frameIndex;
    }

    // 计算方向统计百分比
        int totalVectors = 0;
        for (int i = 0; i < bins; i++)
        {
            totalVectors += data.directionCounts[i];
        }
        if (totalVectors > 0)
        {
            for (int i = 0; i < bins; i++)
            {
                data.directionPercentages[i] = (float)data.directionCounts[i] / totalVectors * 100.0f;
                float avgLength = data.directionCounts[i] > 0 ? data.directionMagnitude[i] / data.directionCounts[i] : 0.0f;
                data.directionAvgLength[i] = avgLength;
            }
        }
        
        // 调试输出：打印方向统计
        qDebug() << "=== 方向统计 ===";
        qDebug() << "Total vectors:" << totalVectors;
        for (int i = 0; i < bins; i++) {
            if (data.directionCounts[i] > 0) {
                qDebug() << i << "bin" << (i*22.5) << "-" << ((i+1)*22.5) << "deg:" 
                         << data.directionCounts[i] << "vectors," 
                         << data.directionPercentages[i] << "%," 
                         << "avg len:" << data.directionAvgLength[i];
            }
        }

    // 保存总能量用于结论
    data.totalEnergy = totalEnergy;
    data.maxFrameEnergy = maxFrameEnergy;

    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);

    qDebug() << "数据收集完成！";
    qDebug() << "视频名称：" << data.videoName;
    qDebug() << "总帧数：" << data.totalFrames;
    qDebug() << "运动向量数量：" << data.allMotionVectors.size();
    qDebug() << "关键帧数量：" << data.keyFrames.size();

    return true;
}

QImage ReportGenerator::convertAVFrameToQImage(AVFrame *frame)
{
    if (!frame)
    {
        qDebug() << "Error: frame is null";
        return QImage();
    }

    int width = frame->width;
    int height = frame->height;

    if (width <= 0 || width > 8192 || height <= 0 || height > 8192)
    {
        qDebug() << "Invalid frame dimensions: width=" << width << ", height=" << height;
        return QImage();
    }

    AVPixelFormat pixFmt = static_cast<AVPixelFormat>(frame->format);

    if (pixFmt == AV_PIX_FMT_NONE)
    {
        qDebug() << "Invalid pixel format";
        return QImage();
    }

    SwsContext *swsCtx = sws_getContext(width, height, pixFmt, width, height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsCtx)
    {
        qDebug() << "Failed to create SwsContext";
        return QImage();
    }

    try
    {
        QImage image(width, height, QImage::Format_RGB888);
        if (image.isNull())
        {
            qDebug() << "Failed to create QImage";
            sws_freeContext(swsCtx);
            return QImage();
        }

        uint8_t *dstData[4] = { image.bits(), nullptr, nullptr, nullptr };
        int dstLinesize[4] = { image.bytesPerLine(), 0, 0, 0 };

        int result = sws_scale(swsCtx, frame->data, frame->linesize, 0, height, dstData, dstLinesize);
        sws_freeContext(swsCtx);

        if (result < 0)
        {
            qDebug() << "sws_scale failed with code:" << result;
            return QImage();
        }

        return image.rgbSwapped();
    }
    catch (...)
    {
        qDebug() << "Exception in convertAVFrameToQImage";
        if (swsCtx) sws_freeContext(swsCtx);
        return QImage();
    }
}

QImage ReportGenerator::generateEnergyCurve(const ReportData &data)
{
    int width = 800;
    int height = 400;
    
    try
    {
        QImage img(width, height, QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter painter(&img);
        painter.setRenderHint(QPainter::Antialiasing);

        float maxEnergy = 0.0f;
        for (float e : data.energyPerFrame) if (e > maxEnergy) maxEnergy = e;
        if (maxEnergy <= 0) maxEnergy = 1.0f;

        painter.setPen(QPen(QColor(200, 200, 200), 1, Qt::DashLine));
        for (int i = 0; i <= 5; i++)
        {
            int y = 50 + i * 60;
            painter.drawLine(50, y, width - 50, y);
        }

        painter.setPen(QPen(QColor(60, 140, 200), 2));
        QPainterPath path;
        int n = (int)data.energyPerFrame.size();
        for (int i = 0; i < n; i++)
        {
            float x = 50.0f + (i / (float)(n > 0 ? n : 1)) * (width - 100.0f);
            float y = height - 50.0f - (data.energyPerFrame[i] / maxEnergy) * (height - 100.0f);
            if (i == 0) path.moveTo(x, y);
            else path.lineTo(x, y);
        }
        painter.drawPath(path);

        painter.setPen(Qt::black);
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.drawText(QRect(0, 10, width, 30), Qt::AlignCenter, "帧运动能量变化曲线");

        painter.setFont(QFont("Arial", 10));
        painter.drawText(QRect(0, height - 35, width, 30), Qt::AlignCenter, "帧数");
        painter.save();
        painter.rotate(-90);
        painter.drawText(QRect(-height + 20, 10, height - 40, 30), Qt::AlignCenter, "能量");
        painter.restore();

        painter.end();
        return img;
    }
    catch (...)
    {
        qDebug() << "Exception in generateEnergyCurve";
        QImage errorImg(width, height, QImage::Format_RGB32);
        errorImg.fill(Qt::white);
        return errorImg;
    }
}

QImage ReportGenerator::generateSpatialHeatmap(const ReportData &data)
{
    int gridRows = (int)data.spatialEnergyGrid.size();
    int gridCols = gridRows > 0 ? (int)data.spatialEnergyGrid[0].size() : 0;
    int cellSize = 8;
    int width = gridCols * cellSize + 100;
    int height = gridRows * cellSize + 100;
    
    try
    {
        QImage img(width, height, QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter painter(&img);

        float maxEnergy = 0.0f;
        for (const auto &row : data.spatialEnergyGrid)
            for (float e : row)
                if (e > maxEnergy) maxEnergy = e;
        if (maxEnergy <= 0) maxEnergy = 1.0f;

        for (int y = 0; y < gridRows; y++)
        {
            for (int x = 0; x < gridCols; x++)
            {
                float val = data.spatialEnergyGrid[y][x] / maxEnergy;
                int r, g, b;
                if (val < 0.25f) { r = 0; g = 0; b = (int)(val * 4 * 255); }
                else if (val < 0.5f) { float t = (val - 0.25f) * 4; r = 0; g = (int)(t * 255); b = 255; }
                else if (val < 0.75f) { float t = (val - 0.5f) * 4; r = (int)(t * 255); g = 255; b = (int)((1 - t) * 255); }
                else { float t = (val - 0.75f) * 4; r = 255; g = (int)((1 - t) * 255); b = 0; }
                painter.fillRect(50 + x * cellSize, 50 + y * cellSize, cellSize, cellSize, QColor(r, g, b));
            }
        }

        painter.setPen(QPen(QColor(100, 100, 100), 1));
        for (int x = 0; x <= gridCols; x++)
            painter.drawLine(50 + x * cellSize, 50, 50 + x * cellSize, 50 + gridRows * cellSize);
        for (int y = 0; y <= gridRows; y++)
            painter.drawLine(50, 50 + y * cellSize, 50 + gridCols * cellSize, 50 + y * cellSize);

        painter.setPen(Qt::black);
        painter.setFont(QFont("Arial", 14, QFont::Bold));
        painter.drawText(QRect(0, 10, width, 30), Qt::AlignCenter, "空间运动热力图");

        painter.end();
        return img;
    }
    catch (...)
    {
        qDebug() << "Exception in generateSpatialHeatmap";
        QImage errorImg(width, height, QImage::Format_RGB32);
        errorImg.fill(Qt::white);
        return errorImg;
    }
}

QImage ReportGenerator::generateDirectionRose(const ReportData &data)
{
    int size = 700; // 更大的画布
    
    try {
        QImage img(size, size, QImage::Format_ARGB32);
        img.fill(Qt::white);
        QPainter painter(&img);
        painter.setRenderHint(QPainter::Antialiasing);
        
        QPointF center(size / 2.0, size / 2.0);
        double maxRadius = size * 0.43; // 更大的半径
        int numSectors = 16;
        double step = 360.0 / numSectors;
        
        // 获取统计数据：用magnitude总和而不是count！
        std::vector<double> magnitudes(numSectors, 0.0);
        for (int i = 0; i < numSectors; i++) {
            magnitudes[i] = data.directionMagnitude[i];
        }
        
        double maxMagnitude = 0.0;
        for (double m : magnitudes) {
            if (m > maxMagnitude) maxMagnitude = m;
        }
        if (maxMagnitude <= 0) maxMagnitude = 1.0;
        
        // 调试输出所有bin的数值
        qDebug() << "=== Rose Diagram Bins ===";
        for (int i = 0; i < numSectors; i++) {
            qDebug() << "Bin" << i << "count" << data.directionCounts[i] << "magnitude" << magnitudes[i];
        }
        
        // 画背景圆环和刻度标注
        painter.setPen(QPen(QColor(200, 200, 200), 1.5, Qt::DashLine));
        for (int i = 1; i <= 5; i++) {
            double r = i * (maxRadius / 5.0);
            painter.drawEllipse(center, r, r);
            
            // 添加刻度数值标注（在右下角）
            double labelAngle = -M_PI / 4; // 45度方向
            double labelX = center.x() + r * cos(labelAngle);
            double labelY = center.y() + r * sin(labelAngle);
            painter.setPen(QColor(100, 100, 100));
            painter.setFont(QFont("Arial", 10));
            painter.drawText(QRectF(labelX - 20, labelY - 10, 40, 20), 
                            Qt::AlignCenter, 
                            QString::number(i * 20) + "%");
        }
        
        // 画主方向轴线
        painter.setPen(QPen(QColor(150, 150, 150), 1.5));
        for (int i = 0; i < 8; i++) {
            double angle = i * 45.0;
            double rad = qDegreesToRadians(-angle);
            double x = center.x() + maxRadius * cos(rad);
            double y = center.y() + maxRadius * sin(rad);
            painter.drawLine(center, QPointF(x, y));
        }
        
        // 绘制玫瑰图
        for (int i = 0; i < numSectors; ++i) {
            if (magnitudes[i] <= 0) continue;
            
            double startAngle = i * step;
            double radius = (magnitudes[i] / maxMagnitude) * maxRadius;
            
            // 计算扇形路径
            QPainterPath path;
            path.moveTo(center);
            
            // 添加弧线（分成10段更平滑）
            for (int seg = 0; seg <= 10; ++seg) {
                double angle = startAngle + seg * (step / 10.0);
                double rad = qDegreesToRadians(-angle); // Qt坐标系角度反向！
                double x = center.x() + radius * cos(rad);
                double y = center.y() + radius * sin(rad);
                if (seg == 0) path.moveTo(x, y);
                else path.lineTo(x, y);
            }
            
            path.lineTo(center);
            
            // 填充颜色
            QColor color = QColor::fromHsv(i * 360 / numSectors, 200, 200, 180);
            painter.fillPath(path, color);
            painter.setPen(QPen(color.darker(120), 1.5));
            painter.drawPath(path);
        }
        
        // 添加方向标签
        painter.setPen(Qt::black);
        painter.setFont(QFont("Arial", 11));
        QString labels[] = {"E", "NE", "N", "NW", "W", "SW", "S", "SE"};
        for (int i = 0; i < 8; i++) {
            double angle = i * 45.0;
            double rad = qDegreesToRadians(-angle);
            double x = center.x() + (maxRadius + 30) * cos(rad);
            double y = center.y() + (maxRadius + 30) * sin(rad);
            painter.drawText(QRectF(x - 20, y - 10, 40, 20), Qt::AlignCenter, labels[i]);
        }
        
        // 标题
        painter.setFont(QFont("Arial", 15, QFont::Bold));
        painter.drawText(QRectF(0, 10, size, 30), Qt::AlignCenter, "运动方向玫瑰图");
        
        painter.end();
        return img;
    } catch (...) {
        qDebug() << "Exception in generateDirectionRose";
        QImage errorImg(size, size, QImage::Format_RGB32);
        errorImg.fill(Qt::white);
        return errorImg;
    }
}

QImage ReportGenerator::generateQPCurve(const ReportData &data)
{
    int width = 800;
    int height = 400;
    
    try
    {
        QImage img(width, height, QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter painter(&img);
        painter.setRenderHint(QPainter::Antialiasing);

        painter.setPen(QPen(QColor(200, 200, 200), 1, Qt::DashLine));
        for (int i = 0; i <= 5; i++)
        {
            int y = 50 + i * 60;
            painter.drawLine(50, y, width - 50, y);
        }

        painter.setPen(QPen(QColor(200, 80, 80), 2));
        QPainterPath path;
        path.moveTo(50, height / 2);
        path.lineTo(width - 50, height / 2);
        painter.drawPath(path);

        painter.setPen(Qt::black);
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.drawText(QRect(0, 10, width, 30), Qt::AlignCenter, "QP统计（暂不支持）");

        painter.end();
        return img;
    }
    catch (...)
    {
        qDebug() << "Exception in generateQPCurve";
        QImage errorImg(width, height, QImage::Format_RGB32);
        errorImg.fill(Qt::white);
        return errorImg;
    }
}

QString ReportGenerator::fillTemplate(const ReportData &data, const QString tempDir)
{
    QString reportId = QUuid::createUuid().toString();
    QString reportDate = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString tempDirFixed = tempDir;
    tempDirFixed.replace("\\", "/");

    // 生成方向统计表
    QString directionTable;
    const int bins = 16;
    QString directionNames[] = {
        "0°", "22.5°", "45°", "67.5°",
        "90°", "112.5°", "135°", "157.5°",
        "180°", "202.5°", "225°", "247.5°",
        "270°", "292.5°", "315°", "337.5°"
    };
    for (int i = 0; i < bins; i++)
    {
        float startAngle = i * 22.5f;
        float endAngle = startAngle + 22.5f;
        directionTable += QString("<tr><td>%1-%2°</td><td>%3</td><td>%4</td><td>%5</td></tr>")
            .arg(QString::number(startAngle, 'f', 1))
            .arg(QString::number(endAngle, 'f', 1))
            .arg(data.directionCounts[i])
            .arg(QString::number(data.directionPercentages[i], 'f', 2))
            .arg(QString::number(data.directionAvgLength[i], 'f', 2));
    }

    // 生成编码模式表（简化版，主要统计I/P/B）
    int totalFrames = data.frameTypeCounts[0] + data.frameTypeCounts[1] + data.frameTypeCounts[2];
    QString encodingTable;
    if (totalFrames > 0)
    {
        encodingTable += QString("<tr><td>I帧（关键帧）</td><td>%1</td><td>%2</td></tr>")
            .arg(data.frameTypeCounts[0])
            .arg(QString::number(100.0f * data.frameTypeCounts[0] / totalFrames, 'f', 2));
        encodingTable += QString("<tr><td>P帧（前向预测）</td><td>%1</td><td>%2</td></tr>")
            .arg(data.frameTypeCounts[1])
            .arg(QString::number(100.0f * data.frameTypeCounts[1] / totalFrames, 'f', 2));
        encodingTable += QString("<tr><td>B帧（双向预测）</td><td>%1</td><td>%2</td></tr>")
            .arg(data.frameTypeCounts[2])
            .arg(QString::number(100.0f * data.frameTypeCounts[2] / totalFrames, 'f', 2));
    }

    // 生成关键帧HTML
    QString keyFramesHtml;
    for (size_t i = 0; i < data.keyFrames.size(); i++)
    {
        keyFramesHtml += QString("<img src=\"file:///%1/keyframe_%2.png\" style=\"margin:10px; max-width:200px; box-shadow:0 2px 6px rgba(0,0,0,0.1); border:1px solid #ccc;\">")
            .arg(tempDirFixed)
            .arg(i);
    }
    if (keyFramesHtml.isEmpty())
    {
        keyFramesHtml = "<p>未获取到关键帧快照</p>";
    }

    // 生成附录数据表格
    QString appendixTable;
    int sampleInterval = std::max(1, (int)data.energyPerFrame.size() / 10);
    for (int i = 0; i < (int)data.energyPerFrame.size(); i += sampleInterval)
    {
        int frameNum = i;
        float energy = data.energyPerFrame[i];
        appendixTable += QString("<tr><td>%1</td><td>%2</td><td>%3</td></tr>")
            .arg(frameNum)
            .arg(QString::number(energy, 'f', 2))
            .arg(QString::number(frameNum / data.fps, 'f', 2));
    }

    // 生成结论
    QString conclusion;
    float avgEnergy = data.totalFrames > 0 ? data.totalEnergy / data.totalFrames : 0.0f;
    conclusion += "<p>本视频分析报告基于H.264/HEVC压缩域数据生成。</p>";
    conclusion += QString("<p>• 视频时长：%1秒，总帧数：%2，平均帧率：%3fps</p>")
        .arg(QString::number(data.durationSec, 'f', 1))
        .arg(data.totalFrames)
        .arg(QString::number(data.fps, 'f', 1));
    conclusion += QString("<p>• 平均运动能量：%1，单帧最大能量：%2</p>")
        .arg(QString::number(avgEnergy, 'f', 2))
        .arg(QString::number(data.maxFrameEnergy, 'f', 2));
    conclusion += QString("<p>• 运动向量总数：%1，有效分析帧：%2</p>")
        .arg((int)data.allMotionVectors.size())
        .arg((int)data.energyPerFrame.size());
    
    if (data.frameTypeCounts[0] > 0)
    {
        conclusion += QString("<p>• 关键帧数量：%1（占比%2%），视频编码质量良好</p>")
            .arg(data.frameTypeCounts[0])
            .arg(QString::number(100.0f * data.frameTypeCounts[0] / totalFrames, 'f', 1));
    }
    
    conclusion += "<p>总体分析：该视频的运动强度适中，编码模式分布合理，适合用于视频编码研究和优化分析。</p>";

    QString html = R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>运动热力与编码行为分析报告</title>
    <style>
        @page { size: A4; margin: 1.2cm; }
        body { font-family: "Segoe UI", "Roboto", "Times New Roman", serif; font-size: 10pt; line-height: 1.3; color: #1a1a1a; background: white; margin: 0; padding: 0.5cm; }
        .cover { text-align: center; margin-top: 3cm; page-break-after: always; }
        .cover h1 { font-size: 22pt; margin-bottom: 0.3cm; }
        .cover .subtitle { font-size: 12pt; color: #2c3e50; margin-top: 0.6cm; }
        .cover .date { margin-top: 2cm; font-size: 11pt; }
        .toc { page-break-after: always; }
        .toc h2 { font-size: 14pt; border-bottom: 1px solid #aaa; margin-bottom: 0.4cm; padding-bottom: 0.1cm; }
        .toc ul { list-style: none; padding-left: 0; }
        .toc li { margin: 0.2cm 0; }
        .toc a { text-decoration: none; color: #1a1a1a; }
        .section { margin-bottom: 0.6cm; page-break-inside: avoid; }
        .section-title { font-size: 13pt; font-weight: bold; margin: 0.5cm 0 0.3cm 0; border-left: 3px solid #2c7da0; padding-left: 0.5cm; }
        .subsection-title { font-size: 12pt; margin: 0.4cm 0 0.2cm 0; padding-left: 0.2cm; }
        .info-table { width: 100%; border-collapse: collapse; margin: 0.4cm 0; }
        .info-table td { padding: 0.25cm 0.3cm; border-bottom: 1px solid #ddd; }
        .info-table td:first-child { width: 30%; font-weight: bold; background-color: #f8f8f8; }
        .stat-grid { display: flex; flex-wrap: wrap; gap: 10px; margin: 0.4cm 0; }
        .stat-card { background: #fafafa; border: 1px solid #ccc; border-radius: 6px; padding: 0.3cm 0.2cm; width: 30%; text-align: center; }
        .stat-value { font-size: 16pt; font-weight: bold; color: #2c7da0; }
        .figure { text-align: center; margin: 0.4cm 0; }
        .figure img { max-width: 100%; box-shadow: 0 2px 6px rgba(0,0,0,0.1); border: 1px solid #ccc; }
        .caption { font-size: 9pt; color: #555; margin-top: 0.2cm; }
        .data-table { width: 100%; border-collapse: collapse; font-size: 9pt; margin: 0.4cm 0; }
        .data-table th { background-color: #eef4f8; border: 1px solid #aaa; padding: 0.25cm 0.2cm; }
        .data-table td { border: 1px solid #ccc; padding: 0.2cm 0.2cm; text-align: center; }
        .appendix { page-break-before: always; padding-top: 0.3cm; }
        .footer { text-align: center; font-size: 8pt; color: #777; border-top: 1px solid #ccc; padding-top: 0.3cm; margin-top: 0.8cm; }
        p { margin: 0.3cm 0; }
        .conclusion { background-color: #f0f8ff; padding: 0.5cm; border-radius: 6px; border-left: 4px solid #2c7da0; margin: 0.5cm 0; }
    </style>
</head>
<body>

<div class="cover">
    <h1>运动热力与编码行为分析报告</h1>
    <div class="subtitle">基于 H.264/HEVC 压缩域特征</div>
    <div class="date">
        <p>报告编号：{{REPORT_ID}}<br>生成日期：{{REPORT_DATE}}</p>
    </div>
</div>

<div class="toc">
    <h2>目录</h2>
    <ul>
        <li><a href="#basic">1. 基础信息</a></li>
        <li><a href="#motion">2. 运动强度统计</a></li>
        <li><a href="#direction">3. 运动方向分析</a></li>
        <li><a href="#qp">4. QP 编码质量统计</a></li>
        <li><a href="#encoding">5. 编码模式分析</a></li>
        <li><a href="#spatial">6. 空间热力图</a></li>
        <li><a href="#keyframes">7. 关键帧快照</a></li>
        <li><a href="#conclusion">8. 结论与建议</a></li>
        <li><a href="#appendix">9. 附录</a></li>
    </ul>
</div>

<div class="section" id="basic">
    <h2 class="section-title">1. 基础信息</h2>
    <table class="info-table">
        <tr><td>视频文件</td><td>{{VIDEO_NAME}}</td></tr>
        <tr><td>时长</td><td>{{DURATION}} 秒</td></tr>
        <tr><td>分辨率</td><td>{{WIDTH}} x {{HEIGHT}}</td></tr>
        <tr><td>帧率</td><td>{{FPS}} fps</td></tr>
        <tr><td>总帧数</td><td>{{TOTAL_FRAMES}}</td></tr>
        <tr><td>编码格式</td><td>{{CODEC}}</td></tr>
    </table>
</div>

<div class="section" id="motion">
    <h2 class="section-title">2. 运动强度统计</h2>
    <div class="figure">
        <img src="file:///{{TEMP_DIR}}/energy_curve.png" alt="能量曲线">
        <div class="caption">图1：帧运动能量变化曲线</div>
    </div>
</div>

<div class="section" id="direction">
    <h2 class="section-title">3. 运动方向分析</h2>
    <div class="figure">
        <img src="file:///{{TEMP_DIR}}/direction_rose.png" alt="方向玫瑰图">
        <div class="caption">图2：运动方向玫瑰图</div>
    </div>
    
    <h3 class="subsection-title">运动方向统计数据表</h3>
    <table class="data-table">
        <tr><th>角度区间</th><th>向量数量</th><th>占比(%)</th><th>平均长度</th></tr>
        {{DIRECTION_TABLE_ROWS}}
    </table>
</div>

<div class="section" id="qp">
    <h2 class="section-title">4. QP 编码质量统计</h2>
    <div class="figure">
        <img src="file:///{{TEMP_DIR}}/qp_curve.png" alt="QP曲线">
        <div class="caption">图3：QP统计曲线（暂不支持）</div>
    </div>
    <p style="font-size:9pt;color:#666;">注：本功能需要编码器导出 QP 数据，当前版本暂不支持。</p>
</div>

<div class="section" id="encoding">
    <h2 class="section-title">5. 编码模式分析</h2>
    <div class="stat-grid">
        <div class="stat-card">
            <div class="stat-value">{{I_FRAMES}}</div>
            <div>I 帧</div>
        </div>
        <div class="stat-card">
            <div class="stat-value">{{P_FRAMES}}</div>
            <div>P 帧</div>
        </div>
        <div class="stat-card">
            <div class="stat-value">{{B_FRAMES}}</div>
            <div>B 帧</div>
        </div>
    </div>
    
    <h3 class="subsection-title">编码模式详细统计表</h3>
    <table class="data-table">
        <tr><th>帧类型</th><th>数量</th><th>占比(%)</th></tr>
        {{ENCODING_TABLE_ROWS}}
    </table>
</div>

<div class="section" id="spatial">
    <h2 class="section-title">6. 空间热力图</h2>
    <div class="figure">
        <img src="file:///{{TEMP_DIR}}/spatial_heatmap.png" alt="空间热力图">
        <div class="caption">图4：空间运动热力图</div>
    </div>
</div>

<div class="section" id="keyframes">
    <h2 class="section-title">7. 关键帧快照</h2>
    <div class="figure">
        {{KEY_FRAMES_IMAGES}}
    </div>
    <p style="font-size:9pt;color:#666;">注：以上为视频均匀分布的关键帧缩略图（最多5帧）。</p>
</div>

<div class="section" id="conclusion">
    <h2 class="section-title">8. 结论与建议</h2>
    <div class="conclusion">
        {{CONCLUSION_TEXT}}
    </div>
</div>

<div class="section appendix" id="appendix">
    <h2 class="section-title">9. 附录：采样数据表</h2>
    <table class="data-table">
        <tr><th>帧号</th><th>运动能量</th><th>时间(秒)</th></tr>
        {{APPENDIX_TABLE_ROWS}}
    </table>
    <p style="font-size:9pt;color:#666;">注：本表为采样数据，每帧数据可根据需要导出为CSV文件。</p>
</div>

<div class="footer">
    <p>本报告由 RinzePlayer 自动生成 | {{REPORT_DATE}}</p>
</div>

</body>
</html>
)";

    html.replace("{{REPORT_ID}}", reportId);
    html.replace("{{REPORT_DATE}}", reportDate);
    html.replace("{{VIDEO_NAME}}", data.videoName.toHtmlEscaped());
    html.replace("{{DURATION}}", QString::number(data.durationSec, 'f', 2));
    html.replace("{{WIDTH}}", QString::number(data.width));
    html.replace("{{HEIGHT}}", QString::number(data.height));
    html.replace("{{FPS}}", QString::number(data.fps, 'f', 2));
    html.replace("{{TOTAL_FRAMES}}", QString::number(data.totalFrames));
    html.replace("{{CODEC}}", data.codecName);
    html.replace("{{I_FRAMES}}", QString::number(data.frameTypeCounts[0]));
    html.replace("{{P_FRAMES}}", QString::number(data.frameTypeCounts[1]));
    html.replace("{{B_FRAMES}}", QString::number(data.frameTypeCounts[2]));
    html.replace("{{TEMP_DIR}}", tempDirFixed);
    html.replace("{{DIRECTION_TABLE_ROWS}}", directionTable);
    html.replace("{{ENCODING_TABLE_ROWS}}", encodingTable);
    html.replace("{{KEY_FRAMES_IMAGES}}", keyFramesHtml);
    html.replace("{{APPENDIX_TABLE_ROWS}}", appendixTable);
    html.replace("{{CONCLUSION_TEXT}}", conclusion);

    return html;
}

bool ReportGenerator::htmlToPdf(const QString &htmlContent, const QString &pdfPath, const QString &baseDir)
{
    try
    {
        QWebEngineView view;

        QEventLoop loop;
        bool loadFinished = false;

        QObject::connect(&view, &QWebEngineView::loadFinished, [&](bool ok)
        {
            loadFinished = ok;
            loop.quit();
        });

        QUrl baseUrl = QUrl::fromLocalFile(baseDir + "/");
        view.setHtml(htmlContent, baseUrl);

        QTimer::singleShot(30000, &loop, &QEventLoop::quit);
        loop.exec();

        if (!loadFinished)
        {
            qDebug() << "HTML加载失败！";
            return false;
        }

        bool pdfSaved = false;
        QEventLoop saveLoop;

        view.page()->printToPdf([&](const QByteArray &pdfData)
        {
            try
            {
                QFile file(pdfPath);
                if (file.open(QIODevice::WriteOnly))
                {
                    file.write(pdfData);
                    file.close();
                    pdfSaved = true;
                }
            }
            catch (...)
            {
                qDebug() << "Exception while writing PDF file";
            }
            saveLoop.quit();
        }, QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Portrait, QMarginsF(12, 12, 12, 12)));

        QTimer::singleShot(30000, &saveLoop, &QEventLoop::quit);
        saveLoop.exec();

        return pdfSaved;
    }
    catch (...)
    {
        qDebug() << "Exception in htmlToPdf";
        return false;
    }
}
