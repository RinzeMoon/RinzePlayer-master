// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "controlbar.h"
#include <QStyle>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>

ControlBar::ControlBar(QWidget *parent)
    : QWidget(parent), m_isDraggingSlider(false), m_sidebarVisible(false), m_duration(0), m_volume(100) {
    setupUi();
    setupSidebar();
}

ControlBar::~ControlBar() {}

void ControlBar::setupUi() {
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 播放控制区
    m_openBtn = new QPushButton("打开", this);
    m_openBtn->setMinimumWidth(100);
    connect(m_openBtn, &QPushButton::clicked, this, &ControlBar::openFile);

    // URL输入区
    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText("输入URL (HLS/RTMP/RTSP)...");
    m_urlEdit->setMinimumWidth(250);
    connect(m_urlEdit, &QLineEdit::returnPressed, this, &ControlBar::onUrlReturnPressed);

    m_openUrlBtn = new QPushButton("播放URL", this);
    m_openUrlBtn->setMinimumWidth(80);
    connect(m_openUrlBtn, &QPushButton::clicked, this, &ControlBar::onOpenUrlClicked);

    // 全屏按钮
    QPushButton *fullscreenBtn = new QPushButton("全屏", this);
    fullscreenBtn->setMinimumWidth(60);
    connect(fullscreenBtn, &QPushButton::clicked, this, &ControlBar::toggleFullscreen);

    // 视频信息标签（隐藏播放控制后显示）
    m_pixelFormatLabel = new QLabel("Pixel: -", this);
    m_colorRangeLabel = new QLabel("Range: -", this);
    m_colorSpaceLabel = new QLabel("Space: -", this);

    // 侧边栏切换按钮
    m_sidebarToggleBtn = new QPushButton("更多", this);
    m_sidebarToggleBtn->setMinimumWidth(80);
    connect(m_sidebarToggleBtn, &QPushButton::clicked, this, &ControlBar::onToggleSidebar);

    // 布局
    layout->addWidget(m_openBtn);
    layout->addWidget(m_urlEdit);
    layout->addWidget(m_openUrlBtn);
    layout->addWidget(fullscreenBtn);

    m_chatBtn = new QPushButton("💬", this);
    m_chatBtn->setMinimumWidth(36);
    m_chatBtn->setToolTip("聊天 (反引号键)`");
    connect(m_chatBtn, &QPushButton::clicked, this, &ControlBar::chatToggle);

    layout->addWidget(m_chatBtn);
    layout->addStretch(1);
    layout->addWidget(m_pixelFormatLabel);
    layout->addWidget(m_colorRangeLabel);
    layout->addWidget(m_colorSpaceLabel);
    layout->addWidget(m_sidebarToggleBtn);
}

