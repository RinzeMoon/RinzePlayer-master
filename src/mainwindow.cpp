#include "../Header/Ui/mainwindow.h"
#include "../Header/Ui/AudioControlBar.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QWindow>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_isMoving = false;
    // 基础窗口设置（无边框+初始尺寸）
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    resize(1280, 800);

    // ======================================
    // 1. 唯一的中央部件（QMainWindow必须且只能有一个）
    // ======================================
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget); // 最终生效的中央部件

    // 中央部件主布局（垂直方向承载所有内容）
    QVBoxLayout *centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    // ======================================
    // 2. 外层阴影容器（替代重复的centralWidget）
    // ======================================
    QWidget *shadowContainer = new QWidget();
    shadowContainer->setStyleSheet(R"(
        QWidget {
            background-color: white;
            border-radius: 8px;
        }
    )");
    shadowContainer->setMinimumSize(1024, 720);
    QVBoxLayout *shadowLayout = new QVBoxLayout(shadowContainer);
    shadowLayout->setContentsMargins(0, 0, 0, 0);
    shadowLayout->setSpacing(0);

    // ======================================
    // 3. 主内容容器（承载顶部栏、内容区、控制栏）
    // ======================================
    QWidget *mainContainer = new QWidget();
    m_mainLayout = new QVBoxLayout(mainContainer);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // 创建各区域
    createTopBar();      // 顶部菜单栏
    createContentArea(); // 主内容区
    createControlBar();  // 控制栏（你的AudioControlBar）

    // 关键：按顺序添加到主布局，并设置拉伸因子
    m_mainLayout->addWidget(m_topBar);           // 顶部栏：不拉伸
    m_mainLayout->addWidget(m_contentArea, 1);   // 内容区：占满剩余空间（拉伸因子1）
    m_mainLayout->addWidget(m_controlBar);       // 控制栏：不拉伸（固定高度）

    // ======================================
    // 4. 组装所有布局
    // ======================================
    shadowLayout->addWidget(mainContainer);      // 主内容容器 → 阴影容器
    centralLayout->addWidget(shadowContainer);   // 阴影容器 → 中央部件布局

    // 红线圈：给最终的中央部件加边框（替代之前被覆盖的设置）
    /*
    centralWidget->setStyleSheet(R"(
        QWidget {
            border: 1px solid #e63946;
            border-radius: 2px;
            margin: 0px;
        }
    )");
    */
    qDebug() << "初始化连接完成";
    qDebug() << "初始化MusicPage";
    initMusicPlayerWidget();
    qDebug() << "MusicPage初始化成功";

    qDebug() << "=== 开始数据同步检查 ===";
    MusicSheetManager* manager = MusicSheetManager::getInstance();

    // 强制加载所有在 item_state.json 中提到的歌单
    QFile file("../LocalSheets/itemMeta/item_state.json");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isArray()) {
            QJsonArray itemsArray = doc.array();
            qDebug() << "初始化时发现" << itemsArray.size() << "个歌单项";

            for (const QJsonValue& val : itemsArray) {
                QJsonObject itemObj = val.toObject();
                QString sheetId = itemObj["sheetId"].toString();

                if (!sheetId.isEmpty() && !manager->getSheets().contains(sheetId)) {
                    qDebug() << "初始化时加载歌单:" << sheetId;
                    manager->loadSheetByIdFromLocal(sheetId);
                }
            }
        }
    }
    qDebug() << "=== 数据同步检查完成 ===";

    initializeSheetUIs();
    playlistWidget->reloadItems();
    createConnections();
    qDebug() << "窗口初始化完成 - 布局层级已整理";

    player = AudioPlayer::getInstance();
    qDebug() << "播放器获取成功";

    m_musicSheetManager = MusicSheetManager::getInstance();
    qDebug() << "歌单管理器获取成功";
}

MainWindow::~MainWindow()
{

}

void MainWindow::initializeSheetUIs()
{
    MusicSheetManager* manager = MusicSheetManager::getInstance();
    auto allSheetInfos = manager->getAllSheetInfos();
    qDebug() << "成功从本地获得LocalSheets(是否为空)" <<  allSheetInfos.isEmpty();

    for (const auto& sheetInfo : allSheetInfos) {
        QString sheetId = sheetInfo.first;
        QString sheetTitle = sheetInfo.second;

        // 获取歌单数据
        MusicSheet sheet = manager->getSheetById(sheetId);
        if (sheet.id.isEmpty()) {
            qWarning() << "无法获取歌单数据，ID:" << sheetId;
            continue;
        }

        // 使用辅助函数创建UI实例
        createSheetUiInstance(sheetId, sheet);

        qDebug() << "已为歌单" << sheetTitle << "(ID:" << sheetId << ") 创建UI并连接信号";
    }

    // 默认显示首页
    m_rightStack->setCurrentIndex(0);
}

