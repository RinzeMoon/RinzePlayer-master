#include "../Header/Ui/VideoPlayerMW.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QApplication>
#include <QFile>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QToolButton>
#include <QWindow>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QSysInfo>
#include <QDateTime>
#include <QLabel>

VideoPlayerMW::VideoPlayerMW(QWidget *parent)
    : QMainWindow(parent)
{
    // 检测是否为 WSL2 环境
    #ifdef Q_OS_LINUX
    if (QFile::exists("/proc/version")) {
        QFile versionFile("/proc/version");
        if (versionFile.open(QIODevice::ReadOnly)) {
            QByteArray content = versionFile.readAll();
            qDebug() << "/proc/version 内容:" << content;
            if (content.contains("WSL2") || content.contains("Microsoft")) {
                m_isWSL2 = true;
                qDebug() << "检测到 WSL2 环境，启用特殊处理";
            } else {
                qDebug() << "未检测到 WSL2 环境";
            }
            versionFile.close();
        }
    }
    #endif

    // 设置无边框窗口
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    // 启用鼠标跟踪
    setMouseTracking(true);

    // WSL2 特殊处理
    if (m_isWSL2) {
        // 设置窗口最小尺寸
        setMinimumSize(800, 600);
        // 设置窗口属性，帮助 WSL2 处理
        setAttribute(Qt::WA_TranslucentBackground, false);
    }

    m_resMgr = new ResMgr();
    m_playerFrame = new BasePlayerFrame();

    setupUi();
    createMenu();
    createConnections();

    m_resMgr->setPlayerFrame(m_playerFrame);

}

VideoPlayerMW::~VideoPlayerMW()
{
    qDebug() << "VideoPlayerMW 析构";
}


void VideoPlayerMW::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 使用 QWindow 的系统移动方法
        if (QWindow *window = this->windowHandle()) {
            window->startSystemMove();
            return; // 直接返回，让系统处理移动
        }
    }

    QMainWindow::mousePressEvent(event);
}


void VideoPlayerMW::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isMoving) {
        // 使用 globalPosition() 获取全局位置
        QPoint delta = event->globalPosition().toPoint() - m_dragStartPosition;
        QPoint newWindowPosition = m_windowStartPosition + delta;

        // 边界检查
        if (m_enableBoundaryCheck) {
            // 获取屏幕可用区域
            QRect screenArea = QGuiApplication::primaryScreen()->availableGeometry();

            // 边界限制 - 确保窗口不会完全移出屏幕
            if (newWindowPosition.x() < screenArea.left()) {
                newWindowPosition.setX(screenArea.left());
            } else if (newWindowPosition.x() + width() > screenArea.right()) {
                newWindowPosition.setX(screenArea.right() - width());
            }

            if (newWindowPosition.y() < screenArea.top()) {
                newWindowPosition.setY(screenArea.top());
            } else if (newWindowPosition.y() + height() > screenArea.bottom()) {
                newWindowPosition.setY(screenArea.bottom() - height());
            }
        }

        // 移动窗口
        this->move(newWindowPosition);

    }

    QMainWindow::mouseMoveEvent(event);
}

void VideoPlayerMW::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isMoving) {
        m_isMoving = false;

        // 在 WSL2 中不要使用 releaseMouse()
        this->setMouseTracking(false);
    }

    QMainWindow::mouseReleaseEvent(event);
}

// 新增：鼠标移出窗口时强制复位拖动状态，解决鬼畜跟随问题
void VideoPlayerMW::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    if(m_isMoving) {
        m_isMoving = false;
    }
}

