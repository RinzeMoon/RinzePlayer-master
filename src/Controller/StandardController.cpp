#include "../Header/Controller/StandardController.h"
#include <QStyle>
#include <QTime>
#include <QDebug>

StandardController::StandardController(QWidget *parent)
    : BaseController(parent)
    , m_currentMode(VideoPlayerMode::LOCAL_FILE)
{
    setupUi();
}

void StandardController::setupUi()
{
    // 全局样式 - B站原版纯黑磨砂底，无任何多余样式
    this->setStyleSheet(R"(
        QWidget {
            background: rgba(24, 24, 24, 255);
            border-radius: 8px;
            padding: 0px;
        }
    )");
    this->setFixedHeight(62); // B站播放器标准高度

    // 顶层主布局
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(20, 0, 20, 0);
    mainLayout->setSpacing(18);

    // 创建双容器 只创建空容器，内容由各自init函数初始化
    m_localHttpContainer = new QWidget(this);
    m_rtmpContainer = new QWidget(this);

    // 调用模块化初始化函数
    initLocalHttpUi();
    initRtmpUi();

    // 添加容器到布局
    mainLayout->addWidget(m_localHttpContainer);
    mainLayout->addWidget(m_rtmpContainer);

    // 默认显示本地播放UI，隐藏直播UI
    m_localHttpContainer->show();
    m_rtmpContainer->hide();
}

// ✅ 修复核心崩溃函数 - setMode 无任何野指针操作，逻辑极简，绝对不崩
void StandardController::setMode(VideoPlayerMode mode)
{
    qDebug() << "[StandardController::setMode] 切换模式" << mode;
    m_currentMode = mode;
    switch (mode)
    {
        // 本地/HTTP模式 → 显示本地UI，隐藏直播UI + 启用本地控件
    case VideoPlayerMode::LOCAL_FILE:
    case VideoPlayerMode::HTTP_STREAM:
        m_localHttpContainer->show();
        m_rtmpContainer->hide();
        m_prevBtn->setEnabled(true);
        m_nextBtn->setEnabled(true);
        m_progressSlider->setEnabled(true);
        m_speedCombo->setEnabled(true);

        m_isPlaying = false;
        break;
        qDebug() << "目前为 HLS/LOCAL 的ui";

        // RTMP直播模式 → 纯观众视角，无开播按钮，永不崩溃
    case VideoPlayerMode::RTMP_STREAM:
        m_localHttpContainer->hide();
        m_rtmpContainer->show();
        // 禁用本地控件，防止误触
        m_prevBtn->setEnabled(false);
        m_nextBtn->setEnabled(false);
        m_progressSlider->setEnabled(false);
        m_speedCombo->setEnabled(false);
        // 重置直播状态
        m_liveStatusLabel->setText("直播就绪 · 超清画质 · 低延迟");
        m_liveStatusLabel->setStyleSheet(R"(
            QLabel{
                color:#FFFFFF;
                font-size:12px;
                font-weight:500;
                padding:3px 14px;
                background-color: #282828;
                border:1px solid #444444;
                border-radius:16px;
            }
        )");
        m_isPlaying = false;
        qDebug() << "目前为 RTMP 的ui";
        break;
    }
}

void StandardController::updateState(bool playing, qint64 position, qint64 duration)
{
    m_isPlaying = playing;
    m_duration = duration;
    m_currentPosition = position;

    // 只更新本地播放按钮，直播按钮在自己的槽函数里更新
    if(m_currentMode != VideoPlayerMode::RTMP_STREAM)
    {
        m_playPauseBtn->setText(playing ? "⏸" : "▶");
    }

    // 更新时间显示
    m_timeLabel->setText(formatTime(position));
    m_durationLabel->setText(formatTime(duration));

    // 更新进度条（避免循环信号）
    if (!m_isSliderPressed && m_duration > 0 && m_currentMode != VideoPlayerMode::RTMP_STREAM)
    {
        int value = (position * 1000) / duration;
        m_progressSlider->setValue(value);
    }
}