void MainWindow::createConnections()
{
    connect(qobject_cast<AudioControlBar*>(m_controlBar), &AudioControlBar::clicked,
            this, &MainWindow::onControlBarClicked);

    connect(playlistWidget, &CollapsibleWidget::createSheetRequested,
            this, &MainWindow::onCreateSheetRequested);

    connect(videoListWidget, &CollapsibleWidget::createSheetRequested,
            this, &MainWindow::onCreateSheetRequested);

    connect(playlistWidget, &CollapsibleWidget::sheetItemClicked,
            this, &MainWindow::onShowSheetUi);

    MusicSheetManager* manager = MusicSheetManager::getInstance();

    connect(manager, &MusicSheetManager::sheetDataChanged,
            this, &MainWindow::onSheetDataChanged);

    connect(manager, &MusicSheetManager::sheetSaved,
            this, &MainWindow::onSheetSaved);
    // 初始化歌单连接
    AudioControlBar* controlBar = qobject_cast<AudioControlBar*>(m_controlBar);
    if (controlBar) {
        controlBar->initializeMusicSheetConnection(m_currentSheetUi);
        qDebug() << "[MainWindow] 已初始化歌单连接";
    }
    
    auto* ACbar = AudioControlBar::getInstance();

    connect(ACbar, &AudioControlBar::setPlayStatusToPlayList_Ui,ACbar->getCurrentPlaylistDlg(),&AudioPlaylistPopup::setCurrentSongPlaying);
    connect(ACbar,&AudioControlBar::sentCurrentSongMetaToUi,m_musicPlayerWidget,&MusicPlayerWidget::currentSongUpdated);

    qDebug() << "[MainWindow] 所有连接建立完成";
}

void MainWindow::initMusicPlayerWidget()
{
    // 创建PlayerWidget，父对象设为centralWidget（确保覆盖范围正确）
    m_musicPlayerWidget = new MusicPlayerWidget(centralWidget());
    m_musicPlayerWidget->setStyleSheet("background-color: #FFFFFF; color: #333333;");

    // 【关键1：初始不设固定尺寸，后续显示时动态计算】
    m_musicPlayerWidget->setMinimumSize(0, 0); // 解除尺寸限制
    m_musicPlayerWidget->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    // 【关键2：初始隐藏位置：Y轴在 controlBar 上方（完全隐藏），X轴=0（左对齐）】
    QWidget *central = centralWidget();
    int controlBarTopY = m_controlBar->geometry().top(); // controlBar 顶部的Y坐标
    QRect initGeom(
        0,                      // X=0：左对齐，无偏移
        controlBarTopY - 100,   // Y：隐藏在 controlBar 上方（高度100仅为临时值，显示时会更新）
        central->width(),       // 初始宽度=centralWidget宽度（避免显示时宽度跳动）
        100                     // 初始高度仅为临时值，显示时会更新
    );
    m_musicPlayerWidget->setGeometry(initGeom);
    m_musicPlayerWidget->hide();
    m_isPlayerShown = false;

    // 动画初始化不变
    m_playerAnim = new QPropertyAnimation(m_musicPlayerWidget, "geometry", this);
    m_playerAnim->setDuration(350);
    m_playerAnim->setEasingCurve(QEasingCurve::OutQuart);
}