void ControlBar::setupSidebar() {
    // 创建自定义侧边栏 - 支持点击外部关闭
    m_sidebar = new SidebarWidget(this);
    m_sidebar->setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::Tool);
    m_sidebar->setAttribute(Qt::WA_ShowWithoutActivating);
    m_sidebar->setAttribute(Qt::WA_QuitOnClose, false);
    m_sidebar->hide();
    // 连接侧边栏关闭信号
    connect(m_sidebar, &SidebarWidget::closed, this, [this]() {
        m_sidebarVisible = false;
        m_sidebarToggleBtn->setText("更多");
    });

    QVBoxLayout *sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(12, 12, 12, 12);
    sidebarLayout->setSpacing(10);

    // 音量控制
    QLabel* volumeTitleLabel = new QLabel("音量", this);
    volumeTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    sidebarLayout->addWidget(volumeTitleLabel);

    QHBoxLayout* volumeLayout = new QHBoxLayout();
    m_volumeLabel = new QLabel("100%", this);
    m_volumeLabel->setMinimumWidth(60);
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setMinimum(0);
    m_volumeSlider->setMaximum(100);
    m_volumeSlider->setValue(100);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &ControlBar::onVolumeSliderChanged);
    volumeLayout->addWidget(m_volumeLabel);
    volumeLayout->addWidget(m_volumeSlider, 1);
    sidebarLayout->addLayout(volumeLayout);

    // 分隔线
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    sidebarLayout->addWidget(line);

    // 视频过滤器
    QLabel* filterTitleLabel = new QLabel("视频过滤器", this);
    filterTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    sidebarLayout->addWidget(filterTitleLabel);

    // Brightness
    QLabel* brightLabel = new QLabel("亮度", this);
    sidebarLayout->addWidget(brightLabel);
    QSlider* brightSlider = new QSlider(Qt::Horizontal, this);
    brightSlider->setRange(-100, 100);
    brightSlider->setValue(0);
    connect(brightSlider, &QSlider::valueChanged, this, [this](int val) {
        emit brightnessChanged(val / 100.0f);
    });
    sidebarLayout->addWidget(brightSlider);

    // Contrast
    QLabel* contrastLabel = new QLabel("对比度", this);
    sidebarLayout->addWidget(contrastLabel);
    QSlider* contrastSlider = new QSlider(Qt::Horizontal, this);
    contrastSlider->setRange(50, 200);
    contrastSlider->setValue(100);
    connect(contrastSlider, &QSlider::valueChanged, this, [this](int val) {
        emit contrastChanged(val / 100.0f);
    });
    sidebarLayout->addWidget(contrastSlider);

    // Saturation
    QLabel* satLabel = new QLabel("饱和度", this);
    sidebarLayout->addWidget(satLabel);
    QSlider* satSlider = new QSlider(Qt::Horizontal, this);
    satSlider->setRange(0, 200);
    satSlider->setValue(100);
    connect(satSlider, &QSlider::valueChanged, this, [this](int val) {
        emit saturationChanged(val / 100.0f);
    });
    sidebarLayout->addWidget(satSlider);

    // Reset filters button
    QPushButton* resetFiltersBtn = new QPushButton("重置过滤器", this);
    resetFiltersBtn->setStyleSheet("background-color: #ff9800; color: white; font-weight: bold;");
    connect(resetFiltersBtn, &QPushButton::clicked, this, &ControlBar::resetFilters);
    sidebarLayout->addWidget(resetFiltersBtn);

    // 分隔线
    QFrame* line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    sidebarLayout->addWidget(line2);

    // 画面控制
    QLabel* videoTitleLabel = new QLabel("画面调节", this);
    videoTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    sidebarLayout->addWidget(videoTitleLabel);

    m_rotateLeftBtn = new QPushButton("左转90度", this);
    connect(m_rotateLeftBtn, &QPushButton::clicked, this, &ControlBar::rotateLeft);
    sidebarLayout->addWidget(m_rotateLeftBtn);

    m_rotateRightBtn = new QPushButton("右转90度", this);
    connect(m_rotateRightBtn, &QPushButton::clicked, this, &ControlBar::rotateRight);
    sidebarLayout->addWidget(m_rotateRightBtn);

    m_hMirrorBtn = new QPushButton("水平镜像", this);
    connect(m_hMirrorBtn, &QPushButton::clicked, this, &ControlBar::horizontalMirror);
    sidebarLayout->addWidget(m_hMirrorBtn);

    m_vMirrorBtn = new QPushButton("垂直镜像", this);
    connect(m_vMirrorBtn, &QPushButton::clicked, this, &ControlBar::verticalMirror);
    sidebarLayout->addWidget(m_vMirrorBtn);

    m_resetBtn = new QPushButton("重置画面", this);
    m_resetBtn->setStyleSheet("background-color: #ff6b6b; color: white; font-weight: bold;");
    connect(m_resetBtn, &QPushButton::clicked, this, &ControlBar::resetTransform);
    sidebarLayout->addWidget(m_resetBtn);

    // 分隔线
    QFrame* line3 = new QFrame(this);
    line3->setFrameShape(QFrame::HLine);
    line3->setFrameShadow(QFrame::Sunken);
    sidebarLayout->addWidget(line3);

    // 显示设置
    QLabel* displayTitleLabel = new QLabel("显示设置", this);
    displayTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    sidebarLayout->addWidget(displayTitleLabel);

    QCheckBox* fpsCheckBox = new QCheckBox("显示帧率", this);
    fpsCheckBox->setChecked(false);
    connect(fpsCheckBox, &QCheckBox::toggled, this, &ControlBar::showFPSChanged);
    sidebarLayout->addWidget(fpsCheckBox);

    // 色彩空间选择
    QLabel* colorSpaceLabel = new QLabel("色彩空间:", this);
    sidebarLayout->addWidget(colorSpaceLabel);
    QComboBox* colorSpaceCombo = new QComboBox(this);
    colorSpaceCombo->addItem("BT.601 (标清)", 0);
    colorSpaceCombo->addItem("BT.709 (高清)", 1);
    colorSpaceCombo->addItem("BT.2020 (超高清/4K)", 2);
    colorSpaceCombo->addItem("Dolby Vision (杜比视界)", 3); // 新增 Dolby Vision 选项
    colorSpaceCombo->setCurrentIndex(1); // 默认BT.709
    connect(colorSpaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
        int colorSpace = colorSpaceCombo->itemData(index).toInt();
        emit colorSpaceChanged(colorSpace);
    });
    sidebarLayout->addWidget(colorSpaceCombo);

    // 分隔线
    QFrame* line4 = new QFrame(this);
    line4->setFrameShape(QFrame::HLine);
    line4->setFrameShadow(QFrame::Sunken);
    sidebarLayout->addWidget(line4);

    // 低延迟模式
    QLabel* lowLatencyLabel = new QLabel("播放设置", this);
    lowLatencyLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    sidebarLayout->addWidget(lowLatencyLabel);
    
    QCheckBox* lowLatencyCheckBox = new QCheckBox("低延迟模式", this);
    lowLatencyCheckBox->setChecked(false);
    connect(lowLatencyCheckBox, &QCheckBox::toggled, this, &ControlBar::lowLatencyModeChanged);
    sidebarLayout->addWidget(lowLatencyCheckBox);

    // 分隔线
    QFrame* line5 = new QFrame(this);
    line5->setFrameShape(QFrame::HLine);
    line5->setFrameShadow(QFrame::Sunken);
    sidebarLayout->addWidget(line5);

    // 视频信息
    QLabel* videoInfoTitleLabel = new QLabel("视频信息", this);
    videoInfoTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    sidebarLayout->addWidget(videoInfoTitleLabel);
    
    m_pixelFormatLabel = new QLabel("像素格式: --", this);
    m_colorRangeLabel = new QLabel("色彩范围: --", this);
    m_colorSpaceLabel = new QLabel("色彩空间: --", this);
    sidebarLayout->addWidget(m_pixelFormatLabel);
    sidebarLayout->addWidget(m_colorRangeLabel);
    sidebarLayout->addWidget(m_colorSpaceLabel);

    sidebarLayout->addStretch();
}

