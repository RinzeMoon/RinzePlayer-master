//
// Created by lsy on 2025/9/22.
//

#include "../Header/Ui/AudioControlBar.h"
#include <../Header/Ui/AudioPlayListWidget.h>

#include <QMouseEvent>
#include <QLayout>
#include <QTimer>
#include <QMainWindow>
#include <QApplication>
#include <../Header/Ui/MusicSheetUI.h>

// 初始化静态成员
AudioControlBar* AudioControlBar::m_instance = nullptr;
QMutex AudioControlBar::m_mutex;

AudioControlBar::AudioControlBar(QWidget *parent)
    : BaseControlBar(parent)
    , m_coverLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_btnPlayList(nullptr)
    , m_playlistDlg(nullptr)
{
    // 重新初始化UI布局
    reinitUI();

    // 初始化音频控件
    initAudioControls();
    initVolumePopup();
    connectSignals();
    // 连接信号槽
    // connect(getVolumeButton(), &QPushButton::clicked, this, &AudioControlBar::onVolumeButtonClicked);
    connect(m_btnPlayPause, &QPushButton::clicked, this, &AudioControlBar::changeTitleScrollStateByPlayState);
    m_parser = AudioMetaParser::getInstance();

    m_progressSlider->setRange(0, 1000);
}

AudioControlBar::~AudioControlBar()
{
    // 程序退出时，手动销毁弹窗（释放内存）
    if (m_playlistDlg) {
        delete m_playlistDlg;
        m_playlistDlg = nullptr;
        qDebug() << "[AudioControlBar] 析构→手动销毁弹窗";
    }
}

