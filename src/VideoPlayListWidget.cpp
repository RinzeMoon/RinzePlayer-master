#include "../Header/Ui/VideoPlayListWidget.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QButtonGroup>
#include <QDebug>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QDesktopServices>
#include <QShortcut>
#include <QInputDialog>
#include <QUrl>

#include "Factory/AVDataFetcherFactory.h"

VideoPlaylistWidget::VideoPlaylistWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    createContextMenu();
    setupKeyboardShortcuts();
}

void VideoPlaylistWidget::setupUi()
{
    // 播放列表外层背景 - 哑光深灰 比主界面稍深 实现「区域突出」 核心配色
    this->setStyleSheet("background: #2F3136;");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ====== 添加源区域 ======
    QWidget *addSourceWidget = new QWidget();
    // 上分区背景 比外层稍暗+深色边框 框定区域 视觉分层突出
    addSourceWidget->setStyleSheet("background: #282A2E; border-bottom: 1px solid #4A4D53;");

    QVBoxLayout *sourceLayout = new QVBoxLayout(addSourceWidget);
    sourceLayout->setContentsMargins(15, 15, 15, 15);
    sourceLayout->setSpacing(10);

    // 模式选项卡 - 使用QButtonGroup确保单选
    QWidget *tabWidget = new QWidget();
    m_tabLayout = new QHBoxLayout(tabWidget);
    m_tabLayout->setSpacing(0);
    m_tabLayout->setContentsMargins(0, 0, 0, 0);

    tabTitles = {"本地", "网络", "RTMP"};
    m_tabGroup = new QButtonGroup(this);
    m_tabGroup->setExclusive(true); // 关键：确保单选

    for (int i = 0; i < tabTitles.size(); ++i)
    {
        QPushButton *tabBtn = new QPushButton(tabTitles[i]);
        tabBtn->setCheckable(true);
        tabBtn->setFixedHeight(32);
        tabBtn->setCursor(Qt::PointingHandCursor);

        QString style = QString(R"(
        QPushButton {
            background: #36383D;
            color: #B0B4BD;
            border: 1px solid #4A4D53;
            border-right: none;
            font-size: 12px;
            font-weight: 500;
        }
        QPushButton:hover {
            background: #3D3F45;
            color: #E2E2E5;
        }
        QPushButton:checked {
            background: #50A8FF;
            color: white;
            border-color: #50A8FF;
        }
        QPushButton:checked:hover {
            background: #4094EA;
        }
    )");

        if (i == 0)
        {
            style += "border-top-left-radius: 4px; border-bottom-left-radius: 4px;";
        }
        else if (i == tabTitles.size() - 1)
        {
            style += "border-top-right-radius: 4px; border-bottom-right-radius: 4px; border-right: 1px solid #4A4D53;";
        }

        tabBtn->setStyleSheet(style);

        m_tabLayout->addWidget(tabBtn);
        m_tabGroup->addButton(tabBtn, i); // 设置按钮ID

        if(i == 0)
        {
            tabBtn->setChecked(true);
        }
    }

    sourceLayout->addWidget(tabWidget);

    // 输入框和添加按钮
    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(8);

    m_inputEdit = new QLineEdit();
    m_inputEdit->setPlaceholderText("输入文件路径或URL...");
    m_inputEdit->setStyleSheet(R"(
        QLineEdit {
            background: #36383D;
            border: 1px solid #4A4D53;
            border-radius: 4px;
            padding: 10px 12px;
            color: #E2E2E5;
            font-size: 13px;
        }
        QLineEdit:focus {
            border-color: #50A8FF;
            background: #3A3C41;
            outline: none;
        }
        QLineEdit::placeholder {
            color: #777A80;
        }
    )");

    m_addBtn = new QPushButton("+ 添加");
    m_addBtn->setFixedHeight(40);
    m_addBtn->setCursor(Qt::PointingHandCursor);
    m_addBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                       stop:0 #50A8FF,
                                       stop:1 #4094EA);
            border: none;
            border-radius: 4px;
            color: white;
            font-weight: bold;
            font-size: 13px;
            min-width: 80px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                       stop:0 #60B2FF,
                                       stop:1 #50A8FF);
        }
        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                       stop:0 #4094EA,
                                       stop:1 #3084D8);
        }
    )");

    inputLayout->addWidget(m_inputEdit, 1);
    inputLayout->addWidget(m_addBtn);

    sourceLayout->addLayout(inputLayout);
    mainLayout->addWidget(addSourceWidget);

    // ====== 播放列表内容【核心突出区】重中之重 ======
    m_listWidget = new QListWidget();
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setStyleSheet(R"(
        QListWidget {
            background: #25272B;  /* 列表核心背景 最暗 形成视觉聚焦 突出重点 */
            border: none;
            color: #E2E2E5;
            font-size: 13px;
            outline: none;
            padding: 8px 5px;
        }
        QListWidget::item {
            border: 1px solid #404349;
            border-radius: 4px;
            padding: 12px 15px;
            margin: 4px 3px;
            background: #2C2E32;
        }
        QListWidget::item:selected {
            background: rgba(80, 168, 255, 0.25); /* 柔蓝半透底 高级不刺眼 */
            border-left: 3px solid #50A8FF;         /* 左侧高亮蓝边 选中超清晰 突出核心 */
            border-color: #4A4D53;
            color: white;
        }
        QListWidget::item:hover:!selected {
            background: #32343A;
            border-color: #4A4D53;
            color: #E2E2E5;
        }
        /* 滚动条美化+突出质感 同色系深灰 */
        QScrollBar:vertical {
            background: #282A2E;
            width: 10px;
            border-radius: 5px;
            margin: 2px 0;
        }
        QScrollBar::handle:vertical {
            background: #4A4D53;
            border-radius: 5px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #5C5F66;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");

    // 启用右键菜单
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    mainLayout->addWidget(m_listWidget, 1);

    // ====== 底部操作栏 ======
    QWidget *bottomWidget = new QWidget();
    bottomWidget->setStyleSheet("background: #282A2E; border-top: 1px solid #4A4D53;");
    bottomWidget->setFixedHeight(60);

    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(15, 10, 15, 10);
    bottomLayout->setSpacing(10);

    m_randomBtn = new QPushButton("🔀 随机播放");
    m_randomBtn->setFixedHeight(36);
    m_randomBtn->setCursor(Qt::PointingHandCursor);
    m_randomBtn->setStyleSheet(R"(
        QPushButton {
            background: #36383D;
            border: 1px solid #4A4D53;
            border-radius: 4px;
            color: #E2E2E5;
            font-size: 13px;
            padding: 0 15px;
        }
        QPushButton:hover {
            background: #3D3F45;
            border-color: #5C5F66;
            color: white;
        }
        QPushButton:pressed {
            background: #404247;
        }
    )");

    m_clearBtn = new QPushButton("🗑️ 清空列表");
    m_clearBtn->setFixedHeight(36);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setStyleSheet(R"(
        QPushButton {
            background: rgba(224, 78, 89, 0.15);
            border: 1px solid rgba(224, 78, 89, 0.4);
            border-radius: 4px;
            color: #E04E59;
            font-size: 13px;
            padding: 0 15px;
        }
        QPushButton:hover {
            background: rgba(224, 78, 89, 0.25);
            border-color: rgba(224, 78, 89, 0.6);
            color: #F0606A;
        }
        QPushButton:pressed {
            background: rgba(224, 78, 89, 0.35);
        }
    )");

    bottomLayout->addWidget(m_randomBtn);
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_clearBtn);

    mainLayout->addWidget(bottomWidget);

    // 连接信号
    connect(m_addBtn, &QPushButton::clicked, this, &VideoPlaylistWidget::onAddButtonClicked);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &VideoPlaylistWidget::onItemDoubleClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &VideoPlaylistWidget::onClearClicked);
    connect(m_randomBtn, &QPushButton::clicked, this, &VideoPlaylistWidget::onRandomClicked);

    // 连接右键菜单信号
    connect(m_listWidget, &QListWidget::customContextMenuRequested,
            this, &VideoPlaylistWidget::showContextMenu);

    // 连接选项卡按钮点击信号 - 使用新式信号槽连接
    connect(m_tabGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            [this](QAbstractButton *button) {
                int id = m_tabGroup->id(button);
                onModeTabChanged(id);
            });
}

