//
// Created by lsy on 2025/9/22.
//

#include "../Header/Ui/BaseControlBar.h"

#include <QTime>
#include <QStyle>

#include "../Header/Ui/AudioControlBar.h"

BaseControlBar::BaseControlBar(QWidget *parent)
    : QWidget(parent)
    , m_topLeftArea(nullptr)
    , m_topRightArea(nullptr)
    , m_mainLayout(nullptr)
    , m_btnPrev(nullptr)
    , m_btnPlayPause(nullptr)
    , m_btnNext(nullptr)
    , m_progressSlider(nullptr)
    , m_timeLabel(nullptr)
    , m_btnMute(nullptr)
    , m_currentVolume(80)
    , m_isMuted(false)
    , m_currentState(PlayState::Stopped)
{
    initUI();
    connectSignals();

    m_progressSlider->setRange(0, 1000);
}

PlayState BaseControlBar::currentState() const
{
    return m_currentState;
}

void BaseControlBar::setControlsEnabled(bool enabled)
{
    if (m_btnPrev) m_btnPrev->setEnabled(enabled);
    if (m_btnPlayPause) m_btnPlayPause->setEnabled(enabled);
    if (m_btnNext) m_btnNext->setEnabled(enabled);
    if (m_progressSlider) m_progressSlider->setEnabled(enabled);
    if (m_btnMute) m_btnMute->setEnabled(enabled);
}

QWidget* BaseControlBar::getTopLeftArea() const
{
    return m_topLeftArea;
}

// 实现右区域访问接口（解决未识别问题）
QWidget* BaseControlBar::getTopRightArea() const
{
    return m_topRightArea;
}

QPushButton* BaseControlBar::getVolumeButton() const
{
    return m_btnMute;
}

QPushButton* BaseControlBar::getPlayPauseButton() const
{
    return m_btnPlayPause;
}

QSlider* BaseControlBar::getProgressSlider() const
{
    return m_progressSlider;
}

QLabel* BaseControlBar::getCurrentTimeLabel() const
{
    return m_timeLabel;
}

QLabel* BaseControlBar::getTotalTimeLabel() const
{
    return m_timeLabel;
}

QWidget* BaseControlBar::getBaseControlArea() const
{
    return const_cast<QWidget*>(static_cast<const QWidget*>(this));
}

void BaseControlBar::setBaseProgress(qint64 currentMs, qint64 totalMs)
{
    if (!m_progressSlider || !m_timeLabel) return;

    // 通过比例计算滑块值
    if (totalMs > 0) {
        double progress = static_cast<double>(currentMs) / totalMs;
        int sliderValue = static_cast<int>(progress * m_progressSlider->maximum());

        // 安全更新：防止触发不必要的信号
        bool wasBlocked = m_progressSlider->signalsBlocked();
        m_progressSlider->blockSignals(true);
        m_progressSlider->setValue(sliderValue);
        m_progressSlider->blockSignals(wasBlocked);
    }

    // 更新时间标签（这部分不变）
    m_timeLabel->setText(QString("%1 / %2")
        .arg(formatTime(currentMs))
        .arg(formatTime(totalMs)));

}