bool VideoPlayerMW::eventFilter(QObject *obj, QEvent *event)
{
    // 只处理centralWidget的事件
    if (obj == this->centralWidget())
    {
        // 确保有标题栏和按钮
        if (!titleBar || !minimizeBtn || !maximizeBtn || !closeBtn) {
            return QMainWindow::eventFilter(obj, event);
        }

        // 检查事件类型
        QEvent::Type type = event->type();

        // 处理鼠标按下事件
        if (type == QEvent::MouseButtonPress)
        {
            QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event);
            if (mouseEvent && mouseEvent->button() == Qt::LeftButton) {
                QPoint localPos = mouseEvent->pos();
                QRect titleBarRect = titleBar->geometry();

                // 检查是否在标题栏区域内
                if (titleBarRect.contains(localPos)) {
                    // 检查是否在按钮区域内
                    bool isButtonArea = false;

                    // 检查是否在按钮上
                    if (minimizeBtn->geometry().contains(localPos) ||
                        maximizeBtn->geometry().contains(localPos) ||
                        closeBtn->geometry().contains(localPos)) {
                        isButtonArea = true;
                    }

                    // 如果不是按钮区域，处理拖动
                    if (!isButtonArea) {
                        this->mousePressEvent(mouseEvent);
                        return true;
                    }
                }
            }
        }
        // 处理鼠标移动事件
        else if (type == QEvent::MouseMove)
        {
            QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event);
            if (mouseEvent) {
                QPoint localPos = mouseEvent->pos();
                QRect titleBarRect = titleBar->geometry();

                if (titleBarRect.contains(localPos)) {
                    // 检查是否在按钮上
                    bool isButtonArea = false;

                    if (minimizeBtn->geometry().contains(localPos) ||
                        maximizeBtn->geometry().contains(localPos) ||
                        closeBtn->geometry().contains(localPos)) {
                        isButtonArea = true;
                    }

                    // 如果不是按钮区域，处理拖动
                    if (!isButtonArea) {
                        this->mouseMoveEvent(mouseEvent);
                        return true;
                    }
                }
            }
        }
        // 处理鼠标释放事件
        else if (type == QEvent::MouseButtonRelease)
        {
            QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event);
            if (mouseEvent && mouseEvent->button() == Qt::LeftButton) {
                this->mouseReleaseEvent(mouseEvent);
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void VideoPlayerMW::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        qDebug() << "窗口状态变化事件";
        updateWindowState();
    }
    QMainWindow::changeEvent(event);
}

void VideoPlayerMW::resizeEvent(QResizeEvent *event)
{
    // 更新窗口状态
    updateWindowState();
    QMainWindow::resizeEvent(event);
}

void VideoPlayerMW::updateWindowState()
{
    qDebug() << "更新窗口状态";

    // 检测当前窗口状态
    bool wasMaximized = m_isMaximized;
    m_isMaximized = this->isMaximized();
    m_isFullscreen = this->isFullScreen();

    // 如果状态变化，更新按钮
    if (wasMaximized != m_isMaximized && maximizeBtn) {
        maximizeBtn->setText(m_isMaximized ? "🗗" : "□");
    }

    // 如果是全屏状态
    if (m_isFullscreen) {
        if (titleBar) {
            titleBar->hide();
        }
    } else if (titleBar) {
        titleBar->show();
    }
}