void AudioControlBar::reinitUI()
{
    // 删除基类的布局
    if (m_mainLayout) {
        delete m_mainLayout;
    }

    // -------------------------- 核心修复：加深顶部分隔线颜色+改用阴影，确保可见 --------------------------
    this->setStyleSheet(R"(
        AudioControlBar {
            /* 改用顶部阴影：比边框更易察觉，1px高度，颜色加深到#e1e8ed（明显浅灰） */
            box-shadow: 0 -1px 0 #e1e8ed;
            background: white; /* 明确背景色，确保阴影对比度 */
            margin: 0;
            padding: 0;
        }
    )");

    // 主布局：彻底消除所有外部间距
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    this->setContentsMargins(0, 0, 0, 0);


    // -------------------------- 最左侧：封面区域（保持不变） --------------------------
    QWidget* coverWidget = new QWidget(this);
    coverWidget->setFixedSize(110, 120);
    coverWidget->setStyleSheet("background: transparent;");
    coverWidget->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout* coverLayout = new QVBoxLayout(coverWidget);
    coverLayout->setContentsMargins(6, 6, 6, 6);
    coverLayout->setAlignment(Qt::AlignCenter);

    m_coverLabel = new QLabel(coverWidget);
    m_coverLabel->setFixedSize(98, 98);
    m_coverLabel->setScaledContents(true);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setText("封面");
    m_coverLabel->setStyleSheet(R"(
        QLabel {
            border-radius: 8px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f0f2f5, stop:1 #e5e6eb);
            color: #6c757d;
            font-size: 12px;
            font-weight: 500;
        }
    )");

    coverLayout->addWidget(m_coverLabel);
    m_mainLayout->addWidget(coverWidget);


    // -------------------------- 左侧第二部分：信息和功能按钮（保持不变） --------------------------
    QWidget* infoAndBtnWidget = new QWidget(this);
    infoAndBtnWidget->setFixedWidth(180);
    infoAndBtnWidget->setStyleSheet("background: transparent;");
    infoAndBtnWidget->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout* infoBtnLayout = new QVBoxLayout(infoAndBtnWidget);
    infoBtnLayout->setContentsMargins(8, 6, 8, 6);
    infoBtnLayout->setSpacing(6);

    // 歌曲信息区域
    QWidget* infoWidget = new QWidget(infoAndBtnWidget);
    infoWidget->setStyleSheet("background: transparent;");
    QVBoxLayout* infoLayout = new QVBoxLayout(infoWidget);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(1);

    m_titleLabel = new ScrollingLabel("未选择歌曲", infoWidget);
    m_titleLabel->setStyleSheet(R"(
        color: #495057;
        font-size: 14px;
        font-weight: 600;
        background: transparent;
    )");
    m_titleLabel->setFixedHeight(19);

    artistLabel = new QLabel("未知艺术家", infoWidget);
    artistLabel->setStyleSheet(R"(
        color: #6c757d;
        font-size: 12px;
        background: transparent;
    )");
    artistLabel->setFixedHeight(15);

    infoLayout->addWidget(m_titleLabel);
    infoLayout->addWidget(artistLabel);

    // 功能按钮
    QHBoxLayout* funcBtnLayout = new QHBoxLayout();
    funcBtnLayout->setSpacing(6);
    funcBtnLayout->setContentsMargins(0, 0, 0, 0);

    m_btnMute = new QPushButton(infoAndBtnWidget);
    m_btnMute->setFixedSize(34, 34);
    m_btnMute->setIcon(QIcon("../images/icons/volume.png"));
    m_btnMute->setIconSize(QSize(18, 18));
    m_btnMute->setStyleSheet(R"(
        QPushButton {
            background: white;
            border-radius: 17px;
            border: 1px solid #e1e8ed;
            outline: none;
        }
        QPushButton:hover {
            background: #f8f9fa;
            border-color: #c9ccd4;
        }
    )");

    m_btnPlayList = new QPushButton(infoAndBtnWidget);
    m_btnPlayList->setFixedSize(34, 34);
    m_btnPlayList->setIcon(QIcon("../images/icons/playlist.png"));
    m_btnPlayList->setIconSize(QSize(18, 18));
    m_btnPlayList->setStyleSheet(R"(
        QPushButton {
            background: white;
            border-radius: 17px;
            border: 1px solid #e1e8ed;
            outline: none;
        }
        QPushButton:hover {
            background: #f8f9fa;
            border-color: #c9ccd4;
        }
    )");

    m_btnModel = new QPushButton(infoAndBtnWidget);
    m_btnModel->setFixedSize(34, 34);
    m_btnModel->setIcon(QIcon("../images/icons/setting.png"));
    m_btnModel->setIconSize(QSize(18, 18));
    m_btnModel->setStyleSheet(R"(
        QPushButton {
            background: white;
            border-radius: 17px;
            border: 1px solid #e1e8ed;
            outline: none;
        }
        QPushButton:hover {
            background: #f8f9fa;
            border-color: #c9ccd4;
        }
    )");


    funcBtnLayout->addWidget(m_btnMute);
    funcBtnLayout->addWidget(m_btnPlayList);
    funcBtnLayout->addWidget(m_btnModel);
    funcBtnLayout->addStretch();

    infoBtnLayout->addWidget(infoWidget);
    infoBtnLayout->addLayout(funcBtnLayout);

    m_mainLayout->addWidget(infoAndBtnWidget);


    // -------------------------- 右侧（可拉伸）：进度控制区（保持不变） --------------------------
    QWidget* controlAndProgressWidget = new QWidget(this);
    controlAndProgressWidget->setStyleSheet("background: transparent;");
    controlAndProgressWidget->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout* controlProgressLayout = new QVBoxLayout(controlAndProgressWidget);
    controlProgressLayout->setContentsMargins(0, 6, 8, 6);
    controlProgressLayout->setSpacing(10);

    // 播放控制按钮
    QHBoxLayout* controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(10);
    controlLayout->setAlignment(Qt::AlignCenter);
    controlLayout->setContentsMargins(0, 0, 0, 0);

    m_btnPrev = new QPushButton(controlAndProgressWidget);
    m_btnPrev->setFixedSize(42, 42);
    m_btnPrev->setIcon(QIcon("../images/icons/prev.png"));
    m_btnPrev->setIconSize(QSize(19, 19));
    m_btnPrev->setStyleSheet(R"(
        QPushButton {
            background: white;
            border-radius: 21px;
            border: 1px solid #e1e8ed;
            outline: none;
        }
        QPushButton:hover {
            background: #f8f9fa;
            border-color: #c9ccd4;
        }
    )");

    m_btnPlayPause = new QPushButton(controlAndProgressWidget);
    m_btnPlayPause->setIcon(QIcon("../images/icons/play.png"));
    m_btnPlayPause->setIconSize(QSize(19, 19));
    m_btnPlayPause->setFixedSize(46, 46);
    m_btnPlayPause->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #d0d3d6, stop:1 #c4c7cc);
            border-radius: 23px;
            border: none;
            outline: none;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #c4c7cc, stop:1 #b8bbc2);
        }
    )");

    m_btnNext = new QPushButton(controlAndProgressWidget);
    m_btnNext->setFixedSize(42, 42);
    m_btnNext->setIcon(QIcon("../images/icons/next.png"));
    m_btnNext->setIconSize(QSize(19, 19));
    m_btnNext->setStyleSheet(R"(
        QPushButton {
            background: white;
            border-radius: 21px;
            border: 1px solid #e1e8ed;
            outline: none;
        }
        QPushButton:hover {
            background: #f8f9fa;
            border-color: #c9ccd4;
        }
    )");

    controlLayout->addWidget(m_btnPrev);
    controlLayout->addWidget(m_btnPlayPause);
    controlLayout->addWidget(m_btnNext);

    // 进度条区域
    QHBoxLayout* progressLayout = new QHBoxLayout();
    progressLayout->setSpacing(6);
    progressLayout->setContentsMargins(0, 0, 0, 0);

    m_timeLabel = new QLabel(controlAndProgressWidget);
    m_timeLabel->setText("00:00 / 00:00");
    m_timeLabel->setStyleSheet(R"(
        color: #6c757d;
        font-size: 12px;
        font-weight: 500;
        background: transparent;
    )");
    m_timeLabel->setFixedWidth(86);

    m_progressSlider = new QSlider(Qt::Horizontal, controlAndProgressWidget);
    m_progressSlider->setMinimum(0);
    m_progressSlider->setMaximum(1000);
    m_progressSlider->setValue(0);
    progressLayout->addWidget(m_progressSlider, 1);
    m_progressSlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            height: 6px;
            background: #e1e8ed;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #c9ccd4, stop:1 #bcc0c8);
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            width: 16px;
            height: 16px;
            margin: -5px 0;
            background: white;
            border-radius: 8px;
            border: 2px solid #b4b8c0;
        }
        QSlider::handle:horizontal:hover {
            background: #b4b8c0;
            border: 2px solid white;
        }
    )");

    progressLayout->addWidget(m_timeLabel);

    controlProgressLayout->addLayout(controlLayout);
    controlProgressLayout->addLayout(progressLayout);

    m_mainLayout->addWidget(controlAndProgressWidget, 1);

    //-----------模式弹窗设置
    // 圆形弹窗大小保持150x150（可根据需要微调）
    m_ModelPopup = new QWidget(this, Qt::Popup | Qt::FramelessWindowHint);
    m_ModelPopup->setFixedSize(160, 180);  // 略微增大弹窗尺寸适配更大的按钮
    m_ModelPopup->setStyleSheet(
        "background-color: white;"
        "border-radius: 12px;"  // 圆角半径12px（适中的弧度，非圆形）
        "border: 1px solid #e0e0e0;"
    );

    // 布局边距减小，给按钮更多空间
    m_popupLayout = new QVBoxLayout(m_ModelPopup);
    m_popupLayout->setContentsMargins(15, 15, 15, 15);  // 边距从20减到15
    m_popupLayout->setSpacing(8);  // 增大按钮间距
    m_popupLayout->setAlignment(Qt::AlignCenter);

    // 创建按钮
    m_btnListPlay = new QPushButton("列表播放", m_ModelPopup);
    m_btnLoopPlay = new QPushButton("循环播放", m_ModelPopup);
    m_btnRandomPlay = new QPushButton("随机播放", m_ModelPopup);
    // 状态文字
    m_ModelLabel = new QLabel(m_ModelPopup);
    m_ModelLabel->setText(QString("当前模式: 列表播放"));


    // 增大按钮尺寸的核心样式调整
    QString btnStyle =
        "QPushButton {"
        "   background-color: transparent;"
        "   color: #333333;"
        "   padding: 10px 15px;"  // 垂直10px+水平15px，显著增大按钮点击区域
        "   border: none;"
        "   border-radius: 6px;"  // 圆角稍大，更协调
        "   font-size: 15px;"     // 字体略大，提升可读性
        "   min-width: 100px;"    // 最小宽度限制，确保按钮不窄
        "}"
        "QPushButton:hover {"
        "   background-color: #f0f0f0;"  // 保持合理灰色hover
        "}"
        "QPushButton:pressed {"
        "   background-color: #e0e0e0;"
        "}";

    m_btnListPlay->setStyleSheet(btnStyle);
    m_btnLoopPlay->setStyleSheet(btnStyle);
    m_btnRandomPlay->setStyleSheet(btnStyle);

    // 设置按钮属性
    m_btnListPlay->setProperty("mode", QVariant::fromValue(PlayMode::Sequential));
    m_btnLoopPlay->setProperty("mode", QVariant::fromValue(PlayMode::LoopSingle));
    m_btnRandomPlay->setProperty("mode", QVariant::fromValue(PlayMode::Random));

    m_popupLayout->addWidget(m_ModelLabel);
    m_popupLayout->addWidget(m_btnListPlay);
    m_popupLayout->addWidget(m_btnLoopPlay);
    m_popupLayout->addWidget(m_btnRandomPlay);

    // 整体设置
    setFixedHeight(120);
}

