// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "videowidget.h"
#include "clock/globalclock.h"
#include "playbackstats.h"
#include "utils/utils.h"
#include <QFile>
#include <QDateTime>
#include <QPainter>
#include <QDebug>
#include <QMouseEvent>
#include <QTimer>
#include <cmath>
#include <set>
#include <utility>

namespace {
    std::vector<uint8_t> &texFill() {
        static std::vector<uint8_t> instance;
        return instance;
    }
    std::vector<QRect> &lastSubTexRect() {
        static std::vector<QRect> instance;
        return instance;
    }
}

VideoWidget::VideoWidget(QWidget *parent)
    : QOpenGLWidget(parent) {
    m_lastFPSUpdate = QDateTime::currentMSecsSinceEpoch();
    
    // 创建悬浮控制栏
    m_hoverControlBar = new HoverControlBar(this);
}

VideoWidget::~VideoWidget() {
    makeCurrent();
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteBuffers(1, &m_ebo);
    if (m_subTex != 0) {
        glDeleteTextures(1, &m_subTex);
    }
    for (auto &v : m_texArr) {
        if (v != 0)
            glDeleteTextures(1, &v);
    }
    doneCurrent();
}

void VideoWidget::initializeGL() {
    initializeOpenGLFunctions();

    const QString vSrcPath = ":/resource/shader.vert";
    const QString fSrcPath = ":/resource/shader.frag";
    QFile vFile(vSrcPath), fFile(fSrcPath);
    if (!vFile.open(QIODevice::ReadOnly | QIODevice::Text) || !fFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开着色器文件";
        return;
    }

    QString vSrc = vFile.readAll(), fSrc = fFile.readAll();
    if (!m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vSrc) ||
        !m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fSrc) ||
        !m_program.link()) {
        qDebug() << "着色器程序编译失败";
        qDebug() << "顶点着色器错误:" << m_program.log();
        qDebug() << "片段着色器错误:" << m_program.log();
        return;
    }
    qDebug() << "着色器程序编译成功";

    float vertices[] = {
        -1.f, 1.f, 0.f, 0.f,  // 左上
        -1.f, -1.f, 0.f, 1.f, // 左下
        1.f, -1.f, 1.f, 1.f,  // 右下
        1.f, 1.f, 1.f, 0.f    // 右上
    };

    unsigned int indices[] = {
        0, 1, 2,
        0, 2, 3
    };

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    DeviceStatus::instance().setVideoInitialized(true);
}

void VideoWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void VideoWidget::resizeEvent(QResizeEvent* event) {
    QOpenGLWidget::resizeEvent(event);
    if (m_hoverControlBar) {
        m_hoverControlBar->updatePosition();
    }
}

void VideoWidget::mousePressEvent(QMouseEvent* event) {
    QOpenGLWidget::mousePressEvent(event);
    if (m_hoverControlBar) {
        // 检查点击是否在控制栏上 - 如果在，不处理切换
        if (m_hoverControlBar->isControlsVisible() && m_hoverControlBar->geometry().contains(event->pos())) {
            return;
        }
        // 否则切换显示/隐藏
        if (m_hoverControlBar->isControlsVisible()) {
            m_hoverControlBar->hideControls();
        } else {
            m_hoverControlBar->showControls();
        }
    }
}