// 处理controlBar点击事件的槽函数
void MainWindow::onControlBarClicked()
{
    if (!m_musicPlayerWidget || !m_playerAnim || !m_controlBar
        || m_playerAnim->state() == QAbstractAnimation::Running) {
        return;
    }

    disconnect(m_playerAnim, &QPropertyAnimation::finished, nullptr, nullptr);
    QWidget *central = centralWidget();
    const int margin = 30; // 此margin仅保留，不再用于尺寸计算（消除边距影响）

    // 【关键1：计算“完全覆盖区域”的尺寸】
    int targetWidth = central->width(); // 宽度 = centralWidget 宽度（无任何边距）
    // 高度 = controlBar 顶部Y坐标 - 0（从窗口顶部到 controlBar 顶部的全部高度）
    int targetHeight = m_controlBar->geometry().top();

    // 【关键2：处理“显示”逻辑（完全覆盖目标区域）】
    if (!m_isPlayerShown) {
        // 起始位置：Y轴在窗口顶部上方（隐藏状态），X=0，尺寸=目标尺寸
        QRect startGeom(
            0,                      // X=0：左对齐
            -targetHeight,          // Y=-目标高度：隐藏在窗口顶部上方
            targetWidth,            // 宽度=目标宽度
            targetHeight            // 高度=目标高度
        );
        m_musicPlayerWidget->setGeometry(startGeom);
        m_musicPlayerWidget->show();
        m_musicPlayerWidget->raise(); // 确保在最上层，不被其他控件遮挡

        // 结束位置：Y=0（窗口顶部），X=0，尺寸=目标尺寸（完全覆盖）
        QRect endGeom(
            0,                      // X=0：左对齐
            0,                      // Y=0：上对齐（覆盖到窗口顶部）
            targetWidth,            // 宽度=centralWidget宽度
            targetHeight            // 高度=除去controlBar的剩余高度
        );
        m_playerAnim->setStartValue(startGeom);
        m_playerAnim->setEndValue(endGeom);

        connect(m_playerAnim, &QPropertyAnimation::finished, this, [=]() {
            m_isPlayerShown = true;
        });
    }
    // 【关键3：处理“隐藏”逻辑（缩回 controlBar 上方）】
    else {
        // 结束位置：Y=controlBar顶部Y坐标（完全隐藏在 controlBar 上方）
        QRect endGeom(
            0,                      // X保持0，避免左右偏移
            targetHeight,           // Y=controlBar顶部Y坐标：隐藏
            targetWidth,            // 宽度不变
            targetHeight            // 高度不变
        );
        m_playerAnim->setStartValue(m_musicPlayerWidget->geometry());
        m_playerAnim->setEndValue(endGeom);

        connect(m_playerAnim, &QPropertyAnimation::finished, this, [=]() {
            m_musicPlayerWidget->hide();
            m_isPlayerShown = false;
        });
    }

    m_playerAnim->setDuration(300);
    m_playerAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_playerAnim->start();
}

