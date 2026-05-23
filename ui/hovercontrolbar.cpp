// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hovercontrolbar.h"
#include <QEnterEvent>

HoverControlBar::HoverControlBar(QWidget *parent)
    : QWidget(parent),
      m_duration(0),
      m_volume(100),
      m_isDraggingSlider(false),
      m_controlsVisible(false) {
    
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    
    setupUi();
    applyStyles();
    
    // 自动隐藏定时器
    m_autoHideTimer = new QTimer(this);
    m_autoHideTimer->setSingleShot(true);
    connect(m_autoHideTimer, &QTimer::timeout, this, &HoverControlBar::hideControls);
    
    // 显示动画（从底部滑入）
    m_showAnimation = new QPropertyAnimation(this, "geometry", this);
    m_showAnimation->setDuration(300);
    m_showAnimation->setEasingCurve(QEasingCurve::OutCubic);
    
    // 隐藏动画（滑出到底部）
    m_hideAnimation = new QPropertyAnimation(this, "geometry", this);
    m_hideAnimation->setDuration(300);
    m_hideAnimation->setEasingCurve(QEasingCurve::InCubic);
    connect(m_hideAnimation, &QPropertyAnimation::finished, [this]() {
        m_controlsVisible = false;
        QWidget::setVisible(false);
    });
    
    // 初始隐藏
    QWidget::setVisible(false);
}

HoverControlBar::~HoverControlBar() {
}

void HoverControlBar::setupUi() {
    // 主容器
    m_container = new QWidget(this);
    m_container->setObjectName("container");
    
    QVBoxLayout* mainLayout = new QVBoxLayout(m_container);
    mainLayout->setContentsMargins(16, 12, 16, 16);
    mainLayout->setSpacing(8);
    
    // 进度条区域
    QWidget* progressWidget = new QWidget(this);
    QHBoxLayout* progressLayout = new QHBoxLayout(progressWidget);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->setSpacing(12);
    
    m_timeLabel = new QLabel("0:00", this);
    m_timeLabel->setObjectName("timeLabel");
    
    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setObjectName("progressSlider");
    m_progressSlider->setMaximum(1000);
    connect(m_progressSlider, &QSlider::sliderPressed, this, &HoverControlBar::onProgressSliderPressed);
    connect(m_progressSlider, &QSlider::sliderReleased, this, &HoverControlBar::onProgressSliderReleased);
    connect(m_progressSlider, &QSlider::sliderMoved, this, &HoverControlBar::onProgressSliderMoved);
    
    m_durationLabel = new QLabel("0:00", this);
    m_durationLabel->setObjectName("timeLabel");
    
    progressLayout->addWidget(m_timeLabel);
    progressLayout->addWidget(m_progressSlider);
    progressLayout->addWidget(m_durationLabel);
    
    mainLayout->addWidget(progressWidget);
    
    // 控制按钮区域 - 三部分严格对称布局
    QWidget* controlsWidget = new QWidget(this);
    QHBoxLayout* controlsLayout = new QHBoxLayout(controlsWidget);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(0);
    
    // 左侧：与右侧等宽的占位
    QWidget* leftPlaceholder = new QWidget(this);
    leftPlaceholder->setFixedWidth(180); // 与右侧音量+全屏宽度一致
    controlsLayout->addWidget(leftPlaceholder);
    
    // 中间：播放按钮组，居中
    controlsLayout->addStretch(1);
    
    QWidget* centerWidget = new QWidget(this);
    QHBoxLayout* centerLayout = new QHBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(16);
    
    // 后退按钮
    m_rewindBtn = new QPushButton("<<", this);
    m_rewindBtn->setObjectName("controlButton");
    m_rewindBtn->setFixedSize(48, 48);
    connect(m_rewindBtn, &QPushButton::clicked, this, &HoverControlBar::fastRewind);
    
    // 播放/暂停按钮
    m_playPauseBtn = new QPushButton("Play", this);
    m_playPauseBtn->setObjectName("playButton");
    m_playPauseBtn->setFixedSize(56, 56);
    connect(m_playPauseBtn, &QPushButton::clicked, this, &HoverControlBar::togglePause);
    
    // 前进按钮
    m_ffwdBtn = new QPushButton(">>", this);
    m_ffwdBtn->setObjectName("controlButton");
    m_ffwdBtn->setFixedSize(48, 48);
    connect(m_ffwdBtn, &QPushButton::clicked, this, &HoverControlBar::fastForward);
    
    centerLayout->addWidget(m_rewindBtn);
    centerLayout->addWidget(m_playPauseBtn);
    centerLayout->addWidget(m_ffwdBtn);
    
    controlsLayout->addWidget(centerWidget);
    
    controlsLayout->addStretch(1);
    
    // 右侧：音量和全屏按钮
    QWidget* rightWidget = new QWidget(this);
    QHBoxLayout* rightLayout = new QHBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(12);
    
    m_volumeIconLabel = new QLabel("Vol", this);
    m_volumeIconLabel->setObjectName("volumeIcon");
    
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setObjectName("volumeSlider");
    m_volumeSlider->setMinimum(0);
    m_volumeSlider->setMaximum(100);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setFixedWidth(100);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &HoverControlBar::onVolumeSliderChanged);
    
    m_fullscreenBtn = new QPushButton("Full", this);
    m_fullscreenBtn->setObjectName("controlButton");
    m_fullscreenBtn->setFixedSize(40, 40);
    connect(m_fullscreenBtn, &QPushButton::clicked, this, &HoverControlBar::toggleFullscreen);
    
    rightLayout->addWidget(m_volumeIconLabel);
    rightLayout->addWidget(m_volumeSlider);
    rightLayout->addWidget(m_fullscreenBtn);
    
    controlsLayout->addWidget(rightWidget);
    
    mainLayout->addWidget(controlsWidget);
    
    // 设置容器布局
    QVBoxLayout* containerLayout = new QVBoxLayout(this);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->addWidget(m_container);
}