void VideoWidget::paintGL() {
    if (!m_vidData) {
        glClearColor(0.0627f, 0.0627f, 0.0627f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    GLint prevAlign = 0;
    GLint prevRowLen = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevAlign);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &prevRowLen);

    if (m_subData) {
        m_subData->read([&](SubRenderData &renData, int) -> bool {
            if (renData.subtitleType != SUBTITLE_NONE && (renData.frmItem.width != m_subtitleSize.width() || renData.frmItem.height != m_subtitleSize.height())) {
                m_needInitSubtitleTex = true;
                initSubtitleTex(&renData);
            }
            return updateSubTex(renData);
        });
    }

    if (m_forceClearSubtitle) {
        clearSubtitleTex();
        m_forceClearSubtitle = false;
    }

    m_vidData->read([&](VideoRenderData &renData, int) -> bool {
        AVFrame *frm = renData.frmItem.frm;
        if (frm == nullptr)
            return false;

        // 更新视频分辨率信息
        m_videoWidth = frm->width;
        m_videoHeight = frm->height;
        
        // 检查视频尺寸是否变化，如果变化则先清理旧的运动向量数据
        if (frm->width != m_frameSize.width() || frm->height != m_frameSize.height() || frm->format != m_AVPixelFormat) {
            clearMotionVectorData();
            m_needInitVideoTex = true;
            initVideoTex(&renData);
        }
        
        // 更新运动向量数据（如果运动向量可视化打开或者热力图/轨迹/历史窗口打开）
        bool needUpdateMotionData = m_showMotionVectors
                                    || m_heatmapWidget != nullptr
                                    || m_trailWidget != nullptr
                                    || m_historyWidget != nullptr;
        if (needUpdateMotionData) {
            updateMotionVectorData(&renData);
        }
        
        // 发送视频信息
        if (!m_videoInfoSent && !renData.pixelFormatName.isEmpty()) {
            emit videoInfoChanged(renData.pixelFormatName, renData.colorRangeName, renData.colorSpaceName);
            m_videoInfoSent = true;
        }

        for (int i = 0; i < 4; ++i) {
            dataArr[i] = renData.dataArr[i];
        }
        updateTex(renData.pixFormat);

        if (frm->format != PlaybackStats::instance().videoFormat) {
            PlaybackStats::instance().videoPixFormat = QString(av_get_pix_fmt_name((AVPixelFormat)frm->format));
            PlaybackStats::instance().videoFormat = frm->format;
        }
        return true;
    });

    glClearColor(0.0627f, 0.0627f, 0.0627f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    bindAllTexturesForDraw();
    m_program.bind();

    if (m_showSubtitle != m_lastShowSubtitle) {
        m_lastShowSubtitle = m_showSubtitle;
        int loc = m_program.uniformLocation("showSub");
        m_program.setUniformValue(loc, m_showSubtitle);
    }

    m_program.setUniformValue(m_program.uniformLocation("transform"), getTransformMat());
    m_program.setUniformValue(m_program.uniformLocation("brightness"), m_brightness);
    m_program.setUniformValue(m_program.uniformLocation("contrast"), m_contrast);
    m_program.setUniformValue(m_program.uniformLocation("saturation"), m_saturation);
    int colorSpaceLocation = m_program.uniformLocation("colorSpace");
    m_program.setUniformValue(colorSpaceLocation, m_colorSpace);
    
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    m_program.release();

    glPixelStorei(GL_UNPACK_ALIGNMENT, prevAlign);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, prevRowLen);

    PlaybackStats::instance().FBOSize = {size().width(), size().height()};
    PlaybackStats::instance().frameRendered();

    // 帧率计算
    m_frameCount++;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastFPSUpdate >= 1000) {
        m_currentFPS = static_cast<float>(m_frameCount) * 1000.0f / static_cast<float>(now - m_lastFPSUpdate);
        m_frameCount = 0;
        m_lastFPSUpdate = now;
    }

    // 绘制帧率显示
    glFlush();
    
    // 小窗口模式不绘制任何额外内容（节省性能）
    if (!m_miniMode) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // 绘制帧率显示
        if (m_showFPS && m_videoWidth > 0 && m_videoHeight > 0) {
        // 设置文字样式
        QFont font;
        font.setFamily("Arial");
        font.setPixelSize(24);
        font.setBold(true);
        painter.setFont(font);
        
        // FPS文本
  QString fpsText = QString("%1*%2@%3 FPS").arg(m_videoWidth).arg(m_videoHeight).arg(m_currentFPS, 0, 'f', 1);
  // 添加视频编码格式
  if (!PlaybackStats::instance().videoCodecName.isEmpty()) {
    fpsText += QString(" [%1]").arg(PlaybackStats::instance().videoCodecName);
  }
        // Buffer文本
        QString bufferText = QString("V-Pkt:%1 V-Frm:%2 A-Pkt:%3 A-Frm:%4")
            .arg(m_videoPktQueueSize).arg(m_videoFrmQueueSize)
            .arg(m_audioPktQueueSize).arg(m_audioFrmQueueSize);
        // 同步文本
        QString syncText = QString("Sync: %1 ms").arg(m_syncDifference * 1000.0, 0, 'f', 1);
        // 内存文本
        QString memoryText = QString("Memory: %1 MB").arg(m_memoryUsageMB, 0, 'f', 1);
        
        QFontMetrics fm(font);
        QRect fpsRect = fm.boundingRect(fpsText);
        QRect bufferRect = fm.boundingRect(bufferText);
        QRect syncRect = fm.boundingRect(syncText);
        QRect memoryRect = fm.boundingRect(memoryText);
        
        int padding = 10;
        int lineSpacing = 5;
        int totalHeight = fpsRect.height() + bufferRect.height() + syncRect.height() + memoryRect.height() + padding * 5 + lineSpacing * 3;
        int maxWidth = qMax(qMax(qMax(fpsRect.width(), bufferRect.width()), syncRect.width()), memoryRect.width()) + padding * 2;
        
        // 绘制半透明背景
        QRect bgRect(10, 10, maxWidth, totalHeight);
        painter.setBrush(QColor(0, 0, 0, 180));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(bgRect, 5, 5);
        
        // 绘制文字
        painter.setPen(Qt::white);
        painter.drawText(20, 30 + fpsRect.height()/2, fpsText);
        painter.drawText(20, 30 + fpsRect.height() + lineSpacing + bufferRect.height()/2, bufferText);
        
        // 同步差根据正负显示不同颜色
        if (std::abs(m_syncDifference) < 0.05) {
            // 良好同步 - 绿色
            painter.setPen(QColor(0, 255, 0));
        } else if (std::abs(m_syncDifference) < 0.15) {
            // 一般同步 - 黄色
            painter.setPen(QColor(255, 255, 0));
        } else {
            // 同步偏差大 - 红色
            painter.setPen(QColor(255, 0, 0));
        }
        painter.drawText(20, 30 + fpsRect.height() + lineSpacing * 2 + bufferRect.height() + syncRect.height()/2, syncText);
        
        // 绘制内存
            painter.setPen(Qt::white);
            painter.drawText(20, 30 + fpsRect.height() + lineSpacing * 3 + bufferRect.height() + syncRect.height() + memoryRect.height()/2, memoryText);
        }
        
        painter.end();
        
        // 单开一个QPainter绘制运动向量
        if (m_showMotionVectors) {
            QPainter mvPainter(this);
            drawSimpleMotionVectors(mvPainter);
        }
    }
}

