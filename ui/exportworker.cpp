// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "exportworker.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QImage>
#include <QPainter>
#include <cmath>
#include <map>
#include <set>

ExportWorker::ExportWorker(QObject* parent)
    : QObject(parent), m_isExporting(false), m_shouldCancel(false), m_frameCount(0), m_totalFrames(0) {
}

ExportWorker::~ExportWorker() {
    cancelExport();
}

void ExportWorker::setSettings(const ExportSettings& settings) {
    m_settings = settings;
}

void ExportWorker::cancelExport() {
    m_shouldCancel = true;
}

void ExportWorker::process() {
    m_isExporting = true;
    m_shouldCancel = false;
    m_frameCount = 0;

    emit progressUpdated(0, "正在准备导出...");
    qDebug() << "[Export] 开始从:" << m_settings.sourcePath << "导出到:" << m_settings.outputPath;

    if (!QFile::exists(m_settings.sourcePath)) {
        m_isExporting = false;
        emit exportFinished(false, "源文件不存在！");
        emit finished();
        return;
    }

    if (QFile::exists(m_settings.outputPath)) {
        QFile::remove(m_settings.outputPath);
    }

    QString ffmpegPath = findFFmpeg();
    if (ffmpegPath.isEmpty()) {
        m_isExporting = false;
        emit exportFinished(false, "找不到 ffmpeg.exe！请确保 FFmpeg 已安装并配置到环境变量中，或者位于 D:\\FFmpegLIB\\bin\\ 目录下！");
        emit finished();
        return;
    }
    qDebug() << "[Export] 使用 FFmpeg:" << ffmpegPath;

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        m_isExporting = false;
        emit exportFinished(false, "无法创建临时目录！");
        emit finished();
        return;
    }

    QString frameDir = tempDir.filePath("frames");
    QDir().mkpath(frameDir);

    emit progressUpdated(5, "正在解码视频并渲染可视化...");
    
    if (!decodeAndRender(frameDir)) {
        m_isExporting = false;
        emit exportFinished(false, "解码或渲染失败！");
        emit finished();
        return;
    }

    if (m_shouldCancel) {
        m_isExporting = false;
        emit exportFinished(false, "导出已取消！");
        emit finished();
        return;
    }

    emit progressUpdated(80, "正在编码视频...");

    QStringList args;
    args << "-y" << "-framerate" << "30" << "-i" << frameDir + "/frame_%06d.png";

    if (m_settings.includeAudio) {
        args << "-i" << m_settings.sourcePath << "-c:a" << "aac" << "-map" << "0:v:0" << "-map" << "1:a:0";
    }

    args << "-c:v" << "libx264" << "-pix_fmt" << "yuv420p" << "-preset" << "medium" << "-crf" << "23" << m_settings.outputPath;

    QProcess process;
    process.start(ffmpegPath, args);

    if (!process.waitForStarted(30000)) {
        m_isExporting = false;
        emit exportFinished(false, "无法启动 FFmpeg 编码！错误: " + process.errorString());
        emit finished();
        return;
    }

    while (!process.waitForFinished(500)) {
        if (m_shouldCancel) {
            process.kill();
            m_isExporting = false;
            emit exportFinished(false, "导出已取消！");
            emit finished();
            return;
        }
    }

    m_isExporting = false;

    if (process.exitCode() == 0) {
        emit progressUpdated(100, "导出完成！");
        emit exportFinished(true, QString("成功导出视频到: %1").arg(m_settings.outputPath));
    } else {
        QString error = QString::fromLocal8Bit(process.readAllStandardError());
        qDebug() << "[Export] FFmpeg 编码错误:" << error;
        emit exportFinished(false, QString("编码失败！FFmpeg 输出: %1").arg(error));
    }

    emit finished();
}