void HoverControlBar::applyStyles() {
    QString containerStyle = R"(
        #container {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 rgba(0, 0, 0, 0),
                stop:0.3 rgba(0, 0, 0, 0.6),
                stop:1 rgba(0, 0, 0, 0.85));
            border-radius: 12px;
        }
    )";
    m_container->setStyleSheet(containerStyle);
    
    QString buttonStyle = R"(
        QPushButton#controlButton {
            background: transparent;
            color: white;
            font-size: 18px;
            font-weight: bold;
            border: none;
            border-radius: 24px;
        }
        QPushButton#controlButton:hover {
            background: rgba(255, 255, 255, 0.15);
        }
        QPushButton#controlButton:pressed {
            background: rgba(255, 255, 255, 0.25);
        }
        
        QPushButton#playButton {
            background: #409eff;
            color: white;
            font-size: 16px;
            font-weight: bold;
            border: none;
            border-radius: 28px;
        }
        QPushButton#playButton:hover {
            background: #66b1ff;
        }
        QPushButton#playButton:pressed {
            background: #3a8ee6;
        }
    )";
    
    m_rewindBtn->setStyleSheet(buttonStyle);
    m_playPauseBtn->setStyleSheet(buttonStyle);
    m_ffwdBtn->setStyleSheet(buttonStyle);
    m_fullscreenBtn->setStyleSheet(buttonStyle);
    
    QString sliderStyle = R"(
        QSlider#progressSlider::groove:horizontal {
            height: 6px;
            background: rgba(255, 255, 255, 0.2);
            border-radius: 3px;
        }
        QSlider#progressSlider::handle:horizontal {
            width: 16px;
            height: 16px;
            background: #409eff;
            border-radius: 8px;
            margin: -5px 0;
        }
        QSlider#progressSlider::sub-page:horizontal {
            background: #409eff;
            border-radius: 3px;
        }
        
        QSlider#volumeSlider::groove:horizontal {
            height: 4px;
            background: rgba(255, 255, 255, 0.2);
            border-radius: 2px;
        }
        QSlider#volumeSlider::handle:horizontal {
            width: 12px;
            height: 12px;
            background: white;
            border-radius: 6px;
            margin: -4px 0;
        }
        QSlider#volumeSlider::sub-page:horizontal {
            background: white;
            border-radius: 2px;
        }
    )";
    
    m_progressSlider->setStyleSheet(sliderStyle);
    m_volumeSlider->setStyleSheet(sliderStyle);
    
    QString labelStyle = R"(
        QLabel#timeLabel {
            color: white;
            font-size: 14px;
            font-weight: 500;
        }
        QLabel#volumeIcon {
            color: white;
            font-size: 14px;
            font-weight: 600;
        }
    )";
    
    m_timeLabel->setStyleSheet(labelStyle);
    m_durationLabel->setStyleSheet(labelStyle);
    m_volumeIconLabel->setStyleSheet(labelStyle);
}