void VideoPlaylistWidget::createContextMenu()
{
    m_contextMenu = new QMenu(this);

    m_playAction = new QAction("播放", this);
    m_removeAction = new QAction("删除", this);

    // 设置快捷键
    m_playAction->setShortcut(QKeySequence(Qt::Key_Return));
    m_removeAction->setShortcut(QKeySequence(Qt::Key_Delete));

    // 添加到菜单
    m_contextMenu->addAction(m_playAction);
    m_contextMenu->addAction(m_removeAction);

    // 连接信号
    connect(m_playAction, &QAction::triggered,
            this, &VideoPlaylistWidget::onPlayActionTriggered);
    connect(m_removeAction, &QAction::triggered,
            this, &VideoPlaylistWidget::onRemoveActionTriggered);
}

void VideoPlaylistWidget::setupKeyboardShortcuts()
{
    // Enter键播放
    QShortcut *playShortcut = new QShortcut(QKeySequence(Qt::Key_Return), m_listWidget);
    connect(playShortcut, &QShortcut::activated,
            this, &VideoPlaylistWidget::onPlayActionTriggered);

    // Delete键删除
    QShortcut *deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), m_listWidget);
    connect(deleteShortcut, &QShortcut::activated,
            this, &VideoPlaylistWidget::onRemoveActionTriggered);
}