QString BaseControlBar::formatTime(qint64 milliseconds)
{
    qint64 seconds = milliseconds / 1000;
    return QString("%1:%2")
        .arg(seconds / 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
}

void BaseControlBar::onPlayStateChanged(PlayState state)
{
    m_currentState = state;
    updatePlayPauseIcon();
}


void BaseControlBar::onMutedChanged(bool muted)
{
    m_isMuted = muted;
    updateVolumeIcon();
    emit muteClicked(muted);
}

void BaseControlBar::onPlayPauseClicked()
{
    if (m_currentState == PlayState::Playing)
    {
        emit pauseClicked();
        m_currentState = PlayState::Paused;
    }
    else
    {
        emit playClicked();
        m_currentState = PlayState::Playing;
    }
    updatePlayPauseIcon();
    qDebug() << "选中状态按钮，目前播放状态" << static_cast<int>(m_currentState) << "[0,1,2](Playing,Stopped,Paused)";
}


void BaseControlBar::initUI() {
    // 1. 主布局 - 更紧凑的边距和间距
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(6, 4, 6, 4);
    m_mainLayout->setSpacing(4);

    // 2. 上层扩展区 - 压缩到最小高度，提供基础扩展能力
    QHBoxLayout* topAreaLayout = new QHBoxLayout();
    topAreaLayout->setContentsMargins(0, 0, 0, 0);
    topAreaLayout->setSpacing(6);

    m_topLeftArea = new QWidget(this);
    m_topLeftArea->setFixedHeight(16); // 进一步压缩
    m_topRightArea = new QWidget(this);
    m_topRightArea->setFixedHeight(16);

    topAreaLayout->addWidget(m_topLeftArea);
    topAreaLayout->addStretch();
    topAreaLayout->addWidget(m_topRightArea);

    m_mainLayout->addLayout(topAreaLayout);

    // 3. 核心控制区 - 保持视觉焦点但更紧凑
    QHBoxLayout* controlBtnLayout = new QHBoxLayout();
    controlBtnLayout->setSpacing(12);
    controlBtnLayout->setAlignment(Qt::AlignCenter);

    // 按钮尺寸优化
    m_btnPrev = new QPushButton(this);
    m_btnPrev->setIcon(QIcon(":/icons/prev.png"));
    m_btnPrev->setIconSize(QSize(16, 16));
    m_btnPrev->setFixedSize(24, 24);
    m_btnPrev->setStyleSheet("QPushButton { border: none; border-radius: 12px; background: transparent; }"
                           "QPushButton:hover { background: #e0e0e0; }");

    m_btnPlayPause = new QPushButton(this);
    m_btnPlayPause->setIcon(QIcon(":/icons/play.png"));
    m_btnPlayPause->setIconSize(QSize(20, 20));
    m_btnPlayPause->setFixedSize(36, 36);
    m_btnPlayPause->setStyleSheet("QPushButton { border: none; border-radius: 18px; background-color: #3a7bd5; }"
                                "QPushButton:hover { background-color: #2d62b0; }");

    m_btnNext = new QPushButton(this);
    m_btnNext->setIcon(QIcon(":/icons/next.png"));
    m_btnNext->setIconSize(QSize(16, 16));
    m_btnNext->setFixedSize(24, 24);
    m_btnNext->setStyleSheet("QPushButton { border: none; border-radius: 12px; background: transparent; }"
                           "QPushButton:hover { background: #e0e0e0; }");

    controlBtnLayout->addWidget(m_btnPrev);
    controlBtnLayout->addWidget(m_btnPlayPause);
    controlBtnLayout->addWidget(m_btnNext);

    m_mainLayout->addLayout(controlBtnLayout);

    // 4. 进度条与时间区 - 更紧凑的布局
    QHBoxLayout* progressLayout = new QHBoxLayout();
    progressLayout->setSpacing(6);
    progressLayout->setContentsMargins(0, 0, 0, 0);

    m_timeLabel = new QLabel(this);
    m_timeLabel->setText("00:00 / 00:00");
    m_timeLabel->setStyleSheet("color: #666; font-size: 10px; font-family: Arial;");
    m_timeLabel->setFixedWidth(65);
    m_timeLabel->setAlignment(Qt::AlignCenter);

    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setMinimum(0);
    m_progressSlider->setMaximum(1000);
    m_progressSlider->setValue(0);
    m_progressSlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "    height: 4px;"
        "    background: #e0e0e0;"
        "    border-radius: 2px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "    background: #3a7bd5;"
        "    border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "    width: 12px;"
        "    height: 12px;"
        "    margin: -4px 0;"
        "    background: #3a7bd5;"
        "    border-radius: 6px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "    background: #2d62b0;"
        "    width: 14px;"
        "    height: 14px;"
        "    margin: -5px 0;"
        "}"
    );

    progressLayout->addWidget(m_timeLabel);
    progressLayout->addWidget(m_progressSlider);

    m_mainLayout->addLayout(progressLayout);

    // 5. 底部功能区 - 整合音量和其他控制
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(8);
    bottomLayout->setContentsMargins(0, 0, 0, 0);

    // 左侧可扩展区域
    QWidget* bottomLeftArea = new QWidget(this);
    bottomLeftArea->setFixedHeight(20);
    bottomLayout->addWidget(bottomLeftArea);

    bottomLayout->addStretch();

    // 音量控制区
    m_btnMute = new QPushButton(this);
    m_btnMute->setIcon(QIcon(":/icons/volume.png"));
    m_btnMute->setIconSize(QSize(14, 14));
    m_btnMute->setFixedSize(20, 20);
    m_btnMute->setStyleSheet("QPushButton { border: none; border-radius: 10px; background: transparent; }"
                           "QPushButton:hover { background: #e0e0e0; }");

    bottomLayout->addWidget(m_btnMute);

    m_mainLayout->addLayout(bottomLayout);

    // 7. 状态初始化
    m_isMuted = false;
    m_currentState = PlayState::Paused;

    // 固定总高度为80px，更加紧凑
    setLayout(m_mainLayout);
    setFixedHeight(80);
}


void BaseControlBar::connectSignals()
{
    connect(m_btnPlayPause, &QPushButton::clicked, this, &BaseControlBar::onPlayPauseClicked);
    connect(m_btnPrev, &QPushButton::clicked, this, &BaseControlBar::prevClicked);
    connect(m_btnNext, &QPushButton::clicked, this, &BaseControlBar::nextClicked);
    connect(m_progressSlider, &QSlider::sliderMoved, this, [this](int value) {
        emit progressChanged(static_cast<qint64>(value));
    });

}

void BaseControlBar::updatePlayPauseIcon()
{
    if (!m_btnPlayPause) return;

    if (m_currentState == PlayState::Playing) {
        m_btnPlayPause->setIcon(QIcon("../images/icons/pause.png"));
    } else {
        m_btnPlayPause->setIcon(QIcon("../images/icons/play.png"));
    }
}

void BaseControlBar::updateVolumeIcon()
{
    if (!m_btnMute) return;

    m_btnMute->setIcon(QIcon(
        m_isMuted ? "../images/icons/mute.png" :
        (m_currentVolume == 0 ? "../images/icons/mute.png" : "../images/icons/volume.png")
    ));
}

void BaseControlBar::onVolumeChanged(int volume)
{
    if (volume < 0 || volume > 100) return;
    m_currentVolume = volume;
    m_isMuted = (volume == 0);
    updateVolumeIcon();
    // 注意：已删除涉及 m_popupVolumeSlider 的代码（基类不再管理弹窗）
}

void BaseControlBar::onMuteBtnClicked()
{
    // 基类不再处理音量按钮逻辑，留给子类实现
    // 保留空实现避免链接错误
}