void AudioControlBar::connectSignals()
{
    auto controller = PlayQueueController::getInstance();
    auto player = AudioPlayer::getInstance();

    // 连接音量按钮
    // 确保m_focusConn在类中声明为QMetaObject::Connection类型
    // 在AudioControlBar.h中添加：QMetaObject::Connection m_focusConn;

    connect(m_btnMute, &QPushButton::clicked, this, [this]() {
    // 1. 先判断弹窗是否初始化，避免空指针崩溃
    if (!m_volumePopup) {
        qDebug() << "错误：m_volumePopup未初始化！";
        return;
    }

    // 2. 切换弹窗显示/隐藏状态
    if (m_volumePopup->isVisible()) {
        m_volumePopup->hide();
        qDebug() << "收起音量弹窗";
        // 隐藏时断开焦点连接，避免无效监听
        if (m_focusConn) {
            QObject::disconnect(m_focusConn);
            m_focusConn = QMetaObject::Connection(); // 重置连接
        }
        return;
    }

    // 3. 计算弹窗位置（带屏幕边界检查）
    QPoint btnPos = m_btnMute->mapToGlobal(QPoint(0, 0)); // 按钮在屏幕上的坐标
    QRect btnRect = m_btnMute->geometry(); // 按钮自身尺寸
    QScreen* screen = QApplication::screenAt(btnPos); // 获取按钮所在屏幕
    if (!screen) {
        qDebug() << "错误：无法获取屏幕信息！";
        return;
    }
    QRect screenRect = screen->availableGeometry(); // 屏幕可用区域（排除任务栏）

    // 计算理想位置（按钮上方居中）
    int popupX = btnPos.x() + (btnRect.width() - m_volumePopup->width()) / 2;
    int popupY = btnPos.y() - m_volumePopup->height() - 8; // 8px间距

    // 强制限制在屏幕内（关键！）
    popupX = qBound(screenRect.left(), popupX, screenRect.right() - m_volumePopup->width());
    popupY = qBound(screenRect.top(), popupY, screenRect.bottom() - m_volumePopup->height());

    // 移动并显示弹窗
    m_volumePopup->move(popupX, popupY);
    m_volumePopup->show();
    qDebug() << "显示音量弹窗，位置：" << popupX << "," << popupY;

    // 4. 连接焦点变化信号（优化逻辑：只在弹窗显示时监听）
    if (m_focusConn) {
        QObject::disconnect(m_focusConn);
    }
    m_focusConn = QObject::connect(qApp, &QApplication::focusChanged, this,
        [this](QWidget* oldFocus, QWidget* newFocus) {
            // 只有当旧焦点在弹窗内，且新焦点不在弹窗内时，才隐藏弹窗
            bool oldInPopup = (oldFocus && m_volumePopup->isAncestorOf(oldFocus));
            bool newInPopup = (newFocus && m_volumePopup->isAncestorOf(newFocus));
            if (oldInPopup && !newInPopup) {
                m_volumePopup->hide();
                qDebug() << "焦点离开弹窗，自动隐藏";
                // 隐藏后断开连接，避免重复触发
                if (m_focusConn) {
                    QObject::disconnect(m_focusConn);
                    m_focusConn = QMetaObject::Connection();
                }
            }
        }
    );
});

    // 连接Mode窗口
    connect(m_btnModel, &QPushButton::clicked, this, [this](){
        showPopupNearby(m_btnModel);
    });

    // 连接模式选择
    connect(m_btnRandomPlay, &QPushButton::clicked, this, [this]()
    {
        this->onbtnRandomClicked();
        m_ModelLabel->setText("当前模式: 随机播放");
    });

    connect(m_btnLoopPlay, &QPushButton::clicked, this, [this]()
    {
        this->onbtnLoopClicked();
        m_ModelLabel->setText("当前模式: 单曲循环");
    });

    connect(m_btnListPlay, &QPushButton::clicked, this, [this]()
    {
        this->onbtnListPlayClicked();
        m_ModelLabel->setText("当前模式: 列表播放");
    });

    //播放与暂停
    connect(m_btnPlayPause, &QPushButton::clicked, this, [controller,this]() {
    m_currentState = controller->getPlaybackState();
    if (m_currentState == RinGlobal::PlayState::Playing)
    {
        qDebug() << "[PAUSED]";
        controller->pause();
        updatePlayPauseIcon();
    }
    else if (m_currentState == RinGlobal::PlayState::Paused)
    {
        qDebug() << "[RESUMING]";
        controller->resume();  // 调用 resume() 而不是 play()
        updatePlayPauseIcon();
    }
    else  // Stopped 或其他状态
    {
        qDebug() << "[PLAYING from stop]";
        controller->play();  // 只有停止状态才调用 play()
        updatePlayPauseIcon();
    }
});
    connect(player,&AudioPlayer::stateChanged,this,[=](PlayState state)
    {
        m_currentState = state;
        updatePlayPauseIcon();
    });


    connect(m_btnPrev, &QPushButton::clicked, controller, &PlayQueueController::previous);
    connect(m_btnNext, &QPushButton::clicked, controller, &PlayQueueController::next);
    connect(controller,&PlayQueueController::sendCurrentSongMeta,this,[=](const SongMeta Sm)
    {
        qDebug() << "[AudioControlBar] 接受来自Queue的meta";
        m_currentSongMeta = Sm;
        qDebug() << "目前meta的URL为" << Sm.coverUrl;
        insertInfoByMeta();

        emit setPlayStatusToPlayList_Ui(Sm.id);
    });

    connect(controller,&PlayQueueController::playbackStateChanged,this,[=](PlayState state)
    {
        m_currentState = state;
    });

    // 进度条
    connect(m_progressSlider, &QSlider::sliderReleased, this, [this, controller]() {
        qDebug() << "[调试] 滑块Value:" << m_progressSlider->value();
        qDebug() << "[调试] 滑块Maximum:" << m_progressSlider->maximum(); // 这是关键！
        qDebug() << "[调试] 存储的总时长m_totalDurationMs:" << m_totalDurationMs;

        // 2. 进行计算
        int64_t position = static_cast<int64_t>(
            (static_cast<double>(m_progressSlider->value()) / m_progressSlider->maximum()) * m_totalDurationMs
        );
        qDebug() << "[Slider跳转] 计算结果position:" << position;
        controller->seek(position);
        qDebug() << "[状态] 跳转请求已发送，标志重置为 false";
        m_isProgressSliderDragging = false;
    });

    connect(m_btnPlayList,&QPushButton::clicked,this,&AudioControlBar::onPlayDialogClicked);

    connect(controller,&PlayQueueController::progressUpdated,this,&AudioControlBar::onProgressUpdated);

    // 连接进度条信号
    connect(m_progressSlider, &QSlider::valueChanged,
            this, &AudioControlBar::onProgressSliderMoved);

    connect(m_progressSlider, &QSlider::sliderPressed,
            this, &AudioControlBar::onProgressSliderPressed);


}