// 顶部栏实现（加宽处理）
void MainWindow::createTopBar()
{
    m_topBar = new QWidget();
    m_topBar->setFixedHeight(60); // 增加高度
    m_topBar->setStyleSheet("background-color: #f9f9f9;");

    QHBoxLayout *topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(30, 0, 30, 0); // 增加左右留白
    topLayout->setSpacing(24); // 增加控件间距

    // 1. LOGO区域（加宽）
    m_logoArea = new QWidget();
    m_logoArea->setFixedWidth(150); // 从120px加宽到180px
    m_logoArea->setStyleSheet("background-color: transparent;");

    QLabel *logoLabel = new QLabel("Rinze", m_logoArea); // 加长文本
    logoLabel->setStyleSheet("color: #1f1e33; font-size: 20px; font-weight: 600;");
    QHBoxLayout *logoLayout = new QHBoxLayout(m_logoArea);
    logoLayout->setContentsMargins(0, 0, 0, 0);
    logoLayout->addWidget(logoLabel);

    topLayout->addWidget(m_logoArea);

    // 2. 搜索框（加宽）

    // 初始化搜索框
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("搜索媒体文件、艺术家或专辑...");

    // 统一尺寸（避免样式表与max尺寸冲突）
    m_searchEdit->setMaximumWidth(400);    // 最大宽度400px
    m_searchEdit->setMaximumHeight(36);    // 最大高度36px（比原32稍大，留足圆角空间）
    m_searchEdit->setMinimumHeight(36);    // 固定高度36px，确保圆角正常显示

    // 添加搜索图标（左侧）
    QAction* searchAction = new QAction(this);
    searchAction->setIcon(QIcon(":/icons/search.png"));  // 替换为你的搜索图标路径
    searchAction->setIconText("搜索");
    m_searchEdit->addAction(searchAction, QLineEdit::LeadingPosition);  // 左侧显示

    // 样式表优化
    m_searchEdit->setStyleSheet(QString(R"(
    QLineEdit {
        border-radius: 18px;  /* 高度36px，圆角18px刚好半圆 */
        border: 1px solid %1;  /* 正常状态边框色 */
        padding-left: 40px;   /* 左侧留空间给图标（图标约24px+间距16px） */
        padding-right: 16px;  /* 右侧内边距 */
        background-color: %2; /* 背景色 */
        font-size: 13px;      /* 字体大小 */
        color: #333333;       /* 输入文字颜色 */
    }
    /* 占位符文本样式 */
    QLineEdit::placeholder-text {
        color: #999999;       /* 占位符灰色，更柔和 */
        font-style: normal;   /* 避免默认斜体 */
    }
    /* 聚焦状态 */
    QLineEdit:focus {
        border-color: %3;     /* 聚焦时边框色（主色） */
        outline: none;
    }
    /* 鼠标悬停状态 */
    QLineEdit:hover {
        border-color: #BBBBBB;  /* 悬停时边框稍亮，增强交互感 */
    }
)").arg(BORDER_NORMAL, CARD_BG, PRIMARY_COLOR));

    topLayout->addWidget(m_searchEdit);
    // 建议给topLayout添加间距，避免搜索框贴边
    topLayout->setContentsMargins(20, 10, 20, 10);  // 上下左右间距

    topLayout->addWidget(m_searchEdit);

    topLayout->addStretch();

    // 3. 右侧控制按钮（加大尺寸）
    m_btnSettings = new QPushButton();
    m_btnSettings->setFixedSize(38, 38); // 加大按钮尺寸
    m_btnSettings->setStyleSheet(R"(
        QPushButton {
            border-radius: 6px;
            background-color: transparent;
            color: #666666;
            font-size: 16px;
        }
        QPushButton:hover {
            background-color: #f8edeb;
            color: #e63946;
        }
    )");
    m_btnSettings->setText("⚙");

    m_btnMinimize = new QPushButton("-", this);
    m_btnMaximize = new QPushButton("□", this);
    m_btnClose = new QPushButton("×", this);

    QList<QPushButton*> winBtns = {m_btnMinimize, m_btnMaximize, m_btnClose};
    for (QPushButton *btn : winBtns) {
        btn->setFixedSize(38, 38); // 加大按钮尺寸
    }

    m_btnMinimize->setStyleSheet(R"(
        QPushButton {
            border-radius: 6px;
            background-color: transparent;
            color: #666666;
            font-size: 16px;
        }
        QPushButton:hover {
            background-color: #eeeeee;
        }
    )");

    m_btnMaximize->setStyleSheet(m_btnMinimize->styleSheet());

    m_btnClose->setStyleSheet(R"(
        QPushButton {
            border-radius: 6px;
            background-color: transparent;
            color: #e63946;
            font-size: 16px;
        }
        QPushButton:hover {
            background-color: #e63946;
            color: white;
        }
    )");

    topLayout->addWidget(m_btnSettings);
    topLayout->addWidget(m_btnMinimize);
    topLayout->addWidget(m_btnMaximize);
    topLayout->addWidget(m_btnClose);

    // 窗口控制信号
    connect(m_btnMinimize, &QPushButton::clicked, this, &MainWindow::showMinimized);
    connect(m_btnMaximize, &QPushButton::clicked, this, [=]() {
        isMaximized() ? showNormal() : showMaximized();
    });
    connect(m_btnClose, &QPushButton::clicked, this, &MainWindow::close);
}

// 中间内容区（保持布局，调整导航宽度）
void MainWindow::createContentArea()
{
    // 1. 整体内容区容器（不变）
    m_contentArea = new QWidget();
    m_contentArea->setStyleSheet("background-color: white;");

    QHBoxLayout *contentLayout = new QHBoxLayout(m_contentArea);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    // 2. 左侧导航栏（不变）
    m_navArea = new QWidget();
    m_navArea->setFixedWidth(180);
    m_navArea->setStyleSheet("background-color: #f9f9f9;");

    QVBoxLayout *navLayout = new QVBoxLayout(m_navArea);
    navLayout->setContentsMargins(0, 24, 0, 0);
    navLayout->setSpacing(2);

    // 导航按钮（不变）
    QPushButton *nav1 = new QPushButton("首页", m_navArea);
    QPushButton *nav2 = new QPushButton("视频", m_navArea);
    QPushButton *nav3 = new QPushButton("音频", m_navArea);
    QList<QPushButton*> navBtns = {nav1, nav2, nav3};

    for (QPushButton *btn : navBtns) {
        btn->setFixedHeight(52);
        btn->setCheckable(true); // 允许选中状态
        btn->setStyleSheet(R"(
            QPushButton {
                border: none;
                background-color: transparent;
                color: #666666;
                text-align: center;
                border-radius: 6px;
                margin: 0 12px;
                font-size: 15px;
            }
            QPushButton:hover {
                background-color: #f8edeb;
                color: #e63946;
            }
            QPushButton:checked {
                background-color: #e63946;
                color: white;
            }
        )");
        navLayout->addWidget(btn);
    }
    nav1->setChecked(true); // 默认选中首页
    navLayout->addStretch();

    // 左侧导航内部的分隔线和列表（保持不变）
    QFrame* separatorTop = new QFrame(this);
    separatorTop->setFrameShape(QFrame::HLine);
    separatorTop->setStyleSheet("background-color: #eeeeee; margin: 12px 16px;");
    navLayout->addWidget(separatorTop);

    playlistWidget = new CollapsibleWidget("创建的歌单",this);
    playlistWidget->addItem("我的收藏",QString("0000"));
    createFavoriteSheet();  // 创建固定的收藏歌单

    navLayout->addWidget(playlistWidget);

    QFrame* separatorMiddle = new QFrame();
    separatorMiddle->setFrameShape(QFrame::HLine);
    separatorMiddle->setStyleSheet("background-color: #eeeeee; margin: 8px 16px;");
    navLayout->addWidget(separatorMiddle);

    videoListWidget = new CollapsibleWidget("视频播放列表",this);
    videoListWidget->addItem("教程合集");
    videoListWidget->addItem("演唱会录像");
    navLayout->addWidget(videoListWidget);

    navLayout->addStretch();

    // 左侧导航添加到整体布局（不变）
    contentLayout->addWidget(m_navArea);

    // 3. 左侧与右侧的分隔线（不变）
    QWidget *splitter = new QWidget();
    splitter->setFixedWidth(1);
    splitter->setStyleSheet("background-color: #eeeeee;");
    contentLayout->addWidget(splitter);

    // 4. 右侧页面切换容器（不变）
    m_rightStack = new QStackedWidget();
    m_rightStack->setStyleSheet("background-color: white;");

    m_homePage = new HomePage();
    m_currentSheetUi = new MusicSheetUI(this);
    m_rightStack->addWidget(m_homePage);
    m_rightStack->addWidget(m_currentSheetUi);
    // m_rightStack->addWidget(m_recentPlayPage);
    // m_rightStack->addWidget(m_localMusicPage);

    contentLayout->addWidget(m_rightStack, 1);

    // --------------------------
    // 核心修改：实现按钮互斥点击
    // --------------------------
    // 遍历所有导航按钮，绑定统一的点击处理函数
    for (QPushButton *btn : navBtns) {
        connect(btn, &QPushButton::clicked, this, [=]() {
            // 点击当前按钮时，将其他所有按钮设为未选中
            for (QPushButton *otherBtn : navBtns) {
                if (otherBtn != btn) { // 排除当前点击的按钮
                    otherBtn->setChecked(false);
                }
            }
            // 确保当前按钮处于选中状态（可选，防止意外取消）
            btn->setChecked(true);
        });
    }

    // 5. 导航按钮绑定右侧页面切换（补充完整）
    connect(nav1, &QPushButton::clicked, this, [=]() {
         m_rightStack->setCurrentIndex(0); // 切换到首页
    });
    connect(nav2, &QPushButton::clicked, this, [=]() {
         m_rightStack->setCurrentIndex(1); // 切换到视频页
    });
    connect(nav3, &QPushButton::clicked, this, [=]() {
         m_rightStack->setCurrentIndex(2); // 切换到音频页
    });
}

// 底部ControlBar（保持高度，调整样式）
void MainWindow::createControlBar()
{
    m_controlBar = AudioControlBar::getInstance();
    m_controlBar->setFixedHeight(120); // 保持100px，与子类UI适配
    // 仅给顶层AudioControlBar设样式，不影响子控件
    m_controlBar->setStyleSheet(R"(
        AudioControlBar { /* 用控件类名，精准匹配顶层容器 */
            background-color: #f9f9f9;
            border-top: 1px solid #eeeeee;
        }
    )");
}

// MainWindow.cpp 中实现槽函数（完整细节）
void MainWindow::onCreateSheetRequested() {
    // 1. 临时创建对话框（获取歌单名称）
    QDialog* createDialog = new QDialog(this);
    createDialog->setWindowTitle("创建新歌单");
    createDialog->setFixedSize(320, 160);
    createDialog->setModal(true);

    // 2. 对话框布局与控件
    QVBoxLayout* mainLayout = new QVBoxLayout(createDialog);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    QLabel* nameLabel = new QLabel("请输入歌单名称：", createDialog);
    nameLabel->setStyleSheet("font-size: 14px; color: #333;");
    mainLayout->addWidget(nameLabel);

    QLineEdit* nameEdit = new QLineEdit(createDialog);
    nameEdit->setPlaceholderText("例如：我的歌单");
    nameEdit->setStyleSheet(R"(
        QLineEdit { height: 30px; padding: 0 8px; border: 1px solid #ddd; border-radius: 4px; }
        QLineEdit:focus { border-color: #e63946; }
    )");
    mainLayout->addWidget(nameEdit);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* confirmBtn = new QPushButton("创建", createDialog);
    QPushButton* cancelBtn = new QPushButton("取消", createDialog);
    btnLayout->addWidget(confirmBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    // 3. 连接按钮信号
    connect(confirmBtn, &QPushButton::clicked, createDialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, createDialog, &QDialog::reject);

    // 4. 显示对话框并处理输入
    if (createDialog->exec() != QDialog::Accepted) {
        createDialog->deleteLater();
        return;
    }

    QString sheetName = nameEdit->text().trimmed();
    if (sheetName.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "歌单名称不能为空！");
        createDialog->deleteLater();
        return;
    }

    // 5. 创建新歌单对象
    MusicSheet newSheet;
    newSheet.id = QUuid::createUuid().toString().remove("{").remove("}").remove("-");
    newSheet.title = sheetName;
    newSheet.tag = "新建歌单";
    newSheet.songCount = 0;
    newSheet.playCount = 0;
    newSheet.desc = "";
    newSheet.cover = QPixmap(":/covers/default.png");
    newSheet.songs = QList<SongMeta>();

    // 6. 添加到管理器
    MusicSheetManager* manager = MusicSheetManager::getInstance();

    if (!manager->addTo_m_sheets(newSheet)) {
        QMessageBox::warning(this, "创建失败", "歌单已存在或添加失败！");
        createDialog->deleteLater();
        return;
    }

    qDebug() << "正在存储到sheet，状态：" << manager->getSheets().contains(newSheet.id);

    // 7. 保存到本地
    bool saveOk = manager->saveSheetsToLocalById(newSheet.id);
    if (!saveOk) {
        QMessageBox::critical(this, "错误", "歌单创建成功，但保存失败！");
    }

    // 8. 【关键修改】立即将新歌单添加到左侧列表并保存到 item_state.json
    qDebug() << "=== 开始添加新歌单到左侧列表 ===";
    playlistWidget->addItem(sheetName, newSheet.id);
    qDebug() << "新歌单已添加到左侧列表，立即保存到 item_state.json";
    playlistWidget->saveItems();
    qDebug() << "=== 左侧列表更新完成 ===";

    // 9. 【直接创建UI实例】
    // 如果已经存在，先清理
    if (m_sheetUiMap.contains(newSheet.id)) {
        MusicSheetUI* oldUi = m_sheetUiMap.take(newSheet.id);
        m_rightStack->removeWidget(oldUi);
        oldUi->deleteLater();
        qWarning() << "发现重复的歌单UI实例，已清理，ID:" << newSheet.id;
    }

    // 创建新的UI实例
    MusicSheetUI* sheetUi = new MusicSheetUI();
    sheetUi->setSheetId(newSheet.id);

    // 添加到映射表和堆栈
    m_sheetUiMap[newSheet.id] = sheetUi;
    m_rightStack->addWidget(sheetUi);

    // 连接信号
    connect(sheetUi, &MusicSheetUI::item_stateChanged,
            this, &MainWindow::onItemStateChanged);

    createSheetUiInstance(newSheet.id, newSheet);

    // 初始化内容
    sheetUi->insertInfoBySheet(newSheet);

    qDebug() << "创建歌单UI实例，ID:" << newSheet.id << "标题:" << newSheet.title;

    // 10. 自动切换到新创建的歌单
    m_currentSheetUi = sheetUi;
    m_currentSheetId = newSheet.id;
    m_rightStack->setCurrentWidget(m_currentSheetUi);

    QMessageBox::information(this, "成功", QString("歌单「%1」创建成功！").arg(sheetName));
    createDialog->deleteLater();
}

void MainWindow::createFavoriteSheet()
{
    MusicSheetManager* manager = MusicSheetManager::getInstance();
    const QString favoriteId = "0000"; // 固定ID，用于标识“我喜欢的音乐”

    // 1. 检查歌单是否已存在（避免重复创建）
    MusicSheet existingSheet = manager->getSheetById(favoriteId);
    if (!existingSheet.id.isEmpty()) {
        qDebug() << "「我喜欢的音乐」歌单已存在，无需创建";
        return;
    }

    // 2. 不存在则创建歌单
    MusicSheet favSheet;
    favSheet.id = favoriteId; // 固定ID，确保唯一且可识别
    favSheet.title = "我喜欢的音乐";
    favSheet.tag = "把喜欢的音乐存放到这里吧";
    favSheet.cover = QPixmap(":/covers/default.png");
    favSheet.songs = QList<SongMeta>(); // 初始为空列表
    favSheet.songCount = 0; // 初始歌曲数为0
    favSheet.playCount = 0;

    manager->addTo_m_sheets(favSheet);
    // 4. 关键：立即保存到本地，确保下次启动能加载
    bool saveOk = manager->saveSheetsToLocalById(favSheet.id); // 全量保存（或用saveSheetsToLocalById(favoriteId)）
    if (saveOk) {
        qDebug() << "「我喜欢的音乐」歌单创建并保存成功";
    } else {
        qWarning() << "「我喜欢的音乐」歌单创建成功，但保存失败";
    }
}

void MainWindow::onShowSheetUi(const QString &id) {
    qDebug() << "打开歌单,id:" << id;
    MusicSheetManager* manager = MusicSheetManager::getInstance();

    // 获取歌单数据 - 这会自动从本地加载
    const MusicSheet& sheet = manager->getSheetById(id);

    if (sheet.id.isEmpty()) {
        QMessageBox::warning(this, "提示", QString("未找到ID为「%1」的歌单").arg(id));
        return;
    }

    // 如果UI实例不存在，则创建
    if (!m_sheetUiMap.contains(id)) {
        qDebug() << "创建新的歌单UI实例，ID:" << id;

        // 使用辅助函数创建UI实例
        createSheetUiInstance(id, sheet);
    } else {
        // UI实例已存在，确保数据是最新的
        qDebug() << "使用现有的歌单UI实例，ID:" << id;
        MusicSheetUI* sheetUi = m_sheetUiMap[id];
        sheetUi->insertInfoBySheet(sheet);
    }

    // 切换到对应的UI实例
    m_currentSheetUi = m_sheetUiMap[id];
    m_currentSheetId = id;
    m_rightStack->setCurrentWidget(m_currentSheetUi);

    qDebug() << "切换到歌单UI，ID:" << id << "标题:" << sheet.title;
}

// 辅助函数：创建歌单UI实例
void MainWindow::createSheetUiInstance(const QString& sheetId, const MusicSheet& sheet)
{
    MusicSheetUI* sheetUi = new MusicSheetUI();
    sheetUi->setSheetId(sheetId);

    m_sheetUiMap[sheetId] = sheetUi;
    m_rightStack->addWidget(sheetUi);

    // 连接所有必要的信号
    connect(sheetUi, &MusicSheetUI::item_stateChanged,
            this, &MainWindow::onItemStateChanged);
    connect(sheetUi, &MusicSheetUI::playSongById,
            this, &MainWindow::onPlaySong);  // 注意：这里连接到 onPlaySong，不是 onShowSheetUi
    connect(sheetUi, &MusicSheetUI::requestAddSongToOtherSheet,
            this, &MainWindow::onrequestAddSongToOtherSheet);
    connect(sheetUi, &MusicSheetUI::requestRemoveSongFromSheet,
            this, &MainWindow::onRemoveSongFromSheetRequested);
    sheetUi->insertInfoBySheet(sheet);

    qDebug() << "创建并连接歌单UI实例，ID:" << sheetId << "标题:" << sheet.title;
}

// 绘制事件
void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制窗口阴影（可选）
    QRect shadowRect = rect();
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    path.addRect(10, 10, shadowRect.width()-20, shadowRect.height()-20);

    painter.fillPath(path, QBrush(Qt::white));
}

void MainWindow::mousePressEvent(QMouseEvent *event)
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

// 鼠标移动事件 - 处理拖动
void MainWindow::mouseMoveEvent(QMouseEvent *event)
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

        qDebug() << "WSL2: 拖动中 - 新位置:" << newWindowPosition;
    }

    QMainWindow::mouseMoveEvent(event);
}

