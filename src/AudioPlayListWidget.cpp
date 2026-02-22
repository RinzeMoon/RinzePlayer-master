//
// Created by lsy on 2025/10/13.
//

#include "../Header/Ui/AudioPlayListWidget.h"
#include <QGuiApplication>
#include <QScreen>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

class MainWindow;


AudioPlaylistPopup::AudioPlaylistPopup(QWidget* parent) : QWidget(parent) {
    m_parser = AudioMetaParser::getInstance();
    setWindowFlags(Qt::Widget); // 或者完全移除setWindowFlags调用

    //setFixedSize(420, 600);
    setFixedWidth(420);
    initUI();
    initConnections();

    qDebug() << "[AudioPlaylistPopup] 弹窗创建完成（作为MainWindow子部件）";
}

void AudioPlaylistPopup::initUI() {
    // 基础窗口设置（原始稳定配置：无边框，普通窗口类型，不阻塞）
    setFixedWidth(420); // 固定宽度，避免布局错乱

    // 1. 外层容器（原始浅灰背景，稳定不冲突）
    QWidget* outerContainer = new QWidget(this);
    outerContainer->setStyleSheet(R"(
        background-color: #F9F9F9;
        border-radius: 8px;
        border: 1px solid #EAEAEA;
    )");

    // 2. 顶部标题栏（原始样式）
    QWidget* titleBar = new QWidget(outerContainer);
    titleBar->setFixedHeight(48);
    titleBar->setStyleSheet("background-color: #F5F5F5; border-bottom: 1px solid #EAEAEA;");

    m_titleLabel = new QLabel("播放列表", titleBar);
    m_titleLabel->setStyleSheet("font-size: 16px; color: #333; font-weight: 500;");

    m_btnClose = new QPushButton("×", titleBar);
    m_btnClose->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            color: #999;
            border: none;
            font-size: 18px;
            width: 48px;
            height: 48px;
        }
        QPushButton:hover {
            color: #333;
            background-color: #EFEFEF;
        }
    )");
    //connect(m_btnClose, &QPushButton::clicked, this, &AudioPlaylistPopup::close);

    QHBoxLayout* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_btnClose);
    titleLayout->setContentsMargins(20, 0, 0, 0);
    titleLayout->setSpacing(0);

    // 3. 列表控件（原始选中样式，稳定无冲突）
    m_listWidget = new QListWidget(outerContainer);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_listWidget->setStyleSheet(R"(
        QListWidget { border: none; background-color: #F9F9F9; }
        QListWidget::item {
            height: 74px;
            color: #333;
            padding-left: 16px;
            border: none;
        }
        QListWidget::item:hover { background-color: #F0F0F0; }
        QListWidget::item:selected {
            background-color: #F9F9F9;
            color: #222;
            border-left: 2px solid #165DFF;
            outline: none;
        }
        QScrollBar:vertical { width: 6px; background: #F9F9F9; }
        QScrollBar::handle:vertical { background: #CCC; border-radius: 3px; }
        QScrollBar::handle:vertical:hover { background: #AAA; }
    )");
    m_listWidget->setMinimumHeight(400);
    m_listWidget->setMaximumHeight(700);

    // 4. 底部按钮栏（原始样式）
    QWidget* btnBar = new QWidget(outerContainer);
    btnBar->setFixedHeight(56);
    btnBar->setStyleSheet("background-color: #F5F5F5; border-top: 1px solid #EAEAEA;");

    m_btnAdd = new QPushButton("+ 添加本地歌曲", btnBar);
    m_btnClear = new QPushButton("清空列表", btnBar);
    m_btnAdd->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            color: #666;
            border: none;
            font-size: 13px;
            padding: 0 16px;
            height: 32px;
            border-radius: 16px;
        }
        QPushButton:hover { color: #165DFF; background-color: #E6F0FF; }
    )");
    m_btnClear->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            color: #666;
            border: none;
            font-size: 13px;
            padding: 0 16px;
            height: 32px;
            border-radius: 16px;
        }
        QPushButton:hover { color: #FF4D4F; background-color: #FFF2F0; }
    )");

    QHBoxLayout* btnLayout = new QHBoxLayout(btnBar);
    btnLayout->addWidget(m_btnAdd);
    btnLayout->addWidget(m_btnClear);
    btnLayout->addStretch();
    btnLayout->setContentsMargins(16, 0, 16, 0);
    btnLayout->setSpacing(8);

    // 5. 外层容器布局
    QVBoxLayout* outerLayout = new QVBoxLayout(outerContainer);
    outerLayout->addWidget(titleBar);
    outerLayout->addWidget(m_listWidget);
    outerLayout->addWidget(btnBar);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // 6. 弹窗主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(outerContainer);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    if (parentWidget()) {
        QPoint parentPos = parentWidget()->mapToGlobal(QPoint(0, 0));
        m_popupPos = QPoint(
            parentPos.x() + 20, // 固定 X 偏移（你的原始逻辑）
            parentPos.y()       // Y 偏移设为父窗口顶部（show时再调整）
        );
    } else {
        QRect screen = QGuiApplication::primaryScreen()->geometry();
        m_popupPos = QPoint(
            screen.width() - width() - 20, // 固定 X 偏移
            20 // 无父窗口时 Y 偏移固定为20
        );
    }

    createContextMenu();
}

void AudioPlaylistPopup::initConnections() {
    qDebug() << "=== 开始初始化连接 ===";

    // 确保关闭按钮正确连接
    connect(m_btnClose, &QPushButton::clicked, this, [this]() {
    qDebug() << "Lambda关闭按钮点击";
    this->hide();
});


    // 其他连接
    connect(m_listWidget, &QListWidget::itemClicked, this, &AudioPlaylistPopup::onItemClicked);
    connect(m_btnAdd, &QPushButton::clicked, this, &AudioPlaylistPopup::onAddClicked);
    connect(m_btnClear, &QPushButton::clicked, this, &AudioPlaylistPopup::onClearClicked);
    connect(PlaylistManager::getInstance(), &PlaylistManager::playlistUpdated,
            this, &AudioPlaylistPopup::refreshList);

    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listWidget, &QListWidget::customContextMenuRequested,
            this, &AudioPlaylistPopup::onCustomContextMenuRequested);

    // 连接PlaylistManager的信号（确保删除后自动刷新）
    PlaylistManager* manager = PlaylistManager::getInstance();
    if (manager) {
        connect(manager, &PlaylistManager::playlistUpdated, this, &AudioPlaylistPopup::refreshList);
        connect(manager, &PlaylistManager::songPlaylistUpdated, this, &AudioPlaylistPopup::refreshList);
    }

    qDebug() << "=== 连接初始化完成 ===";
}