void AudioControlBar::showPopupNearby(QWidget *target)
{
    if (!target || !m_ModelPopup) return;

    // 计算显示位置（圆形窗口中心对齐目标控件底部中心）
    QPoint targetBottomCenter = target->mapToGlobal(
        QPoint(target->width()/2, target->height())
    );

    // 圆形窗口左上角坐标（使窗口中心与目标底部中心对齐）
    QPoint popupPos(
        targetBottomCenter.x() - m_ModelPopup->width()/2,
        targetBottomCenter.y()
    );

    QScreen *screen = QApplication::screenAt(popupPos);
    if (!screen) return;

    QRect screenRect = screen->availableGeometry();
    // 边界检查（确保圆形不超出屏幕）
    if (popupPos.x() < screenRect.left()) {
        popupPos.setX(screenRect.left());
    } else if (popupPos.x() + m_ModelPopup->width() > screenRect.right()) {
        popupPos.setX(screenRect.right() - m_ModelPopup->width());
    }

    if (popupPos.y() + m_ModelPopup->height() > screenRect.bottom()) {
        // 若下方空间不足，显示在目标上方
        popupPos.setY(
            target->mapToGlobal(QPoint(0, 0)).y() - m_ModelPopup->height()
        );
    }

    m_ModelPopup->move(popupPos);
    m_ModelPopup->show();
}