// 鼠标释放事件 - 结束拖动
void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isMoving) {
        m_isMoving = false;

        // 在 WSL2 中不要使用 releaseMouse()
        this->setMouseTracking(false);

        qDebug() << "WSL2: 拖动结束";
    }

    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::onItemStateChanged(const QString & sheetId,const QString & LabelName)
{
    playlistWidget->changeItemById(sheetId, LabelName);
}

void MainWindow::onPlaySong(const QString& songId, const QString& sheetId)
{
    qDebug() << "播放歌曲，歌单ID:" << sheetId << "歌曲ID:" << songId;
    m_currentPlayingSongId = songId;
    m_currentPlayingSheetId = sheetId;
    // 更新所有歌单UI的播放状态
    updateAllSheetPlayingStates(sheetId, songId);

    QString filePath = m_musicSheetManager->getFilePathByIds(songId,sheetId);

    if (filePath == QString("NULL") && filePath == QString("Err"))
    {
        qDebug() << "查询出现问题 [FilePath]:" << filePath;
        return;
    }

    if (!filePath.isEmpty()) {
        // 通过统一的播放控制接口处理播放
        // player->onPlayStatusControl(filePath);
        qDebug() << "播放控制已调用:" << filePath;

        auto* playBar = AudioControlBar::getInstance();
        playBar->setCurrentFilePath(filePath);
        // 填充ControlBar的 Ui
        AudioMeta meta;
        playBar->ParseCoverInfo(meta);
        playBar->insertInfoByMeta();
        // 加入播放队列
        PushSongIntoPlayList(songId,sheetId);
    }
    else
    {
        qWarning() << "未找到歌曲文件路径，songId:" << songId;
    }

}