void AudioPlaylistPopup::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
}

void AudioPlaylistPopup::hideEvent(QHideEvent* event) {
    // 确保所有按钮状态被重置
    m_btnClose->setDown(false);
    m_btnAdd->setDown(false);
    m_btnClear->setDown(false);

    QWidget::hideEvent(event);
}


void AudioPlaylistPopup::checkScreenBounds() {
    if (m_popupPos.isNull()) return;

    QRect screenRect = QGuiApplication::primaryScreen()->geometry();
    QRect popupRect(m_popupPos, size());

    // 仅在Popup超出屏幕时调整，优先保持中心对齐
    if (popupRect.top() < screenRect.top()) {
        m_popupPos.setY(screenRect.top() + 10); // 顶部贴边
    } else if (popupRect.bottom() > screenRect.bottom()) {
        m_popupPos.setY(screenRect.bottom() - popupRect.height() - 10); // 底部贴边
    }

    // 补充：确保左右边界也在屏幕内（避免超出屏幕右侧）
    if (popupRect.right() > screenRect.right()) {
        m_popupPos.setX(screenRect.right() - popupRect.width() - 10);
    } else if (popupRect.left() < screenRect.left()) {
        m_popupPos.setX(screenRect.left() + 10);
    }

    move(m_popupPos);
}


void AudioPlaylistPopup::onItemClicked(QListWidgetItem* item) {

    AudioItemWidget* widget = qobject_cast<AudioItemWidget*>(m_listWidget->itemWidget(item));
    if (widget) {
        emit playRequested(widget->getMeta());
    }
}

void AudioPlaylistPopup::onAddClicked() {
    QStringList filePaths = QFileDialog::getOpenFileNames(
        this, "选择音频文件",
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        "音频文件 (*.mp3 *.flac *.wav *.m4a)"
    );
    if (filePaths.isEmpty()) return;

    // 修复：使用统一的添加方法
    foreach (const QString& path, filePaths) {
        AudioUtil::AudioMeta meta;
        QString error;
        if (m_parser->parse(path, meta, error)) {
            AudioUtil::SongMeta songMeta;
            songMeta.fromAudioMeta(meta);
            addAudioItem(songMeta); // 使用统一的方法添加
        } else {
            QMessageBox::warning(this, "解析失败", error);
        }
    }
}

void AudioPlaylistPopup::onClearClicked()
{
    qDebug() << "[PlaylistPopup] 清除前 - 列表项目数:" << m_listWidget->count();
    //qDebug() << "[PlaylistPopup] 清除前 - 管理器项目数:" << PlaylistManager::getInstance()->itemCount();

    PlaylistManager::getInstance()->clearAll();
    m_listWidget->clear();

    qDebug() << "[PlaylistPopup] 清除后 - 列表项目数:" << m_listWidget->count();
    //qDebug() << "[PlaylistPopup] 清除后 - 管理器项目数:" << PlaylistManager::getInstance()->itemCount();
}

void AudioPlaylistPopup::onCloseClicked() {
    hide();
}