bool ExportWorker::decodeAndRender(const QString& frameDir) {
    qDebug() << "[Export] decodeAndRender 开始，源路径：" << m_settings.sourcePath;
    
    AVFormatContext* fmtCtx = nullptr;
    int ret = avformat_open_input(&fmtCtx, m_settings.sourcePath.toLocal8Bit().constData(), nullptr, nullptr);
    if (ret < 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "[Export] 无法打开输入文件！错误：" << errStr;
        return false;
    }

    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "[Export] 无法找到流信息！错误：" << errStr;
        avformat_close_input(&fmtCtx);
        return false;
    }

    int videoStreamIdx = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIdx = static_cast<int>(i);
            break;
        }
    }

    if (videoStreamIdx < 0) {
        qDebug() << "[Export] 未找到视频流！";
        avformat_close_input(&fmtCtx);
        return false;
    }

    AVCodecParameters* codecPar = fmtCtx->streams[videoStreamIdx]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        qDebug() << "[Export] 未找到合适的解码器！codec_id：" << codecPar->codec_id;
        avformat_close_input(&fmtCtx);
        return false;
    }

    qDebug() << "[Export] 找到解码器：" << avcodec_get_name(codecPar->codec_id);

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        qDebug() << "[Export] 无法分配解码器上下文！";
        avformat_close_input(&fmtCtx);
        return false;
    }

    ret = avcodec_parameters_to_context(codecCtx, codecPar);
    if (ret < 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "[Export] 无法复制解码器参数！错误：" << errStr;
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return false;
    }

    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "flags2", "+export_mvs", 0);
    av_dict_set(&opts, "threads", "auto", 0);

    ret = avcodec_open2(codecCtx, codec, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        char errStr[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errStr, sizeof(errStr));
        qDebug() << "[Export] 无法打开解码器！错误：" << errStr;
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        return false;
    }

    int width = codecCtx->width;
    int height = codecCtx->height;
    qDebug() << "[Export] 视频尺寸：" << width << "x" << height << ", 格式：" << av_get_pix_fmt_name(codecCtx->pix_fmt);

    if (m_settings.exportHeatmap) {
        m_heatmapState.init(width, height);
    }
    if (m_settings.exportTrail) {
        m_trailState.init();
    }
    if (m_settings.exportHistory) {
        m_historyState.init(width, height);
    }

    m_totalFrames = 0;
    if (fmtCtx->streams[videoStreamIdx]->nb_frames > 0) {
        m_totalFrames = static_cast<int>(fmtCtx->streams[videoStreamIdx]->nb_frames);
        qDebug() << "[Export] 总帧数：" << m_totalFrames;
    } else {
        m_totalFrames = 1000;
        qDebug() << "[Export] 未知帧数，估算为" << m_totalFrames;
    }

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    int frameIndex = 0;
    while (av_read_frame(fmtCtx, pkt) >= 0 && !m_shouldCancel) {
        if (pkt->stream_index == videoStreamIdx) {
            ret = avcodec_send_packet(codecCtx, pkt);
            if (ret < 0) {
                char errStr[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errStr, sizeof(errStr));
                qDebug() << "[Export] 发送数据包失败！错误：" << errStr;
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(codecCtx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    char errStr[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(ret, errStr, sizeof(errStr));
                    qDebug() << "[Export] 解码帧失败！错误：" << errStr;
                    break;
                }

                std::vector<MotionVector> mvs;
                int frameType = 0;

                if (frame->pict_type == AV_PICTURE_TYPE_I) {
                    frameType = 0;
                } else if (frame->pict_type == AV_PICTURE_TYPE_P) {
                    frameType = 1;
                } else if (frame->pict_type == AV_PICTURE_TYPE_B) {
                    frameType = 2;
                }

                AVFrameSideData* sd = av_frame_get_side_data(frame, AV_FRAME_DATA_MOTION_VECTORS);
                if (sd) {
                    const AVMotionVector* avMvs = reinterpret_cast<const AVMotionVector*>(sd->data);
                    int mvCount = sd->size / sizeof(AVMotionVector);
                    for (int i = 0; i < mvCount; ++i) {
                        MotionVector mv;
                        mv.dst_x = avMvs[i].dst_x;
                        mv.dst_y = avMvs[i].dst_y;
                        mv.motion_x = avMvs[i].motion_x;
                        mv.motion_y = avMvs[i].motion_y;
                        mv.w = avMvs[i].w;
                        mv.h = avMvs[i].h;
                        mv.source = avMvs[i].source;
                        mv.flags = avMvs[i].flags;
                        mvs.push_back(mv);
                    }
                    if (frameIndex % 30 == 0) {
                        qDebug() << "[Export] 帧" << frameIndex << "找到" << mvs.size() << "个运动向量";
                    }
                }

                QImage image = convertAVFrameToQImage(frame);
                if (image.isNull()) {
                    qDebug() << "[Export] 转换帧为QImage失败！帧" << frameIndex;
                } else {
                    if (m_settings.exportHeatmap) {
                        m_heatmapState.update(mvs);
                    }
                    if (m_settings.exportTrail) {
                        m_trailState.update(mvs);
                    }
                    if (m_settings.exportHistory) {
                        m_historyState.update(mvs, width, height);
                    }

                    renderVisualizations(image, mvs, frameType, width, height);

                    QString framePath = QString("%1/frame_%2.png").arg(frameDir).arg(frameIndex++, 6, 10, QChar('0'));
                    if (!image.save(framePath)) {
                        qDebug() << "[Export] 保存帧失败！" << framePath;
                    }
                    m_frameCount = frameIndex;

                    if (frameIndex % 30 == 0) {
                        int progress = 5 + static_cast<int>((frameIndex * 75.0) / m_totalFrames);
                        if (progress > 80) progress = 80;
                        emit progressUpdated(progress, QString("正在处理第 %1 帧...").arg(frameIndex));
                    }
                }
            }
        }
        av_packet_unref(pkt);
    }

    qDebug() << "[Export] 解码完成，共处理" << frameIndex << "帧";

    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);

    return !m_shouldCancel;
}