void MainWindow::updateAllSheetPlayingStates(const QString& playingSheetId, const QString& playingSongId) {
    for (auto it = m_sheetUiMap.begin(); it != m_sheetUiMap.end(); ++it) {
        MusicSheetUI* sheetUi = it.value();
        sheetUi->updatePlayingState(playingSheetId, playingSongId);
    }
}


void MainWindow::onrequestAddSongToOtherSheet(const QString& targetSheetId, const SongMeta& song)
{
    qDebug() << "=== MainWindow槽函数被调用 ===";
    qDebug() << "目标歌单:" << targetSheetId;
    qDebug() << "歌曲:" << song.name << "ID:" << song.id;

    // 直接调用 MusicSheetManager 的槽函数
    MusicSheetManager::getInstance()->onAddSongToSheet(targetSheetId, song);
}

void MainWindow::onSheetDataChanged(const QString& sheetId) {
    qDebug() << "歌单数据变化，更新UI:" << sheetId;

    // 更新对应的UI实例
    if (m_sheetUiMap.contains(sheetId)) {
        MusicSheetUI* sheetUi = m_sheetUiMap[sheetId];
        MusicSheet sheet = MusicSheetManager::getInstance()->getSheetById(sheetId);
        sheetUi->insertInfoBySheet(sheet);
    }

    // 如果当前显示的UI就是这个歌单，也更新
    if (m_currentSheetUi && m_currentSheetUi->getSheetId() == sheetId) {
        MusicSheet sheet = MusicSheetManager::getInstance()->getSheetById(sheetId);
        m_currentSheetUi->insertInfoBySheet(sheet);
    }
}

void MainWindow::onSheetSaved(const QString& sheetId) {
    qDebug() << "歌单已保存:" << sheetId;
}

void MainWindow::onRemoveSongFromSheetRequested(const QString& sheetId, const QString& songId) {
    qDebug() << "MainWindow: 转发删除歌曲请求，歌单:" << sheetId << "歌曲ID:" << songId;

    // 直接调用MusicSheetManager的槽函数
    MusicSheetManager::getInstance()->onRemoveSongFromSheet(sheetId, songId);
}

void MainWindow::PushSongIntoPlayList(const QString& songId, const QString& sheetId)
{
    auto * controlBar = AudioControlBar::getInstance();
    if (controlBar->getPlaylistPopup())
    {
        controlBar->getPlaylistPopup()->addSongToPlaylist(songId, sheetId);
        return;
    }
    qDebug() << "get到了nullptr,需要注意";
}