void VideoPlaylistWidget::showContextMenu(const QPoint &pos)
{
    // 转换为列表控件的局部坐标
    QPoint globalPos = m_listWidget->mapToGlobal(pos);

    // 获取鼠标位置对应的项
    QListWidgetItem *item = m_listWidget->itemAt(pos);

    if (item) {
        // 有项被右键点击
        int index = m_listWidget->row(item);
        if (index >= 0 && index < m_items.size()) {
            m_contextMenuIndex = index;

            // 高亮选中该项
            m_listWidget->setCurrentItem(item);

            // 显示菜单
            m_contextMenu->exec(globalPos);
        }
    }
}

int VideoPlaylistWidget::getSelectedItemIndex() const
{
    QListWidgetItem *currentItem = m_listWidget->currentItem();
    if (currentItem) {
        return m_listWidget->row(currentItem);
    }
    return -1;
}

PlaylistItem VideoPlaylistWidget::getSelectedPlaylistItem() const
{
    int index = getSelectedItemIndex();
    if (index >= 0 && index < m_items.size()) {
        return m_items[index];
    }
    return PlaylistItem();
}

void VideoPlaylistWidget::onPlayActionTriggered()
{
    // 获取当前选中的项
    int index = -1;

    // 优先使用右键菜单索引
    if (m_contextMenuIndex >= 0) {
        index = m_contextMenuIndex;
    } else {
        // 如果没有右键菜单索引，使用当前选中的项
        index = getSelectedItemIndex();
    }

    if (index >= 0 && index < m_items.size()) {
        PlaylistItem item = m_items[index];

        qDebug() << "右键菜单 - 播放项:" << item.title << "源:" << item.sourceURL;

        // 更新UI：高亮当前项
        for (int i = 0; i < m_listWidget->count(); ++i) {
            QListWidgetItem *listItem = m_listWidget->item(i);
            QFont font = listItem->font();
            font.setBold(i == index);
            listItem->setFont(font);

            // 更新激活状态
            m_items[i].isActive = (i == index);
        }

        // 发射信号
        emit itemSelected(item);
        emit playRequested(item._id, item.mode);

        // 重置右键菜单索引
        m_contextMenuIndex = -1;
    } else {
        qWarning() << "无法播放：未选中有效的项";
    }
}