void AudioControlBar::onModeButtonClicked()
{
    QPushButton *clickedBtn = qobject_cast<QPushButton*>(sender());
    if (!clickedBtn || !m_ModelPopup) return;

    PlayMode mode = clickedBtn->property("mode").value<PlayMode>();

    switch (mode) {
    case PlayMode::Sequential:
        qDebug() << "切换到列表播放";
        break;
    case PlayMode::LoopSingle:
        qDebug() << "切换到循环播放";
        break;
    case PlayMode::Random:
        qDebug() << "切换到随机播放";
        break;
    }

    m_ModelPopup->close();
}

// 重写：设置播放进度
void AudioControlBar::setProgress(qint64 currentMs, qint64 totalMs)
{
    // 调用基类方法更新进度条和时间标签
    setBaseProgress(currentMs, totalMs);
}

// 重写：设置音量
void AudioControlBar::setVolume(int volume)
{
    if (volume < 0 || volume > 100) return;

    // 更新基类音量状态
    onVolumeChanged(volume);

    // 同步音量滑块
    if (m_volumeSlider && m_volumeSlider->value() != volume) {
        m_volumeSlider->setValue(volume);
    }
}

// 重写：设置播放状态
void AudioControlBar::setPlayState(PlayState state)
{
    // 更新基类状态
    onPlayStateChanged(state);
}