void VideoPlayerMW::setupUi()
{
    // 设置窗口属性
    setWindowTitle("RinzePlayer 视频播放器");
    resize(1280, 720);

    // 设置主窗口样式 - 专业播放器 哑光深灰主色调 核心修改 彻底去白
    setStyleSheet(R"(
        QMainWindow {
            background: #333639; /* 哑光深空灰 主背景 播放器标配色 */
            border: 1px solid #44484C; /* 深灰细腻边框 不突兀 */
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.2); /* 暗调阴影 更有质感 */
        }
    )");

    // 创建自定义标题栏
    titleBar = new QWidget();
    titleBar->setObjectName("titleBar");
    titleBar->setFixedHeight(40);
    titleBar->setStyleSheet(R"(
        QWidget {
            background: #2D2F32; /* 标题栏深一级灰 强层次感 比主背景暗 */
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            border-bottom: 1px solid #44484C; /* 统一深灰边框 */
        }
    )");
    titleBar->setMouseTracking(true);

    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(15, 0, 15, 0);
    titleLayout->setSpacing(10);

    // 应用图标 - 暗调适配柔蓝 不刺眼 点睛色
    QLabel *appIcon = new QLabel("🎬");
    appIcon->setObjectName("appIcon");
    appIcon->setStyleSheet("font-size: 16px; color: #50A8FF;"); // 暗调柔和浅蓝

    // 应用标题 - 深灰背景适配 浅灰文字 无纯白 护眼清晰
    QLabel *appTitle = new QLabel("RinzePlayer 视频播放器");
    appTitle->setObjectName("appTitle");
    appTitle->setStyleSheet(R"(
        QLabel {
            color: #E2E2E5; /* 哑光浅灰文字 非纯白 核心修改 */
            font-size: 14px;
            font-weight: bold;
        }
    )");

    // 窗口控制按钮 - 全套深灰哑光适配 悬浮反馈细腻
    QHBoxLayout *windowButtonsLayout = new QHBoxLayout();
    windowButtonsLayout->setSpacing(5);

    minimizeBtn = new QToolButton();
    minimizeBtn->setObjectName("minimizeBtn");
    minimizeBtn->setText("—");
    minimizeBtn->setFixedSize(28, 28);
    minimizeBtn->setStyleSheet(R"(
        QToolButton {
            background: transparent;
            border: 1px solid #44484C; /* 深灰边框 暗调适配 */
            border-radius: 4px;
            color: #B0B4BD; /* 浅灰文字 可见度高 */
            font-size: 12px;
        }
        QToolButton:hover {
            background: #44484C; /* 悬浮深灰填充 细腻反馈 */
            color: #FFFFFF;
            border-color: #55595F;
        }
    )");
    minimizeBtn->setCursor(Qt::PointingHandCursor);
    minimizeBtn->setFocusPolicy(Qt::NoFocus);

    maximizeBtn = new QToolButton();
    maximizeBtn->setObjectName("maximizeBtn");
    maximizeBtn->setText("□");
    maximizeBtn->setFixedSize(28, 28);
    maximizeBtn->setStyleSheet(R"(
        QToolButton {
            background: transparent;
            border: 1px solid #44484C;
            border-radius: 4px;
            color: #B0B4BD;
            font-size: 12px;
        }
        QToolButton:hover {
            background: #44484C;
            color: #FFFFFF;
            border-color: #55595F;
        }
    )");
    maximizeBtn->setCursor(Qt::PointingHandCursor);
    maximizeBtn->setFocusPolicy(Qt::NoFocus);

    closeBtn = new QToolButton();
    closeBtn->setObjectName("closeBtn");
    closeBtn->setText("×");
    closeBtn->setFixedSize(28, 28);
    closeBtn->setStyleSheet(R"(
        QToolButton {
            background: transparent;
            border: 1px solid #44484C;
            border-radius: 4px;
            color: #B0B4BD;
            font-size: 16px;
        }
        QToolButton:hover {
            background: #E04E59; /* 哑光暗红 不刺眼 播放器标配关闭色 */
            border-color: #E04E59;
            color: white;
        }
    )");
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFocusPolicy(Qt::NoFocus);

    windowButtonsLayout->addWidget(minimizeBtn);
    windowButtonsLayout->addWidget(maximizeBtn);
    windowButtonsLayout->addWidget(closeBtn);

    titleLayout->addWidget(appIcon);
    titleLayout->addWidget(appTitle);
    titleLayout->addStretch();
    titleLayout->addLayout(windowButtonsLayout);

    // 主内容区域 - 与主窗口同色 哑光深灰 视觉统一
    QWidget *central = new QWidget();
    central->setObjectName("centralWidget");
    central->setStyleSheet("background: #333639; border-radius: 8px;");
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    mainLayout->addWidget(titleBar);

    // 创建分割器 - 深灰哑光分割线 适配整体风格
    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setObjectName("mainSplitter");
    m_splitter->setHandleWidth(2);
    m_splitter->setStyleSheet(R"(
        QSplitter::handle {
            background: #44484C; /* 深灰分割线 清晰不突兀 */
        }
        QSplitter::handle:hover {
            background: #55595F; /* 悬浮加深 反馈明显 */
        }
    )");

    // 左侧播放器框架
    m_playerFrame = new BasePlayerFrame();
    m_playerFrame->setObjectName("playerFrame");

    // 右侧播放列表
    m_playlistWidget = new VideoPlaylistWidget();
    m_playlistWidget->setObjectName("playlistWidget");
    m_playlistWidget->setMinimumWidth(350);
    m_playlistWidget->setMaximumWidth(400);

    // 添加到分割器
    m_splitter->addWidget(m_playerFrame);
    m_splitter->addWidget(m_playlistWidget);

    // 设置分割比例
    m_splitter->setSizes({850, 350});

    mainLayout->addWidget(m_splitter, 1);

    setCentralWidget(central);

    // 创建控制器并设置给播放器框架
    m_controller = new StandardController(this);
    m_playerFrame->setController(m_controller);

    // 连接窗口控制按钮
    connect(minimizeBtn, &QToolButton::clicked, this, &VideoPlayerMW::showMinimized);
    connect(closeBtn, &QToolButton::clicked, this, &VideoPlayerMW::close);

    connect(maximizeBtn,&QToolButton::clicked, this,&VideoPlayerMW::onMaximizeButtonClicked);

    // 安装事件过滤器（用于双击最大化）
    installEventFilter(this);

    // 美化状态栏 - 标题栏同色 深灰哑光 视觉统一
    statusBar()->setStyleSheet(R"(
        QStatusBar {
            background: #2D2F32; /* 和标题栏同色 统一视觉 */
            color: #B0B4BD; /* 浅灰文字 清晰可见 */
            border-top: 1px solid #44484C;
        }
    )");
    statusBar()->showMessage("就绪", 3000);

    this->centralWidget()->installEventFilter(this);
    this->centralWidget()->setMouseTracking(true);
}