QString ControlBar::formatTime(int seconds) {
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

void ControlBar::setDuration(int seconds) {
    m_duration = seconds;
    m_durationLabel->setText(formatTime(seconds));
    m_progressSlider->setMaximum(seconds);
}

void ControlBar::setCurrentTime(int seconds) {
    if (!m_isDraggingSlider) {
        m_timeLabel->setText(formatTime(seconds));
        m_progressSlider->setValue(seconds);
    }
}

void ControlBar::setPaused(bool paused) {
    m_playPauseBtn->setText(paused ? "播放" : "暂停");
}

void ControlBar::setVolume(int volume) {
    m_volume = volume;
    m_volumeSlider->setValue(volume);
    m_volumeLabel->setText(QString("%1%").arg(volume));
}

void ControlBar::updateVideoInfo(const QString& pixelFormat, const QString& colorRange, const QString& colorSpace) {
    m_pixelFormatLabel->setText("像素格式: " + pixelFormat);
    m_colorRangeLabel->setText("色彩范围: " + colorRange);
    m_colorSpaceLabel->setText("色彩空间: " + colorSpace);
}

void ControlBar::onProgressSliderPressed() {
    m_isDraggingSlider = true;
}

void ControlBar::onProgressSliderReleased() {
    m_isDraggingSlider = false;
    int val = std::max(0, std::min(m_progressSlider->value(), m_duration));
    emit seekTo(val);
}

void ControlBar::onProgressSliderMoved(int value) {
    m_timeLabel->setText(formatTime(value));
}

void ControlBar::onVolumeSliderChanged(int value) {
    m_volume = value;
    m_volumeLabel->setText(QString("%1%").arg(value));
    emit volumeChanged(value);
}

void ControlBar::onOpenUrlClicked() {
    QString urlText = m_urlEdit->text().trimmed();
    if (urlText.isEmpty()) {
        return;
    }
    
    // 显示配置对话框
    UrlPlaybackConfigDialog* dialog = new UrlPlaybackConfigDialog(this);
    dialog->setUrl(urlText);
    
    connect(dialog, &UrlPlaybackConfigDialog::playRequested, this, [this, dialog](const QUrl& url) {
        // 获取配置并发送
        bool lowLatency = dialog->lowLatencyEnabled();
        bool noBuffer = dialog->noBufferEnabled();
        bool lowDelayFlag = dialog->lowDelayFlagEnabled();
        bool discardCorrupt = dialog->discardCorruptEnabled();
        bool noParse = dialog->noParseEnabled();
        bool shortProbe = dialog->shortProbeEnabled();
        
        emit openUrlWithConfig(url, lowLatency, noBuffer, lowDelayFlag, discardCorrupt, noParse, shortProbe);
        dialog->deleteLater();
    });
    
    dialog->exec();
}

void ControlBar::onUrlReturnPressed() {
    onOpenUrlClicked();
}

void ControlBar::onToggleSidebar() {
    if (m_sidebarVisible) {
        m_sidebar->hide();
        m_sidebarVisible = false;
        m_sidebarToggleBtn->setText("更多");
    } else {
        // 定位在主窗口的右下角
        if (this->window()) {
            QRect mainRect = this->window()->geometry();
            int sidebarWidth = 300;
            int sidebarHeight = 600;
            int x = mainRect.right() - sidebarWidth - 20;
            int y = mainRect.top() + 20;
            m_sidebar->setGeometry(x, y, sidebarWidth, sidebarHeight);
        }
        m_sidebar->show();
        m_sidebar->raise();
        m_sidebar->activateWindow();
        m_sidebarVisible = true;
        m_sidebarToggleBtn->setText("关闭");
    }
}

void ControlBar::setRoomMode(bool inRoom) {
    m_inRoom = inRoom;
    m_openBtn->setEnabled(!inRoom);
    m_urlEdit->setEnabled(!inRoom);
    m_openUrlBtn->setEnabled(!inRoom);
}