// 设置歌曲标题
void AudioControlBar::setTitle(const QString &title)
{
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
}

// 设置封面图片
void AudioControlBar::setCoverImage(const QString &imagePath)
{
    if (!m_coverLabel) return;

    QPixmap pixmap(imagePath);
    if (pixmap.isNull()) {
        m_coverLabel->setText("封面");
        m_coverLabel->setStyleSheet("border-radius: 4px; background-color: #2D2D2D; color: #AAAAAA;");
    } else {
        m_coverLabel->setPixmap(pixmap.scaled(
            m_coverLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        ));
        m_coverLabel->setStyleSheet("border-radius: 4px;");
        m_coverLabel->setText("");
    }
}


void AudioControlBar::onVolumeBtnClicked()
{
    qDebug() << "触发VolumeBtn";
    if (!m_volumePopup) return;

    qDebug() << "弹窗状态：" << m_volumePopup->isVisible() << "，尺寸：" << m_volumePopup->width() << "x" << m_volumePopup->height();

    if (m_volumePopup->isVisible()) {
        qDebug() << "隐藏音量窗口";
        m_volumePopup->hide();
    } else {
        qDebug() << "准备显示音量窗口";
        QPushButton* volumeBtn = getVolumeButton();
        if (!volumeBtn) return;

        QPoint btnPos = volumeBtn->mapToGlobal(QPoint(0, 0)); // 按钮在屏幕上的左上角坐标
        QRect btnRect = volumeBtn->geometry(); // 按钮自身宽高
        QScreen* screen = QApplication::screenAt(btnPos);
        if (!screen) return;
        QRect screenRect = screen->availableGeometry(); // 屏幕可用区域（比如1920x1080）

        // 计算弹窗理想位置（按钮上方居中）
        int targetX = btnPos.x() + (btnRect.width() - m_volumePopup->width()) / 2;
        int targetY = btnPos.y() - m_volumePopup->height() - 5;

        // 强制限制在屏幕内（重点！）
        targetX = qBound(screenRect.left(), targetX, screenRect.right() - m_volumePopup->width());
        targetY = qBound(screenRect.top(), targetY, screenRect.bottom() - m_volumePopup->height());

        // 输出最终位置，确认是否在屏幕范围内
        qDebug() << "弹窗最终位置：" << targetX << "," << targetY
                 << "，屏幕范围：" << screenRect.left() << "-" << screenRect.right()
                 << "," << screenRect.top() << "-" << screenRect.bottom();

        m_volumePopup->move(targetX, targetY);
        m_volumePopup->show();
    }
}

// 音量滑块值变化
void AudioControlBar::onVolumeSliderChanged(int value)
{
    emit volumeRequested(value);
    setVolume(value); // 同步更新音量
}