void VideoPlayerMW::createMenu()
{
    QMenu *fileMenu = menuBar()->addMenu(" 文件(&F)");

    QAction *openFileAction = new QAction(" 打开文件...", this);
    openFileAction->setShortcut(QKeySequence::Open);
    fileMenu->addAction(openFileAction);

    QAction *openUrlAction = new QAction(" 打开URL...", this);
    openUrlAction->setShortcut(Qt::CTRL | Qt::Key_U);
    fileMenu->addAction(openUrlAction);

    fileMenu->addSeparator();

    QAction *exitAction = new QAction(" 退出", this);
    exitAction->setShortcut(QKeySequence::Quit);
    fileMenu->addAction(exitAction);

    // 播放菜单
    QMenu *playMenu = menuBar()->addMenu("▶ 播放(&P)");

    QAction *playAction = new QAction("▶ 播放/暂停", this);
    playAction->setShortcut(Qt::Key_Space);
    playMenu->addAction(playAction);

    QAction *stopAction = new QAction("⏹ 停止", this);
    stopAction->setShortcut(Qt::Key_Escape);
    playMenu->addAction(stopAction);

    playMenu->addSeparator();

    QAction *prevAction = new QAction("⏮ 上一曲", this);
    prevAction->setShortcut(Qt::CTRL | Qt::Key_Left);
    playMenu->addAction(prevAction);

    QAction *nextAction = new QAction("⏭ 下一曲", this);
    nextAction->setShortcut(Qt::CTRL | Qt::Key_Right);
    playMenu->addAction(nextAction);

    // 视图菜单
    QMenu *viewMenu = menuBar()->addMenu("视图(&V)");

    QAction *fullscreenAction = new QAction("全屏", this);
    fullscreenAction->setShortcut(QKeySequence::FullScreen);
    viewMenu->addAction(fullscreenAction);

    QAction *showPlaylistAction = new QAction("显示播放列表", this);
    showPlaylistAction->setCheckable(true);
    showPlaylistAction->setChecked(true);
    showPlaylistAction->setShortcut(Qt::CTRL | Qt::Key_P);
    viewMenu->addAction(showPlaylistAction);

    // ========== 核心修改：适配平衡的菜单样式表 ==========
    QString menuStyle = R"(
        /* 弹出菜单主体样式 - 温润哑光浅灰 与主界面区分开 核心配色 */
        QMenu {
            background-color: #F5F6F7;
            border: 1px solid #D1D5DB;
            color: #27272A;
            padding: 6px 0px;
            border-radius: 6px;
            font-size: 12px;
        }
        /* 菜单项基础样式 - 加大点击区域 交互友好 */
        QMenu::item {
            padding: 9px 28px 9px 22px;
            margin: 1px 4px;
            border-radius: 4px;
        }
        /* 选中/悬浮样式 - 柔蓝渐变 点睛强调 与主界面图标色呼应 不刺眼 */
        QMenu::item:selected {
            background-color: rgba(64, 158, 255, 0.8);
            color: white;
        }
        /* 分隔线样式 - 细腻浅灰 不突兀 */
        QMenu::separator {
            height: 1px;
            background-color: #D9D9D9;
            margin: 4px 8px;
        }
        /* 去掉菜单项的虚线焦点框 美化细节 */
        QMenu::item:focus {
            outline: none;
        }
    )";

    // ========== 新增：顶部菜单栏样式 与整体UI完美融合 ==========
    QString menuBarStyle = R"(
        QMenuBar {
            background-color: #E5E6EB;
            color: #333333;
            border-bottom: 1px solid #DCDFE6;
            font-size: 12px;
        }
        QMenuBar::item {
            padding: 8px 12px;
            margin: 0 2px;
        }
        QMenuBar::item:selected {
            background-color: rgba(64, 158, 255, 0.2);
            color: #333333;
            border-radius: 4px;
        }
    )";
    menuBar()->setStyleSheet(menuBarStyle);

    fileMenu->setStyleSheet(menuStyle);
    playMenu->setStyleSheet(menuStyle);
    viewMenu->setStyleSheet(menuStyle);

    // 连接菜单动作
    connect(fullscreenAction, &QAction::triggered,
            this, &VideoPlayerMW::onFullscreenToggled);
    connect(showPlaylistAction, &QAction::toggled, this, [this](bool visible) {
        qDebug() << "显示播放列表:" << visible;
        m_playlistWidget->setVisible(visible);
    });

    qDebug() << "菜单创建完成";
}