void HoverControlBar::showControls() {
    if (m_controlsVisible) {
        // 如果已经显示，重置自动隐藏定时器
        m_autoHideTimer->start(3000);
        return;
    }
    
    m_controlsVisible = true;
    QWidget::setVisible(true);
    
    // 停止正在进行的隐藏动画
    m_hideAnimation->stop();
    
    // 更新位置
    updatePosition();
    
    // 设置显示动画
    QRect hideRect = geometry();
    hideRect.moveTop(parentWidget()->height());
    
    QRect showRect = hideRect;
    showRect.moveTop(parentWidget()->height() - height());
    
    m_showAnimation->setStartValue(hideRect);
    m_showAnimation->setEndValue(showRect);
    m_showAnimation->start();
    
    // 启动自动隐藏定时器
    m_autoHideTimer->start(3000);
}

void HoverControlBar::hideControls() {
    if (!m_controlsVisible) return;
    
    // 停止显示动画
    m_showAnimation->stop();
    
    // 设置隐藏动画
    QRect showRect = geometry();
    QRect hideRect = showRect;
    hideRect.moveTop(parentWidget()->height());
    
    m_hideAnimation->setStartValue(showRect);
    m_hideAnimation->setEndValue(hideRect);
    m_hideAnimation->start();
}

void HoverControlBar::autoHide() {
    // 鼠标不在控制栏上时才自动隐藏
    if (!underMouse()) {
        hideControls();
    }
}

void HoverControlBar::updatePosition() {
    if (!parentWidget()) return;
    
    QRect parentRect = parentWidget()->rect();
    int barHeight = 130;
    
    setGeometry(0, parentRect.height() - barHeight, parentRect.width(), barHeight);
}

void HoverControlBar::setDuration(int seconds) {
    m_duration = seconds;
    m_durationLabel->setText(formatTime(seconds));
    m_progressSlider->setMaximum(seconds);
}

void HoverControlBar::setCurrentTime(int seconds) {
    if (!m_isDraggingSlider) {
        m_progressSlider->setValue(seconds);
    }
    m_timeLabel->setText(formatTime(seconds));
}

void HoverControlBar::setPaused(bool paused) {
    m_playPauseBtn->setText(paused ? "Play" : "Pause");
}

void HoverControlBar::setVolume(int volume) {
    m_volume = volume;
    m_volumeSlider->setValue(volume);
}

QString HoverControlBar::formatTime(int seconds) {
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    
    if (h > 0) {
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
    }
}

void HoverControlBar::onProgressSliderPressed() {
    m_isDraggingSlider = true;
    m_autoHideTimer->stop();
}

void HoverControlBar::onProgressSliderReleased() {
    m_isDraggingSlider = false;
    emit seekTo(m_progressSlider->value());
    m_autoHideTimer->start(3000);
}

void HoverControlBar::onProgressSliderMoved(int value) {
    m_timeLabel->setText(formatTime(value));
}

void HoverControlBar::onVolumeSliderChanged(int value) {
    m_volume = value;
    emit volumeChanged(value);
}

void HoverControlBar::enterEvent(QEnterEvent *event) {
    QWidget::enterEvent(event);
    // 鼠标进入控制栏，停止自动隐藏
    m_autoHideTimer->stop();
}

void HoverControlBar::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    // 鼠标离开控制栏，重新启动自动隐藏
    if (m_controlsVisible) {
        m_autoHideTimer->start(3000);
    }
}

void HoverControlBar::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // 只有在显示时才更新位置
    if (m_controlsVisible) {
        updatePosition();
    }
}