QString StandardController::formatTime(qint64 ms) const
{
    int totalSeconds = ms / 1000;
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    return QString("%1:%2").arg(minutes, 2, 10, QChar('0'))
                          .arg(seconds, 2, 10, QChar('0'));
}

void StandardController::onPlayPauseClicked()
{
    if (m_isPlaying)
    {
        emit pauseClicked();
    }
    else
    {
        emit playClicked();
    }
}

void StandardController::onStopClicked()
{
    emit stopClicked();
}

void StandardController::onVolumeChanged(int value)
{
    emit volumeChanged(value);
}

void StandardController::onSpeedChanged(int index)
{
    float speeds[] = {0.5f, 1.0f, 1.5f, 2.0f};
    float speed = speeds[index];
    emit speedChanged(speed);
}

void StandardController::onProgressSliderReleased()
{
    m_isSliderPressed = false;
    qint64 pos = m_progressSlider->value() * m_duration / 1000;
    emit seekRequested(pos);
}

void StandardController::onProgressSliderPressed()
{
    m_isSliderPressed = true;
}

// ✅ 本地/HTTP播放UI - B站风格，无Emoji，预留图标位，原有逻辑完全保留
void StandardController::initLocalHttpUi()
{
    QHBoxLayout *localHttpLayout = new QHBoxLayout(m_localHttpContainer);
    localHttpLayout->setContentsMargins(0, 0, 0, 0);
    localHttpLayout->setSpacing(18);

    QHBoxLayout *leftLayout = new QHBoxLayout();
    leftLayout->setSpacing(8);

    m_prevBtn = new QPushButton(this);
    m_prevBtn->setFixedSize(36, 36);
    m_prevBtn->setCursor(Qt::PointingHandCursor);
    m_prevBtn->setStyleSheet(R"(
        QPushButton { border-radius: 18px;background-color: transparent; }
        QPushButton:hover { background-color: rgba(255,255,255,0.1); }
        QPushButton:disabled { opacity: 0.3; }
    )");
    // ★图标替换：取消注释+粘贴路径
    // m_prevBtn->setIcon(QIcon(":/icons/player/prev.png"));
    // m_prevBtn->setIconSize(QSize(20, 20));

    m_playPauseBtn = new QPushButton(this);
    m_playPauseBtn->setFixedSize(44, 44);
    m_playPauseBtn->setCursor(Qt::PointingHandCursor);
    m_playPauseBtn->setStyleSheet(R"(
        QPushButton { border-radius: 22px;background-color: #FFFFFF; }
        QPushButton:hover { opacity: 0.9; }
        QPushButton:disabled { background-color: #666666;opacity: 0.5; }
    )");
    // ★图标替换：取消注释+粘贴路径
    m_playPauseBtn->setIcon(QIcon("../images/icons/play.png"));
    m_playPauseBtn->setIconSize(QSize(22, 22));

    m_nextBtn = new QPushButton(this);
    m_nextBtn->setFixedSize(36, 36);
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    m_nextBtn->setStyleSheet(m_prevBtn->styleSheet());
    // 图标替换：取消注释+粘贴路径
    m_nextBtn->setIcon(QIcon("../images/icons/next.png"));
    m_nextBtn->setIconSize(QSize(20, 20));

    leftLayout->addWidget(m_prevBtn);
    leftLayout->addWidget(m_playPauseBtn);
    leftLayout->addWidget(m_nextBtn);

    QVBoxLayout *centerLayout = new QVBoxLayout();
    centerLayout->setSpacing(4);
    centerLayout->setContentsMargins(0, 0, 0, 0);

    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setRange(0, 1000);
    m_progressSlider->setCursor(Qt::PointingHandCursor);
    m_progressSlider->setStyleSheet(R"(
        QSlider::groove:horizontal { height:2px;background:#333;border-radius:1px; }
        QSlider::sub-page:horizontal { height:2px;background:#FFFFFF;border-radius:1px; }
        QSlider::handle:horizontal { width:8px;height:8px;margin:-3px 0;background:#FFFFFF;border-radius:4px; }
        QSlider::handle:horizontal:hover { width:10px;height:10px;margin:-4px 0; }
        QSlider:disabled { QSlider::groove{background:#222};QSlider::sub-page{background:#666};QSlider::handle{background:#666}; }
    )");

    QHBoxLayout *timeLayout = new QHBoxLayout();
    timeLayout->setContentsMargins(0,0,0,0);
    m_timeLabel = new QLabel("00:00", this);
    m_timeLabel->setStyleSheet("color: #888; font-size: 11px;");
    m_timeLabel->setFixedWidth(40);
    m_durationLabel = new QLabel("00:00", this);
    m_durationLabel->setStyleSheet("color: #888; font-size: 11px;");
    m_durationLabel->setFixedWidth(40);
    m_durationLabel->setAlignment(Qt::AlignRight);
    timeLayout->addWidget(m_timeLabel);
    timeLayout->addStretch();
    timeLayout->addWidget(m_durationLabel);

    centerLayout->addWidget(m_progressSlider);
    centerLayout->addLayout(timeLayout);

    QHBoxLayout *rightLayout = new QHBoxLayout();
    rightLayout->setSpacing(12);

    m_speedCombo = new QComboBox(this);
    m_speedCombo->addItems({"0.5x", "1.0x", "1.5x", "2.0x"});
    m_speedCombo->setCurrentIndex(1);
    m_speedCombo->setFixedWidth(58);
    m_speedCombo->setCursor(Qt::PointingHandCursor);
    m_speedCombo->setStyleSheet(R"(
        QComboBox { height:28px;background:#282828;border:none;border-radius:4px;color:#CCC;font-size:12px;padding-left:6px; }
        QComboBox:hover { color:#FFF; }
        QComboBox::drop-down { border:none;width:20px; }
        QComboBox::down-arrow { image: none; }
        QComboBox QAbstractItemView { background:#181818;border:1px solid #333;color:#CCC;selection-background-color:#333; }
        QComboBox:disabled { background:#222;color:#666;opacity:0.6; }
    )");

    m_volumeBtn = new QToolButton(this);
    m_volumeBtn->setFixedSize(32, 32);
    m_volumeBtn->setCursor(Qt::PointingHandCursor);
    m_volumeBtn->setStyleSheet(R"(
        QToolButton { background:transparent; }
        QToolButton:hover { background:rgba(255,255,255,0.1);border-radius:4px; }
    )");
    // ★图标替换：取消注释+粘贴路径
    m_volumeBtn->setIcon(QIcon("../images/icons/volume.png"));
    m_volumeBtn->setIconSize(QSize(18, 18));

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedWidth(80);
    m_volumeSlider->setCursor(Qt::PointingHandCursor);
    m_volumeSlider->setStyleSheet(R"(
        QSlider::groove:horizontal { height:2px;background:#333;border-radius:1px; }
        QSlider::sub-page:horizontal { height:2px;background:#FFFFFF;border-radius:1px; }
        QSlider::handle:horizontal { width:6px;height:6px;margin:-2px 0;background:#FFFFFF;border-radius:3px; }
    )");

    m_settingsBtn = new QToolButton(this);
    m_settingsBtn->setFixedSize(32, 32);
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setStyleSheet(m_volumeBtn->styleSheet());
    // ★图标替换：取消注释+粘贴路径
    // m_settingsBtn->setIcon(QIcon(":/icons/player/settings.png"));
    // m_settingsBtn->setIconSize(QSize(18, 18));

    m_fullscreenBtn = new QPushButton(this);
    m_fullscreenBtn->setFixedSize(32, 32);
    m_fullscreenBtn->setCursor(Qt::PointingHandCursor);
    m_fullscreenBtn->setStyleSheet(m_volumeBtn->styleSheet());
    // ★图标替换：取消注释+粘贴路径
    // m_fullscreenBtn->setIcon(QIcon(":/icons/player/fullscreen.png"));
    // m_fullscreenBtn->setIconSize(QSize(18, 18));

    rightLayout->addWidget(m_speedCombo);
    rightLayout->addWidget(m_volumeBtn);
    rightLayout->addWidget(m_volumeSlider);
    rightLayout->addWidget(m_settingsBtn);
    rightLayout->addWidget(m_fullscreenBtn);

    localHttpLayout->addLayout(leftLayout);
    localHttpLayout->addLayout(centerLayout, 1);
    localHttpLayout->addLayout(rightLayout);

    // 信号槽连接
    connect(m_playPauseBtn, &QPushButton::clicked, this, &StandardController::onPlayPauseClicked);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &StandardController::onVolumeChanged);
    connect(m_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StandardController::onSpeedChanged);
    connect(m_progressSlider, &QSlider::sliderReleased, this, &StandardController::onProgressSliderReleased);
    connect(m_progressSlider, &QSlider::sliderPressed, this, &StandardController::onProgressSliderPressed);
    connect(m_fullscreenBtn, &QPushButton::clicked, this, &StandardController::fullscreenToggled);
    connect(m_prevBtn, &QPushButton::clicked, this, [](){});
    connect(m_nextBtn, &QPushButton::clicked, this, [](){});
}

// ✅ 核心修复：RTMP直播UI【纯观众视角】 无开播按钮 + 成员变量初始化 永不崩溃
void StandardController::initRtmpUi()
{
    QHBoxLayout *rtmpLayout = new QHBoxLayout(m_rtmpContainer);
    rtmpLayout->setContentsMargins(0, 0, 0, 0);
    rtmpLayout->setSpacing(18);

    // 左侧：仅保留直播播放暂停按钮
    QHBoxLayout *rtmpLeftLayout = new QHBoxLayout();
    rtmpLeftLayout->setSpacing(8);
    m_rtmpPlayBtn = new QPushButton(this);
    m_rtmpPlayBtn->setFixedSize(44, 44);
    m_rtmpPlayBtn->setCursor(Qt::PointingHandCursor);
    m_rtmpPlayBtn->setStyleSheet(R"(
        QPushButton { border-radius: 22px;background-color: #FFFFFF; }
        QPushButton:hover { opacity: 0.9; }
    )");
    // ★图标替换：和本地播放按钮同路径
    m_rtmpPlayBtn->setIcon(QIcon(QString("../images/icons/play.png")));
    m_rtmpPlayBtn->setIconSize(QSize(22, 22));
    rtmpLeftLayout->addWidget(m_rtmpPlayBtn);

    // 中间：直播状态标签 居中显示
    QHBoxLayout *rtmpCenterLayout = new QHBoxLayout();
    rtmpCenterLayout->addStretch();
    m_liveStatusLabel = new QLabel(this);
    m_liveStatusLabel->setText("已就绪");
    m_liveStatusLabel->setStyleSheet(R"(
        QLabel{ color:#FFFFFF;font-size:12px;font-weight:500;padding:3px 14px;background:#282828;border:1px solid #444;border-radius:16px; }
    )");
    rtmpCenterLayout->addWidget(m_liveStatusLabel);
    rtmpCenterLayout->addStretch();

    // 右侧：直播功能区 禁用倍速+保留核心功能
    QHBoxLayout *rtmpRightLayout = new QHBoxLayout();
    rtmpRightLayout->setSpacing(12);

    m_rtmpSpeedCombo = new QComboBox(this);
    m_rtmpSpeedCombo->addItem("1.0x");
    m_rtmpSpeedCombo->setEnabled(false);
    m_rtmpSpeedCombo->setFixedWidth(58);
    m_rtmpSpeedCombo->setStyleSheet(R"(
        QComboBox { height:28px;background:#222;border:none;border-radius:4px;color:#666;font-size:12px;padding-left:6px;opacity:0.6; }
    )");

    m_rtmpVolumeBtn = new QToolButton(this);
    m_rtmpVolumeBtn->setFixedSize(32, 32);
    m_rtmpVolumeBtn->setCursor(Qt::PointingHandCursor);
    m_rtmpVolumeBtn->setStyleSheet(R"(
        QToolButton { background:transparent; }
        QToolButton:hover { background:rgba(255,255,255,0.1);border-radius:4px; }
    )");
    // ★图标复用：和本地音量按钮同路径
    m_rtmpVolumeBtn->setIcon(QIcon("../images/icons/volume.png"));
    m_rtmpVolumeBtn->setIconSize(QSize(18, 18));

    m_rtmpVolumeSlider = new QSlider(Qt::Horizontal, this);
    m_rtmpVolumeSlider->setRange(0, 100);
    m_rtmpVolumeSlider->setValue(80);
    m_rtmpVolumeSlider->setFixedWidth(80);
    m_rtmpVolumeSlider->setStyleSheet(m_volumeSlider->styleSheet());

    m_rtmpSettingsBtn = new QToolButton(this);
    m_rtmpSettingsBtn->setFixedSize(32, 32);
    m_rtmpSettingsBtn->setCursor(Qt::PointingHandCursor);
    m_rtmpSettingsBtn->setStyleSheet(m_rtmpVolumeBtn->styleSheet());
    // ★图标复用：和本地设置按钮同路径
    m_rtmpSettingsBtn->setIcon(QIcon("../images/icons/settings.png"));
    // m_rtmpSettingsBtn->setIconSize(QSize(18, 18));

    m_rtmpFullscreenBtn = new QPushButton(this);
    m_rtmpFullscreenBtn->setFixedSize(32, 32);
    m_rtmpFullscreenBtn->setCursor(Qt::PointingHandCursor);
    m_rtmpFullscreenBtn->setStyleSheet(m_rtmpVolumeBtn->styleSheet());
    // ★图标复用：和本地全屏按钮同路径
    m_rtmpFullscreenBtn->setIcon(QIcon("../images/icons/fullscreen.png"));
    // m_rtmpFullscreenBtn->setIconSize(QSize(18, 18));

    rtmpRightLayout->addWidget(m_rtmpSpeedCombo);
    rtmpRightLayout->addWidget(m_rtmpVolumeBtn);
    rtmpRightLayout->addWidget(m_rtmpVolumeSlider);
    rtmpRightLayout->addWidget(m_rtmpSettingsBtn);
    rtmpRightLayout->addWidget(m_rtmpFullscreenBtn);

    rtmpLayout->addLayout(rtmpLeftLayout);
    rtmpLayout->addLayout(rtmpCenterLayout, 1);
    rtmpLayout->addLayout(rtmpRightLayout);

    // 直播播放暂停信号槽 - 独立逻辑，无任何野指针
    connect(m_rtmpPlayBtn, &QPushButton::clicked, this, [this](){
        m_isPlaying = !m_isPlaying;
        if(m_isPlaying){
            qDebug() << "RTMP 播放中";
            m_rtmpPlayBtn->setIcon(QIcon("../images/icons/pause.png"));
            emit playClicked();
        }else{
            qDebug() << "RTMP 已暂停";
            m_rtmpPlayBtn->setIcon(QIcon("../images/icons/play.png"));
            emit pauseClicked();
        }
    });
    connect(m_rtmpVolumeSlider, &QSlider::valueChanged, this, &StandardController::onVolumeChanged);
    connect(m_rtmpFullscreenBtn, &QPushButton::clicked, this, &StandardController::fullscreenToggled);
}