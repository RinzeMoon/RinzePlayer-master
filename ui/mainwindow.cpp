// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "ui/effectspanel.h"
#include "ui/reportdialog.h"
#include "ui/reportgenerator.h"
#include "ui/roomdialog.h"
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMimeData>
#include <QUrl>
#include <QKeyEvent>
#include <QStatusBar>
#include <QLabel>
#include <QFileInfo>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QMenuBar>
#include <QMenu>
#include <QDockWidget>
#include <QThread>
#include <windows.h>
#include <psapi.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_controller(nullptr), m_videoWidget(nullptr), m_controlBar(nullptr),
      m_visualWidget(nullptr), m_visualizerConnected(false), m_effectsPanel(nullptr),
      m_isDragging(false)
{
    // Create controller first
    m_controller = new MediaController(this);

    // 初始化双人观影模块
    m_chatRoomClient = new ChatRoomClient(this);
    m_statusClient = new StatusClient(this);
    m_roomSession = new RoomSession(this);
    m_syncManager = new SyncManager(this);

    m_roomSession->setChatRoomClient(m_chatRoomClient);
    m_roomSession->setMediaController(m_controller);
    m_syncManager->setChatRoomClient(m_chatRoomClient);
    m_controller->setSyncManager(m_syncManager);
    m_controller->setRoomSession(m_roomSession);

    connect(m_roomSession, &RoomSession::roomClosed, this, &MainWindow::onRoomClosed);
    connect(m_roomSession, &RoomSession::exportFinished, this, &MainWindow::onRoomExportFinished);

    // ---- 进入/退出房间 ----
    connect(m_roomSession, &RoomSession::roomCreated, this, [this](const QString &code) {
        m_inRoom = true;
        m_syncManager->setRole("host");
        m_controlBar->setRoomMode(false);
        m_createRoomAction->setEnabled(false); m_joinRoomAction->setEnabled(false);
        m_leaveRoomAction->setEnabled(true);
        updateRoomInfoBar();
        m_statusLabel->setText(QString("房间 %1 — 等待加入...").arg(code));
    });
    connect(m_roomSession, &RoomSession::joinedRoom, this, [this]() {
        m_inRoom = true;
        m_syncManager->setRole("guest");
        m_controlBar->setRoomMode(true);
        m_createRoomAction->setEnabled(false); m_joinRoomAction->setEnabled(false);
        m_leaveRoomAction->setEnabled(true);
        updateRoomInfoBar();
        m_statusLabel->setText(QString("房间 %1 — 已加入").arg(m_roomSession->roomCode()));
    });
    connect(m_roomSession, &RoomSession::peerJoined, this, [this](const QString &name) {
        m_statusLabel->setText(QString("房间 %1 — %2 已加入").arg(m_roomSession->roomCode(), name));
    });

    // ---- Server sync 消息 ----
    connect(m_roomSession, &RoomSession::syncMessageReceived, this, [this](const QJsonObject &msg) {
        QString action = msg["action"].toString();
        QJsonObject data = msg["data"].toObject();
        QString url = data["url"].toString();

        // preload: Guest 收到预加载 URL
        if (action == "preload" && !url.isEmpty() && m_roomSession->role() == "guest"
            && m_currentRoomUrl != url) {
            m_currentRoomUrl = url;
            if (m_readyOverlay) m_readyOverlay->setState(ReadyOverlay::Waiting);
            openForRoom(QUrl(url));
        }

        // start: 双方就绪, 激活同步
        if (action == "start") {
            m_iAmReady = false;
            if (m_readyOverlay) m_readyOverlay->setState(ReadyOverlay::Done);
            int pos = data["position"].toInt();
            qDebug() << "[MainWindow] start pos=" << pos << "role=" << m_roomSession->role();
            m_controller->executeRemoteSeek(pos);  // 设 demux 标志
            m_syncManager->setActive(true);
            // 等 demux 线程执行完 seek 再 unpause
            QTimer::singleShot(300, this, [this]() {
                if (!m_inRoom) return;
                if (m_controller->paused())
                    m_controller->togglePaused();
                qDebug() << "[MainWindow] after unpause, localTime=" << m_controller->getCurrentTime();
                updateRoomInfoBar();
                m_statusLabel->setText(QString("房间 %1 — 同步播放中").arg(m_roomSession->roomCode()));
            });
        }

        // 所有消息交给 SyncManager (seq 去重 + 执行)
        m_syncManager->handleServerMsg(msg);
    });

    // ---- 聊天 ----
    connect(m_roomSession, &RoomSession::chatMessageReceived, this, [this](const ChatMessage &msg) {
        m_danmakuOverlay->pushMessage(msg.sender, msg.text);
    });

    // 弱网 & 断线处理
    connect(m_chatRoomClient, &ChatRoomClient::connectionQuality, this, &MainWindow::updateConnectionIndicator);
    connect(m_chatRoomClient, &ChatRoomClient::reconnecting, this, [this](int attempt, int maxAttempts) {
        m_connStatusLabel->setText(QString("🟠 重连中 (%1/%2)").arg(attempt).arg(maxAttempts));
    });
    connect(m_roomSession, &RoomSession::reconnectTick, this, &MainWindow::onReconnectTick);
    connect(m_roomSession, &RoomSession::peerLeft, this, [this](const QString &) {
        m_connStatusLabel->setText("🟠 对方已断开");
        if (m_controller && !m_controller->paused()) m_controller->togglePaused();
    });

    // 倒计时定时器
    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(1000);

    // Set window properties - frameless with transparent background
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    // Setup UI
    setupUi();

    // Create stats timer
    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(1000); // 1s update
    connect(m_statsTimer, &QTimer::timeout, this, &MainWindow::updateStats);
    m_statsTimer->start();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi() {
    // Set window properties
    resize(1280, 720);
    setAcceptDrops(true);
    setWindowTitle("RinzePlayer");
    
    // Create central widget and layout
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    centralWidget->setStyleSheet(R"(
        #centralWidget {
            background-color: #f5f5f5;
            border-radius: 8px;
        }
    )");
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setCentralWidget(centralWidget);
    
    // 创建菜单栏
    setupMenuBar();
    mainLayout->addWidget(m_menuBar);

    // Room info bar (hidden by default)
    m_roomInfoBar = new QWidget;
    m_roomInfoBar->setFixedHeight(36);
    m_roomInfoBar->setStyleSheet("background: #2d6ff7; border-radius: 4px; margin: 2px 4px;");
    auto *roomBarLayout = new QHBoxLayout(m_roomInfoBar);
    roomBarLayout->setContentsMargins(12, 4, 12, 4);
    roomBarLayout->setSpacing(8);
    auto *roomIcon = new QLabel("📺");
    roomIcon->setStyleSheet("font-size: 14px; color: white; background: transparent;");
    roomBarLayout->addWidget(roomIcon);
    m_roomCodeLabel = new QLabel;
    m_roomCodeLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: white; background: transparent;");
    roomBarLayout->addWidget(m_roomCodeLabel, 1);
    m_copyRoomBtn = new QPushButton("复制房号");
    m_copyRoomBtn->setFixedHeight(26);
    m_copyRoomBtn->setStyleSheet("QPushButton { background: rgba(255,255,255,0.2); color: white; border: 1px solid rgba(255,255,255,0.3); "
                                 "border-radius: 3px; padding: 2px 12px; font-size: 11px; } "
                                 "QPushButton:hover { background: rgba(255,255,255,0.35); }");
    connect(m_copyRoomBtn, &QPushButton::clicked, this, &MainWindow::copyRoomCode);
    roomBarLayout->addWidget(m_copyRoomBtn);

    m_connStatusLabel = new QLabel;
    m_connStatusLabel->setStyleSheet("font-size: 12px; color: #00ff88; background: transparent; padding: 0 8px;");
    roomBarLayout->addWidget(m_connStatusLabel);

    m_roomInfoBar->setVisible(false);
    mainLayout->addWidget(m_roomInfoBar);

    // Setup custom title bar
    setupTitleBar();
    mainLayout->addWidget(m_titleBar);
    
    // Create video widget
    m_videoWidget = new VideoWidget(this);
    mainLayout->addWidget(m_videoWidget, 1);

    // Danmaku overlay on top of video
    m_danmakuOverlay = new DanmakuOverlay(m_videoWidget);
    m_danmakuOverlay->setGeometry(m_videoWidget->rect());
    m_danmakuOverlay->show();

    // Ready overlay (准备中 / 准备完成 + "我准备好了" 按钮)
    m_readyOverlay = new ReadyOverlay(centralWidget);
    m_readyOverlay->setGeometry(m_videoWidget->geometry());
    m_readyOverlay->raise();
    connect(m_readyOverlay, &ReadyOverlay::readyClicked, this, [this]() {
        m_roomSession->sendReady();
        m_iAmReady = true;
        m_statusLabel->setText("等待对方准备...");
    });
    m_videoWidget->installEventFilter(this);

    // Chat window (drawer, starts hidden)
    m_chatWindow = new ChatWindow(centralWidget);
    m_chatWindow->setAnchorWidget(m_videoWidget);
    m_chatWindow->setRoomSession(m_roomSession);
    connect(m_chatWindow, &ChatWindow::sendMessage, [this](const QString &text) {
        if (m_roomSession && m_controller) {
            double pos = m_controller->getCurrentTime();
            m_roomSession->sendChatMessage(text, pos);
            m_danmakuOverlay->pushMessage(m_roomSession->role(), text);
        }
    });

    // Create control bar (with only non-playback controls)
    m_controlBar = new ControlBar(this);
    mainLayout->addWidget(m_controlBar);

    // 创建音频可视化窗口 - 放在 controlbar 下方！
    m_visualWidget = new AudioVisualWidget(this);
    m_visualWidget->setMaximumHeight(180); // 高度适中，不占太多空间
    m_visualWidget->setMinimumHeight(120);
    // 设置 SizePolicy 确保宽度对齐！
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    policy.setHorizontalStretch(1);
    m_visualWidget->setSizePolicy(policy);
    mainLayout->addWidget(m_visualWidget);
    m_visualWidget->hide(); // 初始隐藏
    
    // Connect control bar signals (non-playback controls)
    connect(m_controlBar, &ControlBar::openFile, this, &MainWindow::openFileDialog);
    connect(m_controlBar, &ControlBar::openUrlWithConfig, this, [this](const QUrl &url, bool lowLatency, bool noBuffer, bool lowDelayFlag, bool discardCorrupt, bool noParse, bool shortProbe) {
        m_controller->setLowLatencyMode(lowLatency);
        m_controller->setLowLatencyOptions(noBuffer, lowDelayFlag, discardCorrupt, noParse, shortProbe);
        if (m_inRoom && m_roomSession->role() == "host") {
            openForRoom(url);
            m_roomSession->sendOpen(url.toString());  // Host 告知 Server URL
        } else {
            m_controller->open(url);
        }
    });
    
    // Connect video transform signals
    connect(m_controlBar, &ControlBar::rotateLeft, m_videoWidget, &VideoWidget::rotate90Left);
    connect(m_controlBar, &ControlBar::rotateRight, m_videoWidget, &VideoWidget::rotate90Right);
    connect(m_controlBar, &ControlBar::horizontalMirror, m_videoWidget, &VideoWidget::toggleHorizontalMirror);
    connect(m_controlBar, &ControlBar::verticalMirror, m_videoWidget, &VideoWidget::toggleVerticalMirror);
    connect(m_controlBar, &ControlBar::resetTransform, m_videoWidget, &VideoWidget::resetTransform);
    
    // Connect video filter signals
    connect(m_controlBar, &ControlBar::brightnessChanged, m_videoWidget, &VideoWidget::setBrightness);
    connect(m_controlBar, &ControlBar::contrastChanged, m_videoWidget, &VideoWidget::setContrast);
    connect(m_controlBar, &ControlBar::saturationChanged, m_videoWidget, &VideoWidget::setSaturation);
    connect(m_controlBar, &ControlBar::resetFilters, m_videoWidget, &VideoWidget::resetFilters);
    
    // Connect fullscreen
    connect(m_controlBar, &ControlBar::toggleFullscreen, this, &MainWindow::toggleFullscreen);

    connect(m_controlBar, &ControlBar::chatToggle, this, [this]() {
        if (m_chatWindow) m_chatWindow->toggle();
    });
    
    // Connect FPS display
    connect(m_controlBar, &ControlBar::showFPSChanged, m_videoWidget, &VideoWidget::setShowFPS);
    // Connect color space
    connect(m_controlBar, &ControlBar::colorSpaceChanged, m_videoWidget, &VideoWidget::setColorSpace);
    // Connect low latency mode
    connect(m_controlBar, &ControlBar::lowLatencyModeChanged, m_controller, &MediaController::setLowLatencyMode);
    
    // Connect video info
    connect(m_videoWidget, &VideoWidget::videoInfoChanged, m_controlBar, &ControlBar::updateVideoInfo);
    
    // Connect hover control bar signals (inside VideoWidget)
    if (m_videoWidget && m_videoWidget->hoverControlBar()) {
        connect(m_videoWidget->hoverControlBar(), &HoverControlBar::togglePause, m_controller, &MediaController::togglePaused);
        connect(m_videoWidget->hoverControlBar(), &HoverControlBar::seekTo, [this](int sec) {
            if (m_inRoom && m_roomSession && m_roomSession->role() == "guest") return;
            m_controller->seekBySec((double)sec, 0.0);
        });
        connect(m_videoWidget->hoverControlBar(), &HoverControlBar::volumeChanged, [this](int vol) { m_controller->setVolume(vol / 100.0); });
        connect(m_videoWidget->hoverControlBar(), &HoverControlBar::fastForward, [this]() {
            if (m_inRoom && m_roomSession && m_roomSession->role() == "guest") return;
            m_controller->fastForward();
        });
        connect(m_videoWidget->hoverControlBar(), &HoverControlBar::fastRewind, [this]() {
            if (m_inRoom && m_roomSession && m_roomSession->role() == "guest") return;
            m_controller->fastRewind();
        });
        connect(m_videoWidget->hoverControlBar(), &HoverControlBar::toggleFullscreen, this, &MainWindow::toggleFullscreen);
        
        // Connect controller signals to hover control bar
        connect(m_controller, &MediaController::durationChanged, m_videoWidget->hoverControlBar(), &HoverControlBar::setDuration);
        connect(m_controller, &MediaController::progressChanged, m_videoWidget->hoverControlBar(), &HoverControlBar::setCurrentTime);
        connect(m_controller, &MediaController::pausedChanged, m_videoWidget->hoverControlBar(), &HoverControlBar::setPaused);
    }
    
    // Connect when file opened to setup audio visualizer!
    connect(m_controller, &MediaController::openedChanged, [this](bool opened) {
        if (opened && !m_visualizerConnected && m_controller && m_controller->audioPlayer()) {
            bool ok = connect(m_controller->audioPlayer(), &AudioPlayer::audioDataReady,
                    m_visualWidget, &AudioVisualWidget::setAudioData);
            if (ok) {
                m_visualizerConnected = true;
                qDebug() << "Audio data signal connected successfully!";
            }
        }
    });
    
    // Set render callback and video widget
    m_controller->setVideoWidget(m_videoWidget);
    m_controller->setRenderCallback([this](VideoDoubleBuf* vid, SubtitleDoubleBuf* sub) {
        m_videoWidget->updateRenderData(vid, sub);
    });
    
    // Connect streaming signals
    connect(m_controller, &MediaController::streamStateChanged, this, &MainWindow::onStreamStateChanged);
    connect(m_controller, &MediaController::bufferProgress, this, &MainWindow::onBufferProgress);
    connect(m_controller, &MediaController::streamError, this, &MainWindow::onStreamError);
    
    // Setup status bar
    QStatusBar* bar = statusBar();
    bar->setStyleSheet(R"(
        QStatusBar {
            background-color: #e8e8e8;
            color: #333;
            border: none;
        }
    )");
    m_statusLabel = new QLabel("就绪", this);
    m_bufferLabel = new QLabel("缓冲: --% (-- MB)", this);
    m_statsLabel = new QLabel("播放时间: 0:00:00 | 码率: --", this);
    
    bar->addPermanentWidget(m_statsLabel);
    bar->addPermanentWidget(m_bufferLabel);
    bar->addWidget(m_statusLabel, 1);
}

