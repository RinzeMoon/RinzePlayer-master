// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pipwindow.h"
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QShowEvent>
#include <QHideEvent>

PiPWindow::PiPWindow(QWidget *parent)
    : QWidget(parent), m_isDragging(false) {
    // 设置窗口属性
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    // 优化性能设置
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setFixedSize(320, 180);

    // 创建视频组件
    m_videoWidget = new VideoWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_videoWidget);
    setLayout(layout);

    // 给画中画视频组件禁用FPS和其他显示，减少开销
    m_videoWidget->setShowFPS(false);
    m_videoWidget->setMiniMode(true);

    // 设置样式
    setStyleSheet("QWidget { border: 2px solid white; border-radius: 5px; }");
}

PiPWindow::~PiPWindow() {
}

void PiPWindow::setVideoData(VideoDoubleBuf *vidData, SubtitleDoubleBuf *subData) {
    if (m_videoWidget && isVisible()) {  // 只在可见时更新
        m_videoWidget->updateRenderData(vidData, subData);
    }
}

void PiPWindow::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (m_videoWidget) {
        m_videoWidget->show();
    }
}

void PiPWindow::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    if (m_videoWidget) {
        m_videoWidget->hide();  // 隐藏时也隐藏视频组件，减少GPU负载
    }
}

void PiPWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragStartPos = event->globalPosition().toPoint();
        m_windowStartPos = pos();
    }
}

void PiPWindow::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDragging) {
        QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;
        move(m_windowStartPos + delta);
    }
}

void PiPWindow::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    m_isDragging = false;
}

void PiPWindow::closeEvent(QCloseEvent *event) {
    Q_UNUSED(event);
    // 关闭时也隐藏视频组件
    if (m_videoWidget) {
        m_videoWidget->hide();
    }
}