void VideoPlaylistWidget::onRemoveActionTriggered()
{
    // 获取当前选中的项
    int index = -1;

    // 优先使用右键菜单索引
    if (m_contextMenuIndex >= 0) {
        index = m_contextMenuIndex;
    } else {
        // 如果没有右键菜单索引，使用当前选中的项
        index = getSelectedItemIndex();
    }

    if (index >= 0 && index < m_items.size()) {
        PlaylistItem item = m_items[index];

        qDebug() << "右键菜单 - 删除项:" << item.title;

        // 确认对话框
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "确认删除",
                                      QString("确定要删除 '%1' 吗？").arg(item.title),
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            // 从列表中移除
            QListWidgetItem *listItem = m_listWidget->takeItem(index);
            delete listItem;

            // 从数据列表中移除
            m_items.removeAt(index);

            qDebug() << "已删除项:" << item.title;
        }

        // 重置右键菜单索引
        m_contextMenuIndex = -1;
    } else {
        qWarning() << "无法删除：未选中有效的项";
    }
}

void VideoPlaylistWidget::addItem(const PlaylistItem &item)
{
    m_items.append(item);

    QString displayText = QString("%1 (%2)")
                         .arg(item.title)
                         .arg(item.duration);

    if (item.mode == VideoPlayerMode::RTMP_STREAM)
    {
        displayText += " [直播]";
    }

    QListWidgetItem *listItem = new QListWidgetItem(displayText);
    if (item.isActive)
    {
        listItem->setBackground(QColor(33, 150, 243, 50));
    }
    m_listWidget->addItem(listItem);
}

void VideoPlaylistWidget::clear()
{
    m_listWidget->clear();
    m_items.clear();
}

QList<PlaylistItem> VideoPlaylistWidget::items() const
{
    return m_items;
}

PlaylistItem VideoPlaylistWidget::currentItem() const
{
    int row = m_listWidget->currentRow();
    if (row >= 0 && row < m_items.size())
    {
        return m_items[row];
    }
    return PlaylistItem();
}

void VideoPlaylistWidget::onAddButtonClicked()
{

    PlaylistItem item = createItemFromInput();
    /*
    if (item.sourceURL.isEmpty())
    {
        QMessageBox::warning(this, "错误", "请输入有效的路径或URL");
        return;
    }
    */
    VideoPlayerMode mode = currentTabMode();
    if (mode == VideoPlayerMode::LOCAL_FILE)
    {
        if (item.sourceURL.isEmpty())
        {
            QString file = QFileDialog::getOpenFileName(this,
            "选择媒体文件", "",
            "视频文件 (*.mp4 *.avi *.mkv *.mov *.flv *.wmv);;"
            "音频文件 (*.mp3 *.wav *.flac *.aac);;"
            "所有文件 (*.*)");
            if (!file.isEmpty())
            {
                item.sourceURL = AVSourceResolver::normalizeLocalFilePath(file.trimmed());
                item.title = QFileInfo(file).fileName();
                // 此时传入的是统一的file:// URL，ID会和其他场景一致
                item._id = idGen::generateFileId(item.sourceURL);
                addItem(item);
                emit initRequested(item.sourceURL, item.mode);
            }
        }
        else
        {
            item.sourceURL = m_inputEdit->text().trimmed();
            // 不能在这里read，通过信号read，我觉得这是个很成熟的决定
            /*
            fet_ptr = AVDataFetcherFactory::createFetcher(item.sourceURL);
            qDebug() << "启动读取资源线程";
            fet_ptr->start();
            qDebug() << "读取完毕";
            */
            emit initRequested(item.sourceURL, item.mode);
            addItem(item);
        }

    }
    else
    {
        qDebug() << "这部分是其他的模式，我还尚未实现,啥都不做,list上也不会有内容";

        addItem(item);
        emit playRequested(item.sourceURL, item.mode);

        return;
    }

    m_inputEdit->clear();
}