QImage ExportWorker::convertAVFrameToQImage(AVFrame* frame) {
    if (!frame) {
        qDebug() << "[Export] convertAVFrameToQImage: frame is null!";
        return QImage();
    }
    
    int width = frame->width;
    int height = frame->height;
    AVPixelFormat pixFmt = static_cast<AVPixelFormat>(frame->format);
    
    // 暂时移除帧索引的日志，因为不在作用域内
    // if (frameIndex % 30 == 0) {
    //     qDebug() << "[Export] convertAVFrameToQImage: 输入格式" << av_get_pix_fmt_name(pixFmt) << "，尺寸" << width << "x" << height;
    // }

    // 如果是无效格式，返回空图像
    if (pixFmt == AV_PIX_FMT_NONE || width <= 0 || height <= 0) {
        qDebug() << "[Export] convertAVFrameToQImage: 无效的帧参数";
        return QImage();
    }

    SwsContext* swsCtx = sws_getContext(
        width, height, pixFmt,
        width, height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!swsCtx) {
        qDebug() << "[Export] convertAVFrameToQImage: 无法创建 sws 上下文";
        return QImage();
    }

    QImage image(width, height, QImage::Format_RGB888);
    uint8_t* dstData[4] = {image.bits(), nullptr, nullptr, nullptr};
    int dstLinesize[4] = {image.bytesPerLine(), 0, 0, 0};

    int result = sws_scale(swsCtx, frame->data, frame->linesize, 0, height, dstData, dstLinesize);
    sws_freeContext(swsCtx);

    if (result < 0) {
        qDebug() << "[Export] convertAVFrameToQImage: sws_scale 失败";
        return QImage();
    }

    return image.rgbSwapped();
}

void ExportWorker::renderVisualizations(QImage& image, const std::vector<MotionVector>& mvs, int frameType, int width, int height) {
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_settings.exportHeatmap) {
        renderHeatmap(painter, width, height);
    }
    if (m_settings.exportTrail) {
        renderTrail(painter, width, height);
    }
    if (m_settings.exportHistory) {
        renderHistory(painter, width, height);
    }
    if (m_settings.exportMotionVectors) {
        renderMotionVectors(painter, mvs, width, height);
    }

    painter.end();
}

void ExportWorker::renderMotionVectors(QPainter& painter, const std::vector<MotionVector>& mvs, int width, int height) {
    const float threshold = 0.6f;
    const int gridStep = 8;
    const float angleThreshold = 10.0f * 3.14159265f / 180.0f;
    const float scale = 0.05f;

    std::set<std::pair<int, int>> occupied;
    std::map<std::pair<int, int>, MotionVector> gridToVector;
    std::map<std::pair<int, int>, float> gridToAngle;

    for (const auto& mv : mvs) {
        float mvX = static_cast<float>(mv.motion_x);
        float mvY = static_cast<float>(mv.motion_y);
        float length = std::sqrt(mvX * mvX + mvY * mvY);

        if (length >= threshold) {
            int gridX = mv.dst_x / gridStep;
            int gridY = mv.dst_y / gridStep;
            auto key = std::make_pair(gridX, gridY);
            if (!occupied.count(key)) {
                occupied.insert(key);
                gridToVector[key] = mv;
                gridToAngle[key] = std::atan2(mvY, mvX);
            }
        }
    }

    std::vector<MotionVector> processedMvs;
    for (const auto& entry : gridToVector) {
        auto key = entry.first;
        MotionVector mv = entry.second;

        float avgAngle = 0.0f;
        int count = 0;

        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) continue;
                auto neighborKey = std::make_pair(key.first + dx, key.second + dy);
                if (gridToAngle.count(neighborKey)) {
                    float angleDiff = std::abs(gridToAngle[key] - gridToAngle[neighborKey]);
                    if (angleDiff > 3.14159265f) {
                        angleDiff = 2 * 3.14159265f - angleDiff;
                    }
                    if (angleDiff <= angleThreshold) {
                        avgAngle += gridToAngle[neighborKey];
                        count++;
                    }
                }
            }
        }

        if (count > 0) {
            avgAngle += gridToAngle[key];
            avgAngle /= (count + 1);
            float mvX = static_cast<float>(mv.motion_x);
            float mvY = static_cast<float>(mv.motion_y);
            float length = std::sqrt(mvX * mvX + mvY * mvY);
            mv.motion_x = static_cast<int>(std::cos(avgAngle) * length);
            mv.motion_y = static_cast<int>(std::sin(avgAngle) * length);
        }

        processedMvs.push_back(mv);
    }

    for (const auto& mv : processedMvs) {
        float centerX = static_cast<float>(mv.dst_x);
        float centerY = static_cast<float>(mv.dst_y);
        float mvX = static_cast<float>(mv.motion_x) * scale * width / 100.0f;
        float mvY = static_cast<float>(mv.motion_y) * scale * height / 100.0f;

        float arrowTipX = centerX + mvX;
        float arrowTipY = centerY + mvY;

        float length = std::sqrt(mvX * mvX + mvY * mvY);
        if (length < 0.5f) continue;

        float dirX = mvX / length;
        float dirY = mvY / length;
        float perpX = -dirY;
        float perpY = dirX;

        float arrowLen = 6.0f;
        float arrowWidth = 3.0f;
        QPointF vTip(arrowTipX + dirX * arrowLen, arrowTipY + dirY * arrowLen);
        QPointF v1(arrowTipX + perpX * arrowWidth, arrowTipY + perpY * arrowWidth);
        QPointF v2(arrowTipX - perpX * arrowWidth, arrowTipY - perpY * arrowWidth);

        QColor arrowColor(Qt::black);
        arrowColor.setAlpha(200);

        painter.setPen(QPen(arrowColor, 2.0f));
        painter.drawLine(QPointF(centerX, centerY), QPointF(arrowTipX, arrowTipY));
        painter.drawLine(vTip, v1);
        painter.drawLine(vTip, v2);

        QColor blockColor(128, 128, 128, 100);
        painter.setBrush(QBrush(blockColor));
        painter.setPen(Qt::NoPen);
        painter.drawRect(QRectF(mv.dst_x - mv.w / 2, mv.dst_y - mv.h / 2, mv.w, mv.h));
    }
}