void AudioPlaylistPopup::onPlayListManagerRequest(const QString & m_id,const QString & m_Sheetid)
{
    qDebug() << "播放列表Popup收到数据请求: songId=" << m_id << ", sheetId=" << m_Sheetid;
    MusicSheetManager* manager = MusicSheetManager::getInstance();
    if (!manager) {
        qDebug() << "[AudioPlaylistPopup] 错误：MusicSheetManager 未初始化";
        return;
    }

    // 获取目标歌单
    MusicSheet currentSheet = manager->getSheetById(m_Sheetid);
    if (currentSheet.id.isEmpty()) {
        qDebug() << "[AudioPlaylistPopup] 错误：未找到歌单 sheetId=" << m_Sheetid;
        return;
    }

    // 查找目标歌曲
    QList<SongMeta> currentSongs = currentSheet.songs;
    bool found = false;
    for (const auto & song : currentSongs) {
        qDebug() << "[PlayListWIdget] 测试 内容" << song.filePath;
        if (song.id == m_id) {
            // 调用已有接口添加歌曲（自动转换 SongMeta→AudioMeta）
            addAudioItem(song);
            qDebug() << "已添加歌曲：" << song.name << "（id=" << song.id << "）";
            found = true;
            break;
        }
    }

    if (!found) {
        qDebug() << "未发现歌曲: songId=" << m_id << " 在歌单 sheetId=" << m_Sheetid;
    }
}

void AudioPlaylistPopup::connectToMusicSheet(MusicSheetUI* musicSheetUi)
{
    if (!musicSheetUi) {
        qDebug() << "[AudioPlaylistPopup] 错误：MusicSheetUI 指针为空";
        return;
    }

    // 直接连接歌单的播放信号到弹窗
    bool conn = connect(musicSheetUi, &MusicSheetUI::playSongById,
                       this, &AudioPlaylistPopup::addSongToPlaylist);

    qDebug() << "[AudioPlaylistPopup] 直接连接歌单信号：" << conn;
    if (conn) {
        qDebug() << "[AudioPlaylistPopup] 已建立直接信号连接，无需MainWindow转发";
    }
}

void AudioPlaylistPopup::onCustomContextMenuRequested(const QPoint& pos) {
    QListWidgetItem* item = m_listWidget->itemAt(pos);
    if (!item) return; // 如果没有选中item，不显示菜单

    m_contextMenuItem = item; // 保存当前右键的item

    // 更新菜单项状态
    m_actionPlay->setEnabled(true);
    m_actionRemove->setEnabled(true);

    // 显示菜单
    m_contextMenu->exec(m_listWidget->mapToGlobal(pos));
}

void AudioPlaylistPopup::onMenuPlayAction() {
    if (!m_contextMenuItem) return;

    // 获取音频元数据并发出播放信号
    AudioMeta meta = m_contextMenuItem->data(Qt::UserRole).value<AudioMeta>();
    emit playRequested(meta);

    m_contextMenuItem = nullptr; // 清理
}

void AudioPlaylistPopup::onMenuRemoveAction() {
    if (!m_contextMenuItem) return;

    // 获取要删除的音频元数据
    AudioMeta meta = m_contextMenuItem->data(Qt::UserRole).value<AudioMeta>();
    int row = m_listWidget->row(m_contextMenuItem);

    // 从PlaylistManager中删除
    PlaylistManager* manager = PlaylistManager::getInstance();
    if (manager) {
        manager->removeSong(row); // 假设有removeAudio方法，需要你在PlaylistManager中实现
    }

    // 从UI中删除
    QListWidgetItem* item = m_listWidget->takeItem(row);
    delete item;

    // 更新所有条目的序号
    updateAllIndexes();

    m_contextMenuItem = nullptr; // 清理
}

void AudioPlaylistPopup::createContextMenu()
{
    // 创建菜单
    m_contextMenu = new QMenu(this);

    // 创建动作
    m_actionPlay = new QAction("播放", this);
    m_actionRemove = new QAction("从列表里删除", this);

    // 添加动作到菜单
    m_contextMenu->addAction(m_actionPlay);
    m_contextMenu->addSeparator(); // 分隔线
    m_contextMenu->addAction(m_actionRemove);

    // 设置菜单样式
    QString menuQSS = R"(
        QMenu {
            background-color: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 8px;
            padding: 4px 0;
            font-family: "Microsoft YaHei", "PingFang SC";
            font-size: 14px;
        }
        QMenu::item {
            padding: 8px 24px;
            color: #333333;
            background-color: transparent;
        }
        QMenu::item:selected {
            background-color: #f0f9ff;
            color: #1890ff;
            border-radius: 4px;
        }
        QMenu::item:disabled {
            color: #cccccc;
        }
        QMenu::separator {
            height: 1px;
            background-color: #f2f2f2;
            margin: 4px 0;
        }
    )";
    m_contextMenu->setStyleSheet(menuQSS);

    // 连接信号槽
    connect(m_actionPlay, &QAction::triggered, this, &AudioPlaylistPopup::onMenuPlayAction);
    connect(m_actionRemove, &QAction::triggered, this, &AudioPlaylistPopup::onMenuRemoveAction);
}