// 音量滑块释放（延迟隐藏弹窗）
void AudioControlBar::onVolumeSliderReleased()
{
    QTimer::singleShot(2000, m_volumePopup, &QWidget::hide);
}

// 播放列表按钮点击
void AudioControlBar::onPlayListClicked()
{
    emit playListRequested();
}

// 实现基类纯虚函数
void AudioControlBar::onVolumeButtonClicked()
{
    onVolumeBtnClicked(); // 复用音量按钮点击逻辑
}

// 初始化音频专属控件
void AudioControlBar::initAudioControls()
{
    // 控件已经在reinitUI中创建，这里只需要额外的初始化
    if (m_titleLabel) {
        m_titleLabel->setMinimumWidth(150);
        m_titleLabel->setMaximumWidth(180);
        m_titleLabel->setScrollSpeed(200);
    }
}


// 初始化音量弹窗
void AudioControlBar::initVolumePopup()
{
    m_volumePopup = new QWidget(this, Qt::Popup | Qt::FramelessWindowHint);
    m_volumePopup->setFixedSize(120, 36);
    m_volumePopup->setStyleSheet(R"(
        background: white;
        border-radius: 8px;
        border: 1px solid #e1e8ed;
    )");

    QHBoxLayout* popupLayout = new QHBoxLayout(m_volumePopup);
    popupLayout->setContentsMargins(12, 8, 12, 8);

    m_volumeSlider = new QSlider(Qt::Horizontal, m_volumePopup);
    m_volumeSlider->setMinimum(0);
    m_volumeSlider->setMaximum(100);
    m_currentVolume = 50;
    m_volumeSlider->setValue(m_currentVolume);
    m_isMuted = (m_currentVolume == 0);
    m_volumeSlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            height: 4px;
            background: #e1e8ed;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #667eea, stop:1 #764ba2);
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 16px;
            height: 16px;
            margin: -6px 0;
            background: white;
            border-radius: 8px;
            border: 2px solid #667eea;
        }
        QSlider::handle:horizontal:hover {
            background: #667eea;
            border: 2px solid white;
        }
    )");

    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_currentVolume = value;
        m_isMuted = (value == 0);
        updateVolumeIcon();
    });

    popupLayout->addWidget(m_volumeSlider);
    m_volumePopup->setLayout(popupLayout);
    m_volumePopup->hide();
}


void AudioControlBar::onPlayDialogClicked() {
    qDebug() << "=== 控制栏按钮点击开始 ===";

    if (m_playlistDlg) {
        qDebug() << "弹窗已存在，切换显示/隐藏";

        if (m_playlistDlg->isVisible()) {
            m_playlistDlg->hide();
        } else {
            m_playlistDlg->refreshList();

            // 新位置计算：弹窗右侧中点对准MainWindow右侧中点
            QWidget* mainWindow = window();
            QRect mainRect = mainWindow->geometry();
            int popupWidth = 420;
            int popupHeight = 600;

            // 计算弹窗位置
            int popupX = mainRect.width() - popupWidth; // 右侧对齐
            int popupY = (mainRect.height() - popupHeight) / 2; // 垂直居中

            m_playlistDlg->move(popupX, popupY);
            m_playlistDlg->show();
            m_playlistDlg->raise();
        }

        qDebug() << "=== 控制栏按钮点击结束 ===";
        return;
    }

    // 创建新弹窗
    qDebug() << "创建新弹窗";
    QWidget* mainWindow = window();
    m_playlistDlg = new AudioPlaylistPopup(mainWindow);
    m_playlistDlg->setFixedSize(420, 600);

    // 新位置计算：弹窗右侧中点对准MainWindow右侧中点
    QRect mainRect = mainWindow->geometry();
    int popupWidth = 420;
    int popupHeight = 600;

    int popupX = mainRect.width() - popupWidth; // 右侧对齐
    int popupY = (mainRect.height() - popupHeight) / 2; // 垂直居中

    m_playlistDlg->move(popupX, popupY);

    // 刷新列表并显示
    m_playlistDlg->refreshList();
    m_playlistDlg->show();
    m_playlistDlg->raise();

    qDebug() << "弹窗位置:" << QPoint(popupX, popupY);
    qDebug() << "=== 控制栏按钮点击结束 ===";
}

void AudioControlBar::onbtnListPlayClicked()
{
    PlayQueueController::getInstance()->setPlayMode(RinGlobal::PlayMode::Sequential);
}