void ExportWorker::renderHeatmap(QPainter& painter, int width, int height) {
    QImage heatmap(width, height, QImage::Format_RGBA8888);
    heatmap.fill(QColor(0, 0, 0, 0));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            QColor color = m_heatmapState.getColor(m_heatmapState.heatmapBuffer[y][x]);
            if (color.alpha() > 0) {
                heatmap.setPixelColor(x, y, color);
            }
        }
    }

    painter.drawImage(0, 0, heatmap);
}

void ExportWorker::renderTrail(QPainter& painter, int width, int height) {
    for (const auto& trail : m_trailState.trails) {
        if (trail.size() < 2) continue;

        for (size_t i = 1; i < trail.size(); ++i) {
            float t = static_cast<float>(i) / trail.size();
            int r = static_cast<int>(255 * (1 - t));
            int g = static_cast<int>(255 * (1 - t));
            int b = static_cast<int>(255 * t);
            int a = static_cast<int>(200 * (1 - t));

            painter.setPen(QPen(QColor(r, g, b, a), 2.0f));
            painter.drawLine(trail[i-1].x, trail[i-1].y, trail[i].x, trail[i].y);
        }
    }
}

void ExportWorker::renderHistory(QPainter& painter, int width, int height) {
    QImage colorHistory(width, height, QImage::Format_RGBA8888);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uchar value = m_historyState.historyImage.bits()[y * width + x];
            if (value > 0) {
                QColor color;
                if (value < 64) {
                    color = QColor(0, 0, value * 4, 180);
                } else if (value < 128) {
                    float t = (value - 64) / 64.0f;
                    color = QColor(0, static_cast<int>(t * 255), 255, 180);
                } else if (value < 192) {
                    float t = (value - 128) / 64.0f;
                    color = QColor(static_cast<int>(t * 255), 255, static_cast<int>((1 - t) * 255), 180);
                } else {
                    float t = (value - 192) / 64.0f;
                    color = QColor(255, static_cast<int>((1 - t) * 255), 0, 180);
                }
                colorHistory.setPixelColor(x, y, color);
            } else {
                colorHistory.setPixelColor(x, y, QColor(0, 0, 0, 0));
            }
        }
    }

    painter.drawImage(0, 0, colorHistory);
}

QString ExportWorker::findFFmpeg() {
    QStringList possiblePaths;
    
    possiblePaths << "ffmpeg";
    possiblePaths << QCoreApplication::applicationDirPath() + "/ffmpeg.exe";
    possiblePaths << QCoreApplication::applicationDirPath() + "/ffmpeg";
    possiblePaths << "D:/FFmpegLIB/bin/ffmpeg.exe";
    possiblePaths << "D:\\FFmpegLIB\\bin\\ffmpeg.exe";
    possiblePaths << "C:/FFmpeg/bin/ffmpeg.exe";
    possiblePaths << "C:\\FFmpeg\\bin\\ffmpeg.exe";

    for (const QString& path : possiblePaths) {
        QFileInfo info(path);
        if (info.exists() && info.isExecutable()) {
            return info.absoluteFilePath();
        }
    }

    return QString();
}