void VideoPlayerMW::createConnections()
{
    connect(m_playlistWidget, &VideoPlaylistWidget::itemSelected,
            this, &VideoPlayerMW::onPlaylistItemSelected);
    connect(m_playlistWidget, &VideoPlaylistWidget::playRequested,
            this, &VideoPlayerMW::onPlayRequested);
    connect(m_controller, &BaseController::fullscreenToggled,
            this, &VideoPlayerMW::onFullscreenToggled);

    connect(m_playlistWidget,&VideoPlaylistWidget::setMode,m_controller,&StandardController::setMode);
    connect(m_playlistWidget,&VideoPlaylistWidget::initRequested,m_resMgr,&ResMgr::initResource);
    connect(m_resMgr, &ResMgr::resourceReady,m_playerFrame,&BasePlayerFrame::onResReady);

    connect(this,&VideoPlayerMW::PlayRequestToFrame,m_playerFrame,&BasePlayerFrame::onPlayRequest);
}

void VideoPlayerMW::onPlaylistItemSelected(const PlaylistItem &item)
{
    m_playerFrame->loadMedia(item.sourceURL, item.mode);
}

void VideoPlayerMW::onPlayRequested(const QString &source, VideoPlayerMode mode)
{
    qDebug() << "播放请求:" << source << "模式:" << mode;
    qDebug() << "资源URL:" << source;
    // m_playerFrame->loadMedia(source, mode);
    emit PlayRequestToFrame(source,mode);
}

void VideoPlayerMW::onMaximizeButtonClicked()
{

    if (m_isFullscreen) {
        qDebug() << "当前是全屏状态，先退出全屏";
        onFullscreenToggled();  // 调用现有的全屏切换函数
        // 退出全屏后，窗口应该恢复到最后一次记录的状态
        return;
    }

    // 根据当前状态切换最大化/恢复
    if (m_isMaximized) {
        if (m_isWSL2) {
            qDebug() << "WSL2 下手动恢复窗口";
            qDebug() << "恢复窗口到:" << m_normalGeometry;
            setGeometry(m_normalGeometry);
        } else {
            qDebug() << "调用 showNormal()";
            showNormal();
        }
        m_isMaximized = false;
    } else {
        qDebug() << "执行最大化窗口";
        if (m_isWSL2) {
            // 保存窗口最大化前的原始几何信息
            m_normalGeometry = geometry();
            qDebug() << "保存当前几何:" << m_normalGeometry;
            QRect screenArea = QGuiApplication::primaryScreen()->availableGeometry();
            qDebug() << "屏幕可用区域:" << screenArea;
            setGeometry(screenArea);
        } else {
            qDebug() << "调用 showMaximized()";
            showMaximized();
        }
        m_isMaximized = true;
    }

    // 更新最大化按钮文本
    if (maximizeBtn) {
        maximizeBtn->setText(m_isMaximized ? "🗗" : "□");
    }

}

void VideoPlayerMW::onFullscreenToggled()
{
    if (m_isFullscreen) {
        // 退出全屏
        if (m_isWSL2) {
            // 恢复窗口到之前的状态（最大化或正常）
            if (m_isMaximized) {
                QRect screenArea = QGuiApplication::primaryScreen()->availableGeometry();
                setGeometry(screenArea);
            } else {
                setGeometry(m_normalGeometry);
            }
            titleBar->show();
        } else {
            showNormal();
        }
        m_isFullscreen = false;
    } else {
        // 进入全屏前保存当前窗口状态
        m_isMaximized = this->isMaximized();
        if (!m_isMaximized) {
            m_normalGeometry = geometry();
        }
        if (!m_isMaximized) {
        }

        // 进入全屏
        if (m_isWSL2) {
            titleBar->hide();
            QRect screenArea = QGuiApplication::primaryScreen()->geometry();
            setGeometry(screenArea);
        } else {
            showFullScreen();
        }
        m_isFullscreen = true;
    }

    // 更新最大化按钮文本（在全屏状态下按钮隐藏，但状态需要更新）
    if (maximizeBtn) {
        maximizeBtn->setText(m_isMaximized ? "🗗" : "□");
    }
}