void VideoWidget::setShowSubtitle(bool val) {
    m_showSubtitle = val;
    update();
}

void VideoWidget::setXY(float newX, float newY) {
    float oldTx = m_tx, oldTy = m_ty;
    m_tx = newX;
    m_ty = newY;
    update();
}

void VideoWidget::setX(float newX) {
    float oldTx = m_tx;
    m_tx = newX;
    update();
}

void VideoWidget::setY(float newY) {
    float oldTy = m_ty;
    m_ty = newY;
    update();
}

void VideoWidget::setScale(int newScale) {
    int tmpScale = std::clamp(newScale, 2, 600);
    if (tmpScale != m_scale) {
        m_scale = tmpScale;
        update();
    }
}

void VideoWidget::setHorizontalMirror(bool val) {
    m_horizontalMirror = val;
    update();
}

void VideoWidget::setVerticalMirror(bool val) {
    m_verticalMirror = val;
    update();
}

void VideoWidget::setAngle(float angle) {
    float tmpAngle = std::fmod(angle, 360.0f);
    if (tmpAngle < 0)
        tmpAngle += 360.0f;
    if (std::fabs(tmpAngle - m_angle) > 0.05) {
        m_angle = tmpAngle;
        update();
    }
}

void VideoWidget::updateRenderData(VideoDoubleBuf *vidData, SubtitleDoubleBuf *subData) {
    m_vidData = vidData;
    m_subData = subData;
    update();
}

void VideoWidget::forceClearSubtitle() {
    m_forceClearSubtitle = true;
    update();
}

void VideoWidget::initVideoTex(VideoRenderData *renderData) {
    if (!renderData)
        return;

    AVFrame *frm = renderData->frmItem.frm;
    if (!m_needInitVideoTex || !frm)
        return;

    m_frameSize = {frm->width, frm->height};
    m_AVPixelFormat = (AVPixelFormat)frm->format;

    m_program.bind();
    int loc = m_program.uniformLocation("pixFormat");
    m_program.setUniformValue(loc, (int)renderData->pixFormat);
    loc = m_program.uniformLocation("yTex");
    m_program.setUniformValue(loc, 0);
    loc = m_program.uniformLocation("uTex");
    m_program.setUniformValue(loc, 1);
    loc = m_program.uniformLocation("vTex");
    m_program.setUniformValue(loc, 2);
    loc = m_program.uniformLocation("aTex");
    m_program.setUniformValue(loc, 3);
    m_program.release();

    VideoRenderData::PixFormat tmpFmt = renderData->pixFormat;
    for (int i = 0; i < 4; ++i) {
        GLParaArr[i] = renderData->GLParaArr[i];
        linesizeArr[i] = renderData->linesizeArr[i];
        componentSizeArr[i] = renderData->componentSizeArr[i];
        alignment = renderData->alignment;
    }

    if (tmpFmt == VideoRenderData::RGB_PACKED || tmpFmt == VideoRenderData::RGBA_PACKED ||
        tmpFmt == VideoRenderData::Y || tmpFmt == VideoRenderData::YA) {
        initTex(m_texArr[0], componentSizeArr[0], GLParaArr[0]);
        if (tmpFmt == VideoRenderData::YA) {
            initTex(m_texArr[3], componentSizeArr[1], GLParaArr[1]);
        }
    } else {
        for (int i = 0; i < 3; ++i) {
            initTex(m_texArr[i], componentSizeArr[i], GLParaArr[i]);
        }
        if (tmpFmt == VideoRenderData::RGBA_PLANAR || tmpFmt == VideoRenderData::YUVA) {
            initTex(m_texArr[3], componentSizeArr[3], GLParaArr[3]);
        }
    }
    m_needInitVideoTex = false;
}

void VideoWidget::initSubtitleTex(SubRenderData *subRenderData) {
    if (m_needInitSubtitleTex && subRenderData) {
        m_program.bind();
        int loc = m_program.uniformLocation("subTex");
        m_program.setUniformValue(loc, 4);

        m_subtitleSize.setHeight(subRenderData->frmItem.height);
        m_subtitleSize.setWidth(subRenderData->frmItem.width);
        if (m_subtitleSize.isNull()) {
            m_subtitleSize = m_frameSize;
        }

        initTex(m_subTex, m_subtitleSize, {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}, nullptr);

        loc = m_program.uniformLocation("haveSubTex");
        m_program.setUniformValue(loc, true);

        clearSubtitleTex();
        m_needInitSubtitleTex = false;
        m_program.release();
    }
}