void MainWindow::setupMenuBar() {
    m_menuBar = new QMenuBar(this);
    m_menuBar->setObjectName("menuBar");
    m_menuBar->setStyleSheet(R"(
        #menuBar {
            background-color: #e8e8e8;
            border: none;
            color: #333;
        }
        #menuBar::item {
            padding: 4px 12px;
        }
        #menuBar::item:selected {
            background-color: #d0d0d0;
        }
    )");
    QMenu* viewMenu = m_menuBar->addMenu("视图(&V)");
    
    QAction* toggleVisAction = viewMenu->addAction("音频可视化(&V)");
    toggleVisAction->setShortcut(QKeySequence("V"));
    connect(toggleVisAction, &QAction::triggered, this, &MainWindow::toggleVisualizer);
    
    QAction* nextModeAction = viewMenu->addAction("下一个可视化模式(&N)");
    nextModeAction->setShortcut(QKeySequence("N"));
    connect(nextModeAction, &QAction::triggered, this, &MainWindow::nextVisualizerMode);
    
    // 添加运动向量可视化菜单项
    viewMenu->addSeparator();
    m_motionVectorsAction = viewMenu->addAction("运动向量可视化(&M)");
    m_motionVectorsAction->setShortcut(QKeySequence("M"));
    m_motionVectorsAction->setCheckable(true);
    connect(m_motionVectorsAction, &QAction::triggered, [this](bool checked) {
        if (m_videoWidget) {
            m_videoWidget->setShowMotionVectors(checked);
        }
    });
    
    // 添加运动能量热力图菜单项
    m_motionHeatmapAction = viewMenu->addAction("运动能量热力图(&H)");
    m_motionHeatmapAction->setShortcut(QKeySequence("H"));
    connect(m_motionHeatmapAction, &QAction::triggered, [this]() {
        if (m_videoWidget) {
            m_videoWidget->toggleMotionHeatmap();
        }
    });
    
    // 添加运动轨迹图菜单项
    m_motionTrailAction = viewMenu->addAction("运动轨迹图(&T)");
    m_motionTrailAction->setShortcut(QKeySequence("T"));
    connect(m_motionTrailAction, &QAction::triggered, [this]() {
        if (m_videoWidget) {
            m_videoWidget->toggleMotionTrail();
        }
    });
    
    // 添加运动历史图像菜单项
    m_motionHistoryAction = viewMenu->addAction("运动历史图像 (&I)");
    m_motionHistoryAction->setShortcut(QKeySequence("I"));
    connect(m_motionHistoryAction, &QAction::triggered, [this]() {
        if (m_videoWidget) {
            m_videoWidget->toggleMotionHistory();
        }
    });

    // 添加报告生成菜单项
    viewMenu->addSeparator();
    m_generateReportAction = viewMenu->addAction("生成视频分析报告 (&R)");
    m_generateReportAction->setShortcut(QKeySequence("R"));
    connect(m_generateReportAction, &QAction::triggered, [this]() {
        QString defaultPath;
        if (m_controller && m_controller->opened()) {
            defaultPath = m_controller->currentUrl().toLocalFile();
        }
        
        // 每次创建新的对话框
        ReportDialog *dialog = new ReportDialog(this, defaultPath);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        
        connect(dialog, &ReportDialog::startExport, this, [this, dialog](const QString& videoPath, const QString& outputPath) {
            // 创建新的生成器和线程
            ReportGenerator *generator = new ReportGenerator();
            QThread *thread = new QThread();
            generator->moveToThread(thread);

            connect(thread, &QThread::started, generator, &ReportGenerator::process);
            connect(generator, &ReportGenerator::progressUpdated, dialog, &ReportDialog::setProgress);
            connect(generator, &ReportGenerator::finished, this, [this, dialog, generator, thread](bool success, const QString& message) {
                // 确保UI重新启用
                dialog->setExporting(false);
                dialog->setProgress(100);
                QMessageBox::information(this, success ? "成功" : "失败", message);
                dialog->accept();
            });
            connect(generator, &ReportGenerator::finished, thread, &QThread::quit);
            connect(thread, &QThread::finished, generator, &QObject::deleteLater);
            connect(thread, &QThread::finished, thread, &QObject::deleteLater);
            
            // 设置并启动
            generator->setSettings(videoPath, outputPath);
            thread->start();
        });
        
        dialog->show();
        dialog->raise();
    });

    // 添加音频效果器菜单项
    viewMenu->addSeparator();
    QAction* toggleEffectsAction = viewMenu->addAction("音频效果器(&F)");
    toggleEffectsAction->setShortcut(QKeySequence("F"));
    connect(toggleEffectsAction, &QAction::triggered, [this]() {
        if (!m_effectsPanel && m_controller && m_controller->audioPlayer()) {
            // 创建效果器面板作为一个停靠窗口
            QDockWidget *dock = new QDockWidget("Audio Effects", this);
            dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
            m_effectsPanel = new EffectsPanel(m_controller->audioPlayer()->mixer(), dock);
            dock->setWidget(m_effectsPanel);
            addDockWidget(Qt::RightDockWidgetArea, dock);
        } else if (m_effectsPanel) {
            // 如果已存在，切换显示/隐藏
            QDockWidget *dock = qobject_cast<QDockWidget*>(m_effectsPanel->parent());
            if (dock) {
                if (dock->isVisible()) {
                    dock->hide();
                } else {
                    dock->show();
                    dock->raise();
                }
            }
        }
    });

    // 双人观影菜单
    viewMenu->addSeparator();
    QMenu *syncMenu = viewMenu->addMenu("双人观影");

    m_createRoomAction = syncMenu->addAction("创建房间");
    connect(m_createRoomAction, &QAction::triggered, this, &MainWindow::onCreateRoom);

    m_joinRoomAction = syncMenu->addAction("加入房间");
    connect(m_joinRoomAction, &QAction::triggered, this, &MainWindow::onJoinRoom);

    m_leaveRoomAction = syncMenu->addAction("离开房间");
    m_leaveRoomAction->setEnabled(false);
    connect(m_leaveRoomAction, &QAction::triggered, this, &MainWindow::onLeaveRoom);
}