void VideoPlaylistWidget::onItemDoubleClicked(QListWidgetItem *listItem)
{
    int index = m_listWidget->row(listItem);
    if (index >= 0 && index < m_items.size())
    {
        const PlaylistItem &item = m_items[index];
        emit itemSelected(item);
        emit playRequested(item.sourceURL, item.mode);
    }
}

VideoPlayerMode VideoPlaylistWidget::currentTabMode() const
{
    int id = m_tabGroup->checkedId();
    switch (id)
    {
    case 0: return VideoPlayerMode::LOCAL_FILE;
    case 1: return VideoPlayerMode::HTTP_STREAM;
    case 2: return VideoPlayerMode::RTMP_STREAM;
    default: return VideoPlayerMode::LOCAL_FILE;
    }
}

PlaylistItem VideoPlaylistWidget::createItemFromInput() const
{
    PlaylistItem item;
    item.sourceURL = m_inputEdit->text().trimmed();
    item.mode = currentTabMode();

    // 生成标题
    if (item.mode == VideoPlayerMode::LOCAL_FILE)
    {
        item.title = QFileInfo(item.sourceURL).fileName();
    }
    else
    {
        // 从URL提取文件名或使用默认标题
        QUrl url(item.sourceURL);
        QString fileName = url.fileName();
        if (!fileName.isEmpty())
        {
            item.title = fileName;
        }
        else
        {
            item.title = QString("%1流").arg(
                item.mode == VideoPlayerMode::HTTP_STREAM ? "HTTP" : "RTMP");
        }
    }

    item.duration = "00:00";
    item.isActive = false;
    item._id = idGen::generateFileId(item.sourceURL);
    return item;
}

void VideoPlaylistWidget::onModeTabChanged(int index)
{
    qDebug() << "切换到播放模式 index:" << index << " 对应模式:" << VideoPlayerMode(index);
    // 1. 切换模式时：清空输入框内容 + 设置占位符，体验更好
    m_inputEdit->clear();
    QString placeholder;
    VideoPlayerMode targetMode;


    // 2. 分支逻辑 - 和你原有一致，只做变量抽离，更清晰
    switch (index)
    {
    case 0:
        placeholder = "输入文件路径或点击添加按钮选择...";
        targetMode = VideoPlayerMode::LOCAL_FILE;

        break;
    case 1:
        placeholder = "输入HTTP视频流URL...";
        targetMode = VideoPlayerMode::HTTP_STREAM;
        break;
    case 2:
        placeholder = "输入RTMP直播流地址...";
        targetMode = VideoPlayerMode::RTMP_STREAM;
        break;
    default:
        placeholder = "输入文件路径/HTTP/RTMP地址...";
        targetMode = VideoPlayerMode::LOCAL_FILE;
        break;
    }

    // 3. 更新输入框占位文本
    m_inputEdit->setPlaceholderText(placeholder);
    // 4. 发射模式切换信号，给控制器
    emit setMode(targetMode);
}

void VideoPlaylistWidget::onClearClicked()
{
    if (m_listWidget->count() > 0)
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "确认",
            "确定要清空播放列表吗？",
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes)
        {
            clear();
            emit clearRequested();
        }
    }
}

void VideoPlaylistWidget::onRandomClicked()
{
    if (m_listWidget->count() > 0)
    {
        // 随机播放逻辑
        int randomIndex = QRandomGenerator::global()->bounded(m_listWidget->count());
        m_listWidget->setCurrentRow(randomIndex);

        if (randomIndex >= 0 && randomIndex < m_items.size())
        {
            const PlaylistItem &item = m_items[randomIndex];
            emit itemSelected(item);
            emit playRequested(item.sourceURL, item.mode);
        }
    }
}