bool VideoWidget::updateTex(VideoRenderData::PixFormat fmt) {
    if (fmt == VideoRenderData::NONE)
        return false;

    auto uploadTexture = [&](int pos, GLenum textureOffset) {
        glActiveTexture(GL_TEXTURE0 + textureOffset);
        glBindTexture(GL_TEXTURE_2D, m_texArr[textureOffset]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, linesizeArr[pos]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        componentSizeArr[pos].width(), componentSizeArr[pos].height(),
                        GLParaArr[pos][1], GLParaArr[pos][2],
                        dataArr[pos]);
    };

    if (fmt == VideoRenderData::RGB_PACKED || fmt == VideoRenderData::RGBA_PACKED ||
        fmt == VideoRenderData::Y || fmt == VideoRenderData::YA) {
        uploadTexture(0, 0);
        if (fmt == VideoRenderData::YA) {
            uploadTexture(1, 3);
        }
    } else {
        for (int pos = 0; pos < 3; ++pos) {
            uploadTexture(pos, pos);
        }
        if (fmt == VideoRenderData::RGBA_PLANAR || fmt == VideoRenderData::YUVA) {
            uploadTexture(3, 3);
        }
    }
    PlaybackStats::instance().videoSize = m_frameSize;
    return true;
}

bool VideoWidget::updateSubTex(SubRenderData &renData) {
    if (renData.uploaded) {
        return true;
    }
    if (m_subTex == 0) {
        return true;
    }

    glActiveTexture(GL_TEXTURE0 + 4);
    glBindTexture(GL_TEXTURE_2D, m_subTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    static AVSubtitleType lastSubType = SUBTITLE_NONE;

    if (renData.size == 0) {
        clearSubtitleTex();
        renData.uploaded = true;
        return true;
    }

    if (renData.subtitleType != lastSubType) {
        clearSubtitleTex();
        lastSubType = renData.subtitleType;
    }

    for (size_t i = 0; i < lastSubTexRect().size(); ++i) {
        const QRect &rect = lastSubTexRect()[i];
        int x = std::clamp(rect.x(), 0, m_frameSize.width());
        int y = std::clamp(rect.y(), 0, m_frameSize.height());
        int w = std::clamp(rect.width(), 0, m_frameSize.width() - x);
        int h = std::clamp(rect.height(), 0, m_frameSize.height() - y);

        if (w == 0 || h == 0)
            continue;

        if ((int)texFill().size() < h * w * 4) {
            texFill().assign(h * w * 4, 0);
        }
        glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, texFill().data());
    }

    lastSubTexRect() = renData.rects;

    for (size_t i = 0; i < renData.size; ++i) {
        int len = renData.linesizeArr[i];
        const QRect &rect = renData.rects[i];
        int x = rect.x();
        int y = rect.y();
        int h = rect.height();
        int w = rect.width();
        if (h == 0 || w == 0) {
            continue;
        }
        uint8_t *subtitleData = renData.dataArr[i].data();

        glPixelStorei(GL_UNPACK_ROW_LENGTH, len);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, subtitleData);
    }

    PlaybackStats::instance().subtitleSize = m_subtitleSize;
    renData.uploaded = true;
    return true;
}

void VideoWidget::initTex(GLuint &tex, const QSize &size, const std::array<unsigned int, 3> &para, uint8_t *fill) {
    if (tex != 0)
        glDeleteTextures(1, &tex);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, size.width());
    glTexImage2D(GL_TEXTURE_2D, 0, para[0], size.width(), size.height(), 0, para[1], para[2], fill);
    glBindTexture(GL_TEXTURE_2D, 0);
}

QMatrix4x4 VideoWidget::getTransformMat() {
    QMatrix4x4 mat;
    mat.setToIdentity();

    float tx = 2.f * m_tx / size().width();
    float ty = 2.f * m_ty / size().height();
    mat.translate(tx, ty, 0.0f);

    float scaleX0 = 1.0f, scaleY0 = 1.0f;
    if (m_frameSize.width() > m_frameSize.height()) {
        scaleY0 = 1.f * m_frameSize.height() / m_frameSize.width();
    } else {
        scaleX0 = 1.f * m_frameSize.width() / m_frameSize.height();
    }

    float scaleX = 1.0f, scaleY = 1.0f;
    float videoAspect = 1.f * m_frameSize.width() / m_frameSize.height();
    float widgetAspect = 1.f * size().width() / size().height();
    if (videoAspect > widgetAspect) {
        scaleY = widgetAspect / videoAspect;
    } else {
        scaleX = videoAspect / widgetAspect;
    }
    mat.scale(scaleX / scaleX0, scaleY / scaleY0, 1.0f);

    float sclX = m_scale / 100.f * (m_horizontalMirror ? -1.f : 1.f);
    float sclY = m_scale / 100.f * (m_verticalMirror ? -1.f : 1.f);
    mat.scale(sclX, sclY, 1.0f);

    float ang = m_angle * (m_horizontalMirror ^ m_verticalMirror ? -1.f : 1.f);
    mat.rotate(ang, 0, 0, 1);

    mat.scale(scaleX0, scaleY0, 1.f);

    return mat;
}