void MainWindow::toggleVisualizer()
{
    if (m_visualWidget->isVisible()) {
        m_visualWidget->hide();
    } else {
        m_visualWidget->show();
        // 连接音频数据（如果还没连接）
        if (!m_visualizerConnected && m_controller && m_controller->audioPlayer()) {
            bool ok = connect(m_controller->audioPlayer(), &AudioPlayer::audioDataReady,
                    m_visualWidget, &AudioVisualWidget::setAudioData);
            if (ok) {
                m_visualizerConnected = true;
                qDebug() << "Audio data signal connected to visualizer!";
            } else {
                qDebug() << "Failed to connect audio data signal!";
            }
        }
    }
}

void MainWindow::nextVisualizerMode() {
    if (!m_visualWidget) return;
    int mode = (int)m_visualWidget->visualizationMode();
    mode = (mode + 1) % 4; // 现在有4个模式！
    m_visualWidget->setVisualizationMode((AudioVisualWidget::VisualizationMode)mode);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls() || event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        for (const QUrl& url : mimeData->urls()) {
            if (m_inRoom && m_roomSession && m_roomSession->role() == "host") {
                openForRoom(url);
                m_roomSession->sendOpen(url.toString());
            } else {
                m_controller->open(url);
            }
        }
    } else if (mimeData->hasText()) {
        QString urlText = mimeData->text().trimmed();
        if (!urlText.isEmpty()) {
            QUrl url(urlText);
            if (m_inRoom && m_roomSession && m_roomSession->role() == "host") {
                openForRoom(url);
                m_roomSession->sendOpen(url.toString());
            } else {
                m_controller->open(url);
            }
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        if (m_isFullscreen) {
            toggleFullscreen();
        }
    } else if (event->key() == Qt::Key_F) {
        toggleFullscreen();
    } else if (event->key() == Qt::Key_V) {
        toggleVisualizer();
    } else if (event->key() == Qt::Key_N) {
        nextVisualizerMode();
    } else if (event->key() == Qt::Key_M) {
        // M键 - 切换运动向量可视化
        if (m_videoWidget) {
            bool newState = !m_videoWidget->showMotionVectors();
            m_videoWidget->setShowMotionVectors(newState);
            // 更新菜单的选中状态
            if (m_motionVectorsAction) {
                m_motionVectorsAction->setChecked(newState);
            }
        }
    } else if (event->key() == Qt::Key_Period) {
        if (m_inRoom && m_roomSession && m_roomSession->role() == "guest") return;
        m_controller->nextFrame();
    } else if (event->key() == Qt::Key_Comma) {
        if (m_inRoom && m_roomSession && m_roomSession->role() == "guest") return;
        m_controller->prevFrame();
    } else if (event->key() == Qt::Key_P) {
        // P键 - 画中画
        m_controller->togglePictureInPicture();
        event->accept();
    } else if (event->key() == Qt::Key_QuoteLeft && m_inRoom) {
        // 反引号 - 聊天抽屉
        if (m_chatWindow) {
            m_chatWindow->toggle();
        }
        event->accept();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::openFileDialog() {
    QString filePath = QFileDialog::getOpenFileName(this, "打开视频文件", "",
        "视频文件 (*.mp4 *.mkv *.avi *.mov *.flv *.webm);;所有文件 (*)");
    if (!filePath.isEmpty()) {
        onOpenFile(filePath);
    }
}

void MainWindow::onOpenFile(const QString &filePath) {
    m_videoWidget->resetVideoInfoSent();
    QUrl url = QUrl::fromLocalFile(filePath);
    if (m_inRoom && m_roomSession && m_roomSession->role() == "host") {
        openForRoom(url);
        m_roomSession->sendOpen(url.toString());
    } else {
        m_controller->open(url);
    }
    m_playbackStartSeconds = 0;
}

void MainWindow::updateDuration(int duration) {
    if (m_controlBar) {
        m_controlBar->setDuration(duration);
    }
}

void MainWindow::updateProgress(int progress) {
    if (m_controlBar) {
        m_controlBar->setCurrentTime(progress);
    }
}

void MainWindow::updatePaused(bool paused) {
    if (m_controlBar) {
        m_controlBar->setPaused(paused);
    }
}

void MainWindow::toggleFullscreen() {
    bool wasVisible = m_visualWidget && m_visualWidget->isVisible();
    if (m_isFullscreen) {
        // Exit fullscreen
        showNormal();
        statusBar()->show();
        if (wasVisible && m_visualWidget) {
            m_visualWidget->show();
        }
        m_isFullscreen = false;
    } else {
        // Enter fullscreen
        showFullScreen();
        statusBar()->show();
        if (m_visualWidget) {
            m_visualWidget->hide();
        }
        m_isFullscreen = true;
    }
}

void MainWindow::updateStats() {
    static int elapsedSeconds = 0;

    // Increment elapsed only if not paused
    if (m_controller && !m_controller->paused() && m_controller->duration() > 0) {
        elapsedSeconds++;
    } else if (m_controller && m_controller->duration() <= 0) {
        elapsedSeconds = 0; // Reset if no file playing
    }

    QString bitrateText = "--";
    if (m_controller) {
        double bitrate = m_controller->getBitrate();
        if (bitrate > 0) {
            bitrateText = QString("%1 kbps").arg(bitrate, 0, 'f', 1);
        }
        // 更新Buffer信息、同步信息和内存信息
        if (m_videoWidget) {
            m_videoWidget->updateBufferInfo(
                m_controller->getVideoPacketQueueSize(),
                m_controller->getVideoFrameQueueSize(),
                m_controller->getAudioPacketQueueSize(),
                m_controller->getAudioFrameQueueSize()
            );
            m_videoWidget->updateSyncInfo(m_controller->getSyncDifference());
            
            // 获取内存信息
            PROCESS_MEMORY_COUNTERS_EX pmc;
            GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
            SIZE_T virtualMemUsedByMe = pmc.WorkingSetSize;
            double memoryMB = (double)virtualMemUsedByMe / (1024 * 1024);
            m_videoWidget->updateMemoryInfo(memoryMB);
        }
    }

    m_statsLabel->setText(QString("播放时间: %1 | 码率: %2").arg(formatTimeForDisplay(elapsedSeconds), bitrateText));
}

QString MainWindow::formatTimeForDisplay(int seconds) {
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

QString MainWindow::streamStateToString(StreamState state) {
    switch (state) {
        case StreamState::Idle: return "就绪";
        case StreamState::Connecting: return "连接中...";
        case StreamState::Connected: return "已连接";
        case StreamState::Buffering: return "缓冲中...";
        case StreamState::Playing: return "播放中";
        case StreamState::Paused: return "已暂停";
        case StreamState::Reconnecting: return "重连中...";
        case StreamState::Error: return "错误";
        case StreamState::Disconnected: return "已断开";
        default: return "未知";
    }
}

void MainWindow::onStreamStateChanged(StreamState oldState, StreamState newState) {
    m_statusLabel->setText(QString("状态: %1").arg(streamStateToString(newState)));
}

void MainWindow::onBufferProgress(double percent, double bufferMB) {
    m_bufferLabel->setText(QString("缓冲: %1% (%2 MB)").arg((int)percent).arg(bufferMB, 0, 'f', 2));
}

void MainWindow::onOpenFailed(const QString& error) {
    // 显示错误对话框
    QMessageBox::warning(this, "打开失败", error);
}

void MainWindow::onStreamError(const QString& error) {
    m_statusLabel->setText(QString("错误: %1").arg(error));
}

void MainWindow::setupTitleBar() {
    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName("titleBar");
    m_titleBar->setFixedHeight(32);
    m_titleBar->setStyleSheet(R"(
        #titleBar {
            background-color: #e8e8e8;
            color: #333;
        }
    )");

    QHBoxLayout* titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(8, 0, 8, 0);
    titleLayout->setSpacing(8);

    // Title label
    m_titleLabel = new QLabel("RinzePlayer", this);
    m_titleLabel->setStyleSheet("color: #333;");
    titleLayout->addWidget(m_titleLabel);

    titleLayout->addStretch();

    // Window control buttons
    m_minimizeBtn = new QPushButton("─", this);
    m_minimizeBtn->setFixedSize(30, 22);
    m_minimizeBtn->setFocusPolicy(Qt::NoFocus);
    m_minimizeBtn->setStyleSheet("QPushButton { color: #333; background: transparent; border: none; } QPushButton:hover { background-color: #d0d0d0; }");
    connect(m_minimizeBtn, &QPushButton::clicked, this, &MainWindow::minimizeWindow);
    titleLayout->addWidget(m_minimizeBtn);

    m_maximizeBtn = new QPushButton("□", this);
    m_maximizeBtn->setFixedSize(30, 22);
    m_maximizeBtn->setFocusPolicy(Qt::NoFocus);
    m_maximizeBtn->setStyleSheet("QPushButton { color: #333; background: transparent; border: none; } QPushButton:hover { background-color: #d0d0d0; }");
    connect(m_maximizeBtn, &QPushButton::clicked, this, &MainWindow::maximizeRestoreWindow);
    titleLayout->addWidget(m_maximizeBtn);

    m_closeBtn = new QPushButton("✕", this);
    m_closeBtn->setFixedSize(30, 22);
    m_closeBtn->setFocusPolicy(Qt::NoFocus);
    m_closeBtn->setStyleSheet("QPushButton { color: #333; background: transparent; border: none; } QPushButton:hover { background-color: #ff5f57; color: white; }");
    connect(m_closeBtn, &QPushButton::clicked, this, &MainWindow::closeWindow);
    titleLayout->addWidget(m_closeBtn);
}

void MainWindow::minimizeWindow() {
    showMinimized();
}

void MainWindow::maximizeRestoreWindow() {
    if (isMaximized()) {
        showNormal();
        m_maximizeBtn->setText("□");
    } else {
        showMaximized();
        m_maximizeBtn->setText("▢");
    }
}

void MainWindow::closeWindow() {
    close();
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_titleBar) {
        if (m_titleBar->geometry().contains(event->pos())) {
            m_isDragging = true;
            m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDragging && event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
    QMainWindow::mouseMoveEvent(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_videoWidget && event->type() == QEvent::Resize) {
        if (m_danmakuOverlay) m_danmakuOverlay->setGeometry(m_videoWidget->rect());
        if (m_readyOverlay) m_readyOverlay->setGeometry(m_videoWidget->geometry());
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    m_isDragging = false;
    QMainWindow::mouseReleaseEvent(event);
}

// --- 双人观影 ---

void MainWindow::onCreateRoom() {
    RoomDialog dlg(RoomDialog::Create, this);
    if (dlg.exec() != QDialog::Accepted) return;

    QString host = dlg.serverHost();
    quint16 port = dlg.serverPort();
    QString userName = dlg.userName();
    QString password = dlg.password();

    m_chatRoomClient->connectToServer(host, port);

    // Wait for connected, then create room
    connect(m_chatRoomClient, &ChatRoomClient::connected, this, [this, userName, password]() {
        m_roomSession->createRoom(userName, password);
    }, Qt::SingleShotConnection);
}

void MainWindow::onJoinRoom() {
    RoomDialog dlg(RoomDialog::Join, this);
    if (dlg.exec() != QDialog::Accepted) return;

    QString host = dlg.serverHost();
    quint16 port = dlg.serverPort();
    QString code = dlg.roomCode();
    QString userName = dlg.userName();
    QString password = dlg.password();

    m_chatRoomClient->connectToServer(host, port);

    connect(m_chatRoomClient, &ChatRoomClient::connected, this, [this, code, userName, password]() {
        m_roomSession->joinRoom(code, userName, password);
    }, Qt::SingleShotConnection);
}

void MainWindow::onLeaveRoom() {
    if (m_roomSession) m_roomSession->leaveRoom();
    if (m_chatRoomClient) m_chatRoomClient->disconnectFromServer();
    onRoomClosed("left");
}

void MainWindow::onRoomClosed(const QString &reason) {
    Q_UNUSED(reason)
    m_inRoom = false;
    m_iAmReady = false;
    if (m_readyOverlay) m_readyOverlay->setState(ReadyOverlay::Hidden);
    m_currentRoomUrl.clear();
    m_syncManager->setActive(false);
    m_createRoomAction->setEnabled(true);
    m_joinRoomAction->setEnabled(true);
    m_leaveRoomAction->setEnabled(false);
    m_controlBar->setRoomMode(false);
    if (m_chatWindow) m_chatWindow->slideOut();
    m_statusLabel->setText("就绪");
}

void MainWindow::onRoomExportFinished(const QString &filePath) {
    m_statusLabel->setText(QString("聊天记录已保存: %1").arg(QFileInfo(filePath).fileName()));
}

void MainWindow::openForRoom(const QUrl &url) {
    m_controller->open(url);  // open() 内房间模式不自动播放, 已停住
    auto onLoaded = [this]() {
        if (!m_inRoom) return;
        if (m_readyOverlay) m_readyOverlay->setState(ReadyOverlay::Waiting);
        m_statusLabel->setText("点击 [我准备好了] 按钮...");
    };
    if (m_controller->opened()) onLoaded();
    else connect(m_controller, &MediaController::openedChanged, this,
                 [this, onLoaded](bool opened) { if (opened) onLoaded(); },
                 Qt::SingleShotConnection);
}

void MainWindow::updateRoomInfoBar() {
    if (!m_roomInfoBar || !m_roomSession) return;
    QString code = m_roomSession->roomCode();
    QString status = m_syncManager->isSyncing() ? "同步中" : "";
    m_roomCodeLabel->setText(QString("房间号: %1 %2").arg(code, status));
    m_connStatusLabel->setText("🟢 已连接");
    m_roomInfoBar->setVisible(true);

    QString role = m_roomSession->role();
    if (role == "host") {
        m_roomInfoBar->setStyleSheet("background: #2d6ff7; border-radius: 4px; margin: 2px 4px;");
    } else {
        m_roomInfoBar->setStyleSheet("background: #6b3fa0; border-radius: 4px; margin: 2px 4px;");
    }
}

void MainWindow::copyRoomCode() {
    if (!m_roomSession) return;
    QApplication::clipboard()->setText(m_roomSession->roomCode());
    m_statusLabel->setText("房号已复制到剪贴板");
}

void MainWindow::updateConnectionIndicator(int level, int pingMs) {
    if (!m_connStatusLabel || !m_inRoom) return;
    switch (level) {
    case 0: // good
        m_connStatusLabel->setText(pingMs > 0 ? QString("🟢 已连接 (%1ms)").arg(pingMs) : "🟢 已连接");
        m_connStatusLabel->setStyleSheet("font-size: 12px; color: #00ff88; background: transparent; padding: 0 8px;");
        break;
    case 1: // weak
        m_connStatusLabel->setText(QString("🟡 网络慢 (%1ms)").arg(pingMs));
        m_connStatusLabel->setStyleSheet("font-size: 12px; color: #ffcc00; background: transparent; padding: 0 8px;");
        break;
    case 2: // bad / disconnected
        m_connStatusLabel->setText("🔴 已断开");
        m_connStatusLabel->setStyleSheet("font-size: 12px; color: #ff4444; background: transparent; padding: 0 8px;");
        break;
    }
}

void MainWindow::onReconnectTick(int remaining) {
    m_countdownRemaining = remaining;
    m_roomCodeLabel->setText(QString("房间号: %1 — 等待对方重连 %2s")
        .arg(m_roomSession->roomCode()).arg(remaining));
}