void AudioControlBar::onbtnLoopClicked()
{
    PlayQueueController::getInstance()->setPlayMode(RinGlobal::PlayMode::LoopSingle);
}

void AudioControlBar::onbtnRandomClicked()
{
    PlayQueueController::getInstance()->setPlayMode(RinGlobal::PlayMode::Random);
}

void AudioControlBar::initializeMusicSheetConnection(MusicSheetUI* musicSheetUi)
{
    if (!musicSheetUi) {
        qDebug() << "[AudioControlBar] 错误：MusicSheetUI 指针为空";
        return;
    }

    // 如果弹窗已存在，直接建立连接
    if (m_playlistDlg) {
        m_playlistDlg->connectToMusicSheet(musicSheetUi);
        qDebug() << "[AudioControlBar] 已为现有弹窗建立歌单连接";
        return;
    }

    qDebug() << "[AudioControlBar] 弹窗尚未创建，将在创建时自动建立连接";
    // 连接将在弹窗创建时建立
}

// 在AudioControlBar中添加辅助方法
QPoint AudioControlBar::calculatePopupPosition() {
    QWidget* mainWindow = window();
    QPoint barPos = this->mapTo(mainWindow, QPoint(0, 0));

    int popupX = barPos.x() + (this->width() - 420) / 2;
    int popupY = barPos.y() - 600 - 10;

    // 确保在MainWindow范围内
    QRect mainRect = mainWindow->rect();
    popupX = qMax(10, qMin(popupX, mainRect.width() - 430));
    popupY = qMax(10, popupY);

    return QPoint(popupX, popupY);
}

AudioPlaylistPopup* AudioControlBar::getPlaylistPopup()
{
    if (!m_playlistDlg) {
        // 弹窗未创建，立即创建
        qDebug() << "[AudioControlBar] 自动创建播放列表弹窗";
        QWidget* mainWindow = window();
        m_playlistDlg = new AudioPlaylistPopup(mainWindow);
        m_playlistDlg->setFixedSize(420, 600);

        // 设置初始位置（隐藏状态）
        QRect mainRect = mainWindow->geometry();
        int popupX = mainRect.width() - 420;
        int popupY = (mainRect.height() - 600) / 2;
        m_playlistDlg->move(popupX, popupY);
        m_playlistDlg->hide();
    }
    return m_playlistDlg;
}

void AudioControlBar::onProgressSliderPressed()
{
    m_isProgressSliderDragging = true;
    qDebug() << "[进度条] 用户开始拖动，标志设为 true";
}

// 进度条释放槽函数（无参数版本）
void AudioControlBar::onProgressSliderReleasedInternal() {
    if (m_isProgressSliderDragging && m_totalDurationMs > 0) {
        // 获取当前滑块值（百分比）
        int sliderValue = m_progressSlider->value();

        // 计算目标毫秒数
        qint64 targetMs = static_cast<qint64>(sliderValue * m_totalDurationMs / 100.0);

        // 确保在有效范围内
        targetMs = qBound(0LL, targetMs, m_totalDurationMs);

        qDebug() << "[进度条] 跳转到：" << targetMs << "ms";

        // 发送跳转请求（毫秒数）
        emit progressSliderReleased(static_cast<int>(targetMs));

        // 重置拖动状态
        m_isProgressSliderDragging = false;

        // 更新当前时间显示（如果有时间标签）
        // updateCurrentTimeDisplay(targetMs);
        setProgress(targetMs,m_totalDurationMs);
    }
}

// 进度更新槽函数
void AudioControlBar::onProgressUpdated(float progress, int64_t currentMs, int64_t totalMs) {

    // qDebug() << "[AudioControlBar]:#######################进度条变化信号";
    // 保存总时长
    m_totalDurationMs = totalMs;

    // 如果用户没有拖动进度条，才自动更新
    if (!m_isProgressSliderDragging) {
        // 计算进度条值 (0-100)
        int sliderValue = static_cast<int>(progress * 1000);

        // 防止信号循环：临时阻塞信号
        bool wasBlocked = m_progressSlider->signalsBlocked();
        m_progressSlider->blockSignals(true);
        m_progressSlider->setValue(sliderValue);
        m_progressSlider->blockSignals(wasBlocked);

        // 更新进度显示（通过setProgress函数）
        setProgress(currentMs, m_totalDurationMs);

    }
}