void VideoWidget::bindAllTexturesForDraw() {
    for (int i = 0; i < 4; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        if (m_texArr[i] != 0) {
            glBindTexture(GL_TEXTURE_2D, m_texArr[i]);
        } else {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    glActiveTexture(GL_TEXTURE0 + 4);
    if (m_subTex != 0) {
        glBindTexture(GL_TEXTURE_2D, m_subTex);
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glActiveTexture(GL_TEXTURE0);
}

void VideoWidget::resetTransform() {
    m_tx = 0.f;
    m_ty = 0.f;
    m_angle = 0.f;
    m_scale = 100;
    m_horizontalMirror = false;
    m_verticalMirror = false;
    update();
}

void VideoWidget::clearSubtitleTex() {
    if (m_subTex == 0) {
        return;
    }

    glActiveTexture(GL_TEXTURE0 + 4);
    glBindTexture(GL_TEXTURE_2D, m_subTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    int fillLen = m_subtitleSize.height() * m_subtitleSize.width() * 4;
    if ((int)texFill().size() < fillLen) {
        texFill().assign(fillLen, 0);
    }
    glPixelStorei(GL_UNPACK_ROW_LENGTH, m_subtitleSize.width());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_subtitleSize.width(), m_subtitleSize.height(), GL_RGBA, GL_UNSIGNED_BYTE, texFill().data());
    lastSubTexRect().clear();
}

void VideoWidget::setBrightness(float val) {
    m_brightness = std::clamp(val, -1.0f, 1.0f);
    update();
}

void VideoWidget::setContrast(float val) {
    m_contrast = std::clamp(val, 0.5f, 2.0f);
    update();
}

void VideoWidget::setSaturation(float val) {
    m_saturation = std::clamp(val, 0.0f, 2.0f);
    update();
}

void VideoWidget::setColorSpace(int colorSpace) {
    m_colorSpace = std::clamp(colorSpace, 0, 3); // 支持 Dolby Vision (3)
    update();
}

void VideoWidget::resetFilters() {
    m_brightness = 0.0f;
    m_contrast = 1.0f;
    m_saturation = 1.0f;
    update();
}

void VideoWidget::setShowFPS(bool val) {
    m_showFPS = val;
    update();
}

void VideoWidget::setMiniMode(bool isMini) {
    m_miniMode = isMini;
    if (isMini) {
        m_showFPS = false;
    }
    update();
}

void VideoWidget::updateBufferInfo(int videoPktSize, int videoFrmSize, int audioPktSize, int audioFrmSize) {
    m_videoPktQueueSize = videoPktSize;
    m_videoFrmQueueSize = videoFrmSize;
    m_audioPktQueueSize = audioPktSize;
    m_audioFrmQueueSize = audioFrmSize;
}

void VideoWidget::updateSyncInfo(double syncDifference) {
    m_syncDifference = syncDifference;
}

void VideoWidget::updateMemoryInfo(double memoryMB) {
    m_memoryUsageMB = memoryMB;
}

void VideoWidget::setShowMotionVectors(bool val) {
    m_showMotionVectors = val;
    if (!val) {
        // 关闭显示时清理数据
        clearMotionVectorData();
    }
    update();
}

void VideoWidget::setMotionVectorScale(float scale) {
    m_motionVectorScale = std::clamp(scale, 0.01f, 1.0f);
    update();
}

void VideoWidget::toggleMotionHeatmap() {
    if (!m_heatmapWidget) {
        // 创建新窗口
        m_heatmapWidget = new MotionHeatmapWidget();
        m_heatmapWidget->setWindowTitle("运动能量热力图");
        m_heatmapWidget->resize(600, 400);
        m_heatmapWidget->show();
    } else {
        // 关闭窗口
        m_heatmapWidget->close();
        m_heatmapWidget->deleteLater();
        m_heatmapWidget = nullptr;
    }
}

void VideoWidget::toggleMotionTrail() {
    if (!m_trailWidget) {
        // 创建新窗口
        m_trailWidget = new MotionTrailWidget();
        m_trailWidget->setWindowTitle("运动轨迹图");
        m_trailWidget->resize(600, 400);
        m_trailWidget->show();
    } else {
        // 关闭窗口
        m_trailWidget->close();
        m_trailWidget->deleteLater();
        m_trailWidget = nullptr;
    }
}

void VideoWidget::toggleMotionHistory() {
    if (!m_historyWidget) {
        // 创建新窗口
        m_historyWidget = new MotionHistoryWidget();
        m_historyWidget->setWindowTitle("运动历史图像");
        m_historyWidget->resize(600, 400);
        m_historyWidget->show();
    } else {
        // 关闭窗口
        m_historyWidget->close();
        m_historyWidget->deleteLater();
        m_historyWidget = nullptr;
    }
}

void VideoWidget::clearMotionVectorData() {
    // 清理所有指向可能被释放内存的指针
    m_pixFormat = VideoRenderData::NONE;
    for (int i = 0; i < 4; ++i) {
        m_mvDataArr[i] = nullptr;
        m_mvLinesizeArr[i] = 0;
        m_mvComponentSizeArr[i] = QSize();
    }
    m_currentMVs.clear();
    m_smoothedMVs.clear();
}

void VideoWidget::updateMotionVectorData(VideoRenderData* renderData) {
    if (renderData) {
        // 先保存帧数据用于计算平均颜色
        m_pixFormat = renderData->pixFormat;
        for (int i = 0; i < 4; ++i) {
            m_mvDataArr[i] = renderData->dataArr[i];
            m_mvLinesizeArr[i] = renderData->linesizeArr[i];
            m_mvComponentSizeArr[i] = renderData->componentSizeArr[i];
        }
        
        // 更新热力图数据（如果窗口打开的话）
        if (m_heatmapWidget && renderData->frmItem.frm) {
            m_heatmapWidget->updateMotionData(
                renderData->motionVectors,
                renderData->frmItem.frm->width,
                renderData->frmItem.frm->height,
                renderData->frameType
            );
        }
        
        // 更新轨迹图数据（如果窗口打开的话）
        if (m_trailWidget && renderData->frmItem.frm) {
            m_trailWidget->updateMotionData(
                renderData->motionVectors,
                renderData->frmItem.frm->width,
                renderData->frmItem.frm->height
            );
        }
        
        // 更新运动历史图像（如果窗口打开的话）
        if (m_historyWidget && renderData->frmItem.frm) {
            m_historyWidget->updateMotionData(
                renderData->motionVectors,
                renderData->frmItem.frm->width,
                renderData->frmItem.frm->height
            );
        }

        // 再保存运动向量（注意：即使这一帧没有，我们也不清空 m_currentMVs，保留上一帧）
        if (renderData->hasMotionVectors) {
            std::vector<MotionVector> newCurrentMVs;
            const float threshold = 0.6f; // 运动长度阈值，过滤掉长度 < 0.6 像素的噪声矢量
            const int gridStep = 8; // 空间下采样步长
            const float angleThreshold = 10.0f * 3.14159265f / 180.0f; // 10度转换为弧度
            
            std::set<std::pair<int, int>> occupied; // 记录已占用的网格
            std::map<std::pair<int, int>, MotionVector> gridToVector; // 网格到运动向量的映射
            std::map<std::pair<int, int>, float> gridToAngle; // 网格到角度的映射
            
            // 第一步：先收集所有符合阈值的矢量并计算角度
            for (const auto& mv : renderData->motionVectors) {
                float mvX = static_cast<float>(mv.motion_x);
                float mvY = static_cast<float>(mv.motion_y);
                float length = std::sqrt(mvX*mvX + mvY*mvY);
                
                if (length >= threshold) {
                    // 计算该向量所在的网格
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
            
            // 第二步：对每个网格进行角度归一化
            for (const auto& item : gridToVector) {
                auto key = item.first;
                MotionVector mv = item.second;
                
                float avgAngle = 0.0f;
                int count = 0;
                
                // 检查周围8格
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        if (dx == 0 && dy == 0) continue; // 跳过自己
                        
                        auto neighborKey = std::make_pair(key.first + dx, key.second + dy);
                        if (gridToAngle.count(neighborKey)) {
                            float angleDiff = std::abs(gridToAngle[key] - gridToAngle[neighborKey]);
                            // 处理角度环绕问题（-π 和 π 是同一个角度）
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
                
                // 如果有足够相似的邻居，进行归一
                if (count > 0) {
                    avgAngle += gridToAngle[key]; // 加入自己的角度
                    avgAngle /= (count + 1);
                    
                    // 根据平均角度重新计算运动向量
                    float mvX = static_cast<float>(mv.motion_x);
                    float mvY = static_cast<float>(mv.motion_y);
                    float length = std::sqrt(mvX*mvX + mvY*mvY);
                    
                    mv.motion_x = static_cast<int>(std::cos(avgAngle) * length);
                    mv.motion_y = static_cast<int>(std::sin(avgAngle) * length);
                }
                
                newCurrentMVs.push_back(mv);
            }
            
            // 只有当新向量不为空时，才更新 m_currentMVs
            if (!newCurrentMVs.empty()) {
                m_currentMVs = std::move(newCurrentMVs);
            }
            
            // 平滑插值：用 m_smoothedMVs 与 m_currentMVs 进行插值
            const float smoothFactor = 0.3f; // 平滑系数，0-1，越大越平滑
            std::vector<MotionVector> newSmoothedMVs;
            
            for (const auto& currentMV : m_currentMVs) {
                // 在 m_smoothedMVs 中找相同位置的向量
                bool found = false;
                for (const auto& smoothMV : m_smoothedMVs) {
                    if (smoothMV.dst_x == currentMV.dst_x && smoothMV.dst_y == currentMV.dst_y) {
                        // 找到！插值
                        MotionVector blended;
                        blended.dst_x = currentMV.dst_x;
                        blended.dst_y = currentMV.dst_y;
                        blended.w = currentMV.w;
                        blended.h = currentMV.h;
                        
                        // LERP 插值
                        blended.motion_x = static_cast<int>(
                            (1 - smoothFactor) * smoothMV.motion_x + smoothFactor * currentMV.motion_x
                        );
                        blended.motion_y = static_cast<int>(
                            (1 - smoothFactor) * smoothMV.motion_y + smoothFactor * currentMV.motion_y
                        );
                        
                        newSmoothedMVs.push_back(blended);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    // 没找到，直接用当前的
                    newSmoothedMVs.push_back(currentMV);
                }
            }
            
            m_smoothedMVs = std::move(newSmoothedMVs);
        }
    }
}

QColor VideoWidget::calculateMetaBlockAverageColor(int dst_x, int dst_y, int w, int h) {
    // 完整的安全检查
    if (m_pixFormat == VideoRenderData::NONE || 
        m_frameSize.isEmpty() ||
        !m_mvDataArr[0] || 
        m_mvLinesizeArr[0] <= 0) {
        return QColor(128, 128, 128, 128);
    }
    
    int frameW = m_frameSize.width();
    int frameH = m_frameSize.height();
    
    // 边界检查
    dst_x = std::max(0, dst_x);
    dst_y = std::max(0, dst_y);
    int endX = std::min(dst_x + w, frameW);
    int endY = std::min(dst_y + h, frameH);
    int blockW = endX - dst_x;
    int blockH = endY - dst_y;
    if (blockW <= 0 || blockH <= 0) {
        return QColor(128, 128, 128, 128);
    }
    
    long long sumY = 0, sumU = 0, sumV = 0;
    long long sumR = 0, sumG = 0, sumB = 0;
    int pixelCount = blockW * blockH;
    
    if (m_pixFormat == VideoRenderData::YUV || m_pixFormat == VideoRenderData::YUVA ||
        m_pixFormat == VideoRenderData::Y || m_pixFormat == VideoRenderData::YA) {
        // YUV 格式 - 额外检查 U 和 V 数据
        if (!m_mvDataArr[1] || !m_mvDataArr[2] || 
            m_mvLinesizeArr[1] <= 0 || m_mvLinesizeArr[2] <= 0 ||
            m_mvComponentSizeArr[1].isEmpty()) {
            return QColor(128, 128, 128, 128);
        }
        
        const uint8_t* yData = m_mvDataArr[0];
        int yStride = m_mvLinesizeArr[0];
        const uint8_t* uData = m_mvDataArr[1];
        int uStride = m_mvLinesizeArr[1];
        const uint8_t* vData = m_mvDataArr[2];
        int vStride = m_mvLinesizeArr[2];
        int uW = m_mvComponentSizeArr[1].width();
        int uH = m_mvComponentSizeArr[1].height();
        
        for (int y = dst_y; y < endY; ++y) {
            for (int x = dst_x; x < endX; ++x) {
                int yIdx = y * yStride + x;
                int uvX = x * uW / frameW;
                int uvY = y * uH / frameH;
                int uIdx = uvY * uStride + uvX;
                int vIdx = uvY * vStride + uvX;
                
                sumY += yData[yIdx];
                sumU += uData[uIdx];
                sumV += vData[vIdx];
            }
        }
        
        // YUV → RGB (BT.709)
        int avgY = static_cast<int>(sumY / pixelCount);
        int avgU = static_cast<int>(sumU / pixelCount);
        int avgV = static_cast<int>(sumV / pixelCount);
        
        int r = static_cast<int>(1.164 * (avgY - 16) + 1.793 * (avgV - 128));
        int g = static_cast<int>(1.164 * (avgY - 16) - 0.213 * (avgU - 128) - 0.533 * (avgV - 128));
        int b = static_cast<int>(1.164 * (avgY - 16) + 2.112 * (avgU - 128));
        
        r = std::clamp(r, 0, 255);
        g = std::clamp(g, 0, 255);
        b = std::clamp(b, 0, 255);
        
        return QColor(r, g, b, 180);
    } else if (m_pixFormat == VideoRenderData::RGB_PACKED || m_pixFormat == VideoRenderData::RGBA_PACKED) {
        // RGB packed
        const uint8_t* data = m_mvDataArr[0];
        int stride = m_mvLinesizeArr[0];
        int bytesPerPixel = (m_pixFormat == VideoRenderData::RGB_PACKED) ? 3 : 4;
        
        for (int y = dst_y; y < endY; ++y) {
            for (int x = dst_x; x < endX; ++x) {
                int idx = y * stride + x * bytesPerPixel;
                if (m_pixFormat == VideoRenderData::RGB_PACKED) {
                    sumR += data[idx];
                    sumG += data[idx + 1];
                    sumB += data[idx + 2];
                } else {
                    sumR += data[idx];
                    sumG += data[idx + 1];
                    sumB += data[idx + 2];
                }
            }
        }
        
        int r = static_cast<int>(sumR / pixelCount);
        int g = static_cast<int>(sumG / pixelCount);
        int b = static_cast<int>(sumB / pixelCount);
        return QColor(r, g, b, 180);
    } else {
        return QColor(128, 128, 128, 128);
    }
}

void VideoWidget::drawSimpleMotionVectors(QPainter& painter) {
    if (m_smoothedMVs.empty() || m_frameSize.isEmpty()) return;
    
    int widgetW = width();
    int widgetH = height();
    float fw = static_cast<float>(m_frameSize.width());
    float fh = static_cast<float>(m_frameSize.height());
    
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    
    // --- 先垂直翻转，再把坐标系原点移到屏幕中心 ---
    painter.translate(widgetW / 2.0, widgetH / 2.0);
    painter.scale(1.0, -1.0); // 垂直翻转
    
    // --- 第2步：计算视频基础缩放 (scaleX0, scaleY0) ---
    float scaleX0 = 1.0f, scaleY0 = 1.0f;
    if (m_frameSize.width() > m_frameSize.height()) {
        scaleY0 = 1.0f * m_frameSize.height() / m_frameSize.width();
    } else {
        scaleX0 = 1.0f * m_frameSize.width() / m_frameSize.height();
    }
    painter.scale(scaleX0, scaleY0); // 先应用基础缩放
    
    // --- 第3步：旋转 ---
    float ang = m_angle * ((m_horizontalMirror ^ m_verticalMirror) ? -1.f : 1.f);
    painter.rotate(ang);
    
    // --- 第4步：用户缩放 (带镜像) ---
    float sclX = m_scale / 100.0f * (m_horizontalMirror ? -1.0f : 1.0f);
    float sclY = m_scale / 100.0f * (m_verticalMirror ? -1.0f : 1.0f);
    painter.scale(sclX, sclY);
    
    // --- 第5步：保持视频比例的缩放 ---
    float scaleX = 1.0f, scaleY = 1.0f;
    float videoAspect = fw / fh;
    float widgetAspect = static_cast<float>(widgetW) / widgetH;
    if (videoAspect > widgetAspect) {
        scaleY = widgetAspect / videoAspect;
    } else {
        scaleX = videoAspect / widgetAspect;
    }
    painter.scale(scaleX / scaleX0, scaleY / scaleY0);
    
    // --- 第6步：用户平移 ---
    float tx = 2.0f * m_tx / widgetW;
    float ty = 2.0f * m_ty / widgetH;
    painter.translate(tx, ty);
    
    // --- 现在绘制每个运动向量 ---
    float halfW = widgetW / 2.0f;
    float halfH = widgetH / 2.0f;
    
    for (const auto& mv : m_smoothedMVs) {
        // 转换视频坐标到归一化设备坐标（矩形刚好大小，不扩展）
        int expand = 0; // 不扩展
        float nx = (2.0f * (mv.dst_x - expand) / fw) - 1.0f;
        float ny = 1.0f - (2.0f * (mv.dst_y - expand) / fh);
        
        // meta块的右下角
        float nx2 = (2.0f * (mv.dst_x + mv.w + expand) / fw) - 1.0f;
        float ny2 = 1.0f - (2.0f * (mv.dst_y + mv.h + expand) / fh);
        
        // 转到屏幕坐标
        float x0 = nx * halfW;
        float y0 = ny * halfH;
        float x2 = nx2 * halfW;
        float y2 = ny2 * halfH;
        
        // 计算箭头（尖端由运动矢量长度决定，底边中点在块中心）
        float centerX = (x0 + x2) / 2.0f;
        float centerY = (y0 + y2) / 2.0f;
        float mvX = static_cast<float>(mv.motion_x);
        float mvY = static_cast<float>(mv.motion_y);
        
        // 箭头尖端位置（根据运动矢量长度决定）
        float arrowTipX = centerX + (mvX / fw) * 2.0f * m_motionVectorScale * halfW;
        float arrowTipY = centerY - (mvY / fh) * 2.0f * m_motionVectorScale * halfH;
        
        // 计算meta块的平均颜色
        QColor blockColor = calculateMetaBlockAverageColor(mv.dst_x, mv.dst_y, mv.w, mv.h);
        
        // 画meta块（平均颜色半透明填充+边框）
        QRectF blockRect(QPointF(x0, y0), QPointF(x2, y2));
        painter.setBrush(QBrush(blockColor));
        painter.setPen(QPen(blockColor.darker(120), 1.0f));
        painter.drawRect(blockRect);
        
        // 全部显示黑色箭头！
        QColor blackArrowColor(Qt::black);
        blackArrowColor.setAlpha(200);
        
        // 画完整的箭头：箭杆 + V形箭头
        float dx = arrowTipX - centerX; // 从中心指向尖端的方向
        float dy = arrowTipY - centerY;
        float len = sqrt(dx * dx + dy * dy);
        if (len > 0.1f) {
            float dirX = dx / len; // 方向向量（从中心指向尖端）
            float dirY = dy / len;
            
            float arrowVLength = 8.0f; // V形箭头的长度
            float arrowVWidth = 4.0f; // V形箭头的宽度
            
            // 计算垂直于运动方向的向量
            float perpX = -dirY;
            float perpY = dirX;
            
            // V形箭头：在箭杆末端，开口朝向中心，尖端向外指向运动方向
            QPointF vTip(arrowTipX + dirX * arrowVLength, arrowTipY + dirY * arrowVLength); // V形尖端（向外）
            QPointF v1(arrowTipX + perpX * arrowVWidth, arrowTipY + perpY * arrowVWidth); // V形左端点
            QPointF v2(arrowTipX - perpX * arrowVWidth, arrowTipY - perpY * arrowVWidth); // V形右端点
            
            // 画箭杆：从中心画到箭杆末端
            painter.setPen(QPen(blackArrowColor, 2.0f));
            painter.drawLine(QPointF(centerX, centerY), QPointF(arrowTipX, arrowTipY));
            
            // 画V形箭头：两条线从尖端到左右端点
            painter.setPen(QPen(blackArrowColor, 2.0f));
            painter.drawLine(vTip, v1);
            painter.drawLine(vTip, v2);
        }
    }
    
    painter.restore();
}
