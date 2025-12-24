#ifndef RINZEPLAYER_AUDIOPLAYLISTWIDGET_H
#define RINZEPLAYER_AUDIOPLAYLISTWIDGET_H

// 必要Qt头文件（避免重复包含）
#include <QLabel>

#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QFileDialog>
#include <QMenu>
#include <QContextMenuEvent>
#include <QVariant>
#include <QWindow>
#include <QStandardPaths>

#include "../Header/Ui/AudioControlBar.h"
#include <../Header/Manager/PlaylistManager.h>

// 项目自定义头文件（按实际路径调整）
#include "../Global/Global.h"
#include "../Header/AudioPart/AudioMetaParser.h"
#include "../Header/Ui/MusicSheetUI.h"
using AudioUtil::AudioMeta;


// -------------------------- AudioItemWidget（类内完整实现）--------------------------
// 单个音频条目 Widget（保持完整尺寸和信息）
class AudioItemWidget : public QWidget {
    Q_OBJECT
public:
    explicit AudioItemWidget(const AudioMeta& meta, QWidget* parent = nullptr)
        : QWidget(parent), m_meta(meta), m_index(0) {
        initUI();
        updateTextLayout();
    }

    void setIndex(int index) {
        m_index = index;
        m_indexLabel->setText(QString("%1").arg(index));
    }

    const AudioMeta& getMeta() const { return m_meta; }

    void setPlaying(bool playing) {
        if (playing) {
            m_indexLabel->setStyleSheet("color: #ec4141; font-size: 12px; font-weight: bold;");
        } else {
            m_indexLabel->setStyleSheet("color: #999; font-size: 12px;");
        }
    }
private:
    void initUI() {
        // 1. 曲目序号（不变）
        m_indexLabel = new QLabel(this);
        m_indexLabel->setFixedWidth(30);
        m_indexLabel->setAlignment(Qt::AlignCenter);
        m_indexLabel->setStyleSheet(R"(
            color: #999;
            font-size: 12px;
            border: none;
        )");
        m_indexLabel->setText(QString("%1").arg(m_index));

        // 2. 封面（不变）
        m_coverLabel = new QLabel(this);
        m_coverLabel->setFixedSize(50, 50);
        m_coverLabel->setAlignment(Qt::AlignCenter);
        m_coverLabel->setStyleSheet("border-radius: 2px; background: #F5F5F5;");
        updateCover();

        // 3. 文本区域容器（新增专辑标签）
        QWidget* textContainer = new QWidget(this);
        textContainer->setStyleSheet("border: none;");
        QVBoxLayout* textLayout = new QVBoxLayout(textContainer);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);

        // 标题（不变）
        m_titleLabel = new QLabel(this);
        QFont titleFont = m_titleLabel->font();
        titleFont.setBold(true);
        titleFont.setPointSize(14);
        m_titleLabel->setFont(titleFont);
        m_titleLabel->setToolTip(m_meta.filePath);
        m_titleLabel->setStyleSheet(R"(
            color: #333;
            border: none;
        )");

        // 艺术家（不变）
        m_artistLabel = new QLabel(this);
        m_artistLabel->setStyleSheet(R"(
            color: #999;
            font-size: 12px;
            border: none;
        )");

        // -------------------------- 新增：专辑信息 --------------------------
        m_albumLabel = new QLabel(this);
        m_albumLabel->setStyleSheet(R"(
            color: #BBB; /* 比艺术家颜色更浅，区分层级 */
            font-size: 11px;
            border: none;
        )");

        textLayout->addWidget(m_titleLabel);
        textLayout->addWidget(m_artistLabel);
        textLayout->addWidget(m_albumLabel); // 加入布局

        // 4. 右侧信息区（新增格式+比特率）
        QWidget* rightContainer = new QWidget(this); // 右侧容器（整合时长和格式）
        rightContainer->setStyleSheet("border: none;");
        QVBoxLayout* rightLayout = new QVBoxLayout(rightContainer);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(1);

        // 时长（原逻辑保留，放入右侧容器）
        m_durationLabel = new QLabel(this);
        m_durationLabel->setStyleSheet(R"(
            color: #999;
            font-size: 12px;
            border: none;
        )");
        m_durationLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        // -------------------------- 新增：格式+比特率 --------------------------
        m_formatLabel = new QLabel(this);
        m_formatLabel->setStyleSheet(R"(
            color: #BBB; /* 浅色，不抢眼 */
            font-size: 10px;
            border: none;
        )");
        m_formatLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        rightLayout->addWidget(m_durationLabel);
        rightLayout->addWidget(m_formatLabel); // 加入右侧布局

        // 5. 主布局（调整右侧为容器，保持整体风格）
        QHBoxLayout* mainLayout = new QHBoxLayout(this);
        mainLayout->addWidget(m_indexLabel);
        mainLayout->addSpacing(8);
        mainLayout->addWidget(m_coverLabel);
        mainLayout->addSpacing(12);
        mainLayout->addWidget(textContainer, 1); // 文本区占主要空间
        mainLayout->addSpacing(8);
        mainLayout->addWidget(rightContainer); // 右侧容器（时长+格式）
        mainLayout->setContentsMargins(16, 12, 16, 12);
        mainLayout->setSpacing(0);

        setStyleSheet("border: none;");
        setLayout(mainLayout);
        setFixedHeight(74); // 高度不变，垂直紧凑排列
    }

    void updateCover() {
        if (m_meta.cover.isNull()) {
            m_coverLabel->setText("音");
            m_coverLabel->setStyleSheet("color: #999; border-radius: 2px; background: #F5F5F5;");
            return;
        }
        QPixmap scaled = m_meta.cover.scaled(50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_coverLabel->setPixmap(scaled);
    }

    void updateTextLayout() {
        // 原有信息更新
        QString titleText = m_meta.title.isEmpty() ? QFileInfo(m_meta.filePath).fileName() : m_meta.title;
        QString artistText = m_meta.artist.isEmpty() ? "未知艺术家" : m_meta.artist;
        m_durationLabel->setText(m_meta.durationString());

        // -------------------------- 新增信息更新 --------------------------
        // 专辑（无则显示"未知专辑"）
        QString albumText = m_meta.album.isEmpty() ? "未知专辑" : m_meta.album;
        // 格式+比特率（如"MP3 · 320kbps"，无则显示"未知格式"）
        QString formatText;
        if (!m_meta.format.isEmpty() && m_meta.bitRate > 0) {
            formatText = QString("%1 · %2kbps").arg(m_meta.format).arg(m_meta.bitRate);
        } else {
            formatText = "未知格式";
        }

        // 文本省略处理（适配宽度）
        int textWidth = m_titleLabel->width() - 20;
        if (textWidth <= 0) textWidth = 200;
        QFontMetrics titleMetrics(m_titleLabel->font());
        QFontMetrics artistMetrics(m_artistLabel->font());
        QFontMetrics albumMetrics(m_albumLabel->font()); // 专辑文本省略

        m_titleLabel->setText(titleMetrics.elidedText(titleText, Qt::ElideRight, textWidth));
        m_artistLabel->setText(artistMetrics.elidedText(artistText, Qt::ElideRight, textWidth));
        m_albumLabel->setText(albumMetrics.elidedText(albumText, Qt::ElideRight, textWidth)); // 专辑省略
        m_formatLabel->setText(formatText); // 格式信息短，无需省略
    }

    void resizeEvent(QResizeEvent* event) override {
        Q_UNUSED(event);
        updateTextLayout(); //  resize时同步更新文本省略
    }

private:
    AudioMeta m_meta;
    int m_index;
    QLabel* m_indexLabel = nullptr;
    QLabel* m_coverLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_artistLabel = nullptr;
    QLabel* m_durationLabel = nullptr;
    // -------------------------- 新增成员变量 --------------------------
    QLabel* m_albumLabel = nullptr;    // 专辑标签
    QLabel* m_formatLabel = nullptr;   // 格式+比特率标签
};

// -------------------------- AudioPlaylistWidget（仅类声明）--------------------------
// 播放列表 Popup 主类（自适应 Item 尺寸）
class AudioPlaylistPopup : public QWidget {
    Q_OBJECT
public:
    explicit AudioPlaylistPopup(QWidget* parent = nullptr);
    // 设置Popup显示位置（相对于屏幕）
    void setPopupPos(const QPoint& pos) { m_popupPos = pos; }

    // Wayland兼容：设置临时父窗口
    void setTransientParent(QWindow* parentWindow) {
        if (windowHandle() && parentWindow) {
            windowHandle()->setTransientParent(parentWindow);
        }
    }

    // 新增：更新所有条目序号（增删曲目时调用）
    void updateAllIndexes() {
        for (int i = 0; i < m_listWidget->count(); i++) {
            QListWidgetItem* item = m_listWidget->item(i);
            AudioItemWidget* widget = qobject_cast<AudioItemWidget*>(m_listWidget->itemWidget(item));
            if (widget) {
                widget->setIndex(i + 1); // 序号从1开始
            }
        }
    }

    int get_FIXED_WIDTH()
    {
        return FIXED_WIDTH;
    }

    void refreshList() {
        m_listWidget->clear();

        const QList<AudioMeta>& globalPlaylist = PlaylistManager::getInstance()->getPlaylist();
        qDebug() << "刷新播放列表，共有:" << globalPlaylist.size() << "首歌曲";

        if (globalPlaylist.isEmpty()) {
            // 空列表提示
            QListWidgetItem* emptyItem = new QListWidgetItem(m_listWidget);
            QLabel* emptyLabel = new QLabel("播放列表为空，点击添加按钮添加音乐", m_listWidget);
            emptyLabel->setAlignment(Qt::AlignCenter);
            emptyLabel->setStyleSheet("color: #999; padding: 10px;");
            emptyItem->setSizeHint(emptyLabel->sizeHint() + QSize(0, 20));
            m_listWidget->setItemWidget(emptyItem, emptyLabel);
            return;
        }

        // 修复：直接从全局管理器创建所有项目，确保数据一致性
        for (int i = 0; i < globalPlaylist.size(); ++i) {
            const AudioMeta & _meta = globalPlaylist[i];
            addItemToUI(_meta, i);
        }
    }

    // 修复：统一添加项目的方法
    void addItemToUI(const AudioMeta& meta, int index) {
        AudioItemWidget* widget = new AudioItemWidget(meta, m_listWidget);
        widget->setIndex(index + 1); // 立即设置正确的索引

        QListWidgetItem* listItem = new QListWidgetItem(m_listWidget);
        listItem->setSizeHint(widget->sizeHint());
        listItem->setData(Qt::UserRole, QVariant::fromValue(meta)); // 重要：存储元数据

        m_listWidget->setItemWidget(listItem, widget);
    }

    // 修复：添加音频到列表（同时更新数据源和UI）
    void addAudioItem(const SongMeta& meta) {
        qDebug() << "=== addAudioItem 开始 ===";
        qDebug() << "原始 SongMeta:";
        qDebug() << "  名称:" << meta.name;
        qDebug() << "  歌手:" << meta.singer;
        qDebug() << "  路径:" << meta.filePath;
        qDebug() << "  专辑:" << meta.album;

        // 1. 修正文件路径
        QString originalPath = meta.filePath;
        QString correctedPath = correctMusicPath(originalPath);

    if (correctedPath.isEmpty()) {
        qWarning() << "无法修正路径，歌曲添加失败:" << originalPath;
        return;
    }

    qDebug() << "修正后路径:" << correctedPath;

    // 2. 检查文件是否存在
    QFileInfo fileInfo(correctedPath);
    if (!fileInfo.exists()) {
        qWarning() << "文件不存在，歌曲添加失败:" << correctedPath;
        return;
    }

    // 3. 使用修正后的路径创建新的 AudioMeta
    AudioUtil::AudioMeta audioMeta;
    try {
        audioMeta = AudioUtil::AudioMeta(correctedPath);

        qDebug() << "新创建的 AudioMeta:";
        qDebug() << "  标题:" << audioMeta.title;
        qDebug() << "  艺术家:" << audioMeta.artist;
        qDebug() << "  专辑:" << audioMeta.album;
        qDebug() << "  格式:" << audioMeta.format;

        // 4. 更新到数据源
        PlaylistManager* manager = PlaylistManager::getInstance();
        if (!manager) return;

        manager->addAudio(audioMeta);
        qDebug() << "[############Test] 修正后:" << audioMeta.title << audioMeta.artist << audioMeta.album << audioMeta.format;
        }
        catch (const std::exception& e) {
        qWarning() << "创建 AudioMeta 失败:" << e.what();

        // 5. 备选方案：如果 AudioMeta 创建失败，使用 SongMeta 创建简单的 AudioMeta
        AudioUtil::AudioMeta simpleAudioMeta;
        simpleAudioMeta.title = meta.name;
        simpleAudioMeta.artist = meta.singer;
        simpleAudioMeta.album = meta.album;
        simpleAudioMeta.filePath = correctedPath;
        simpleAudioMeta.id = meta.id;

        // 设置默认封面
        if (!meta.coverUrl.isEmpty() && meta.coverUrl != "qrc:/images/default_cover.png") {
            // 尝试加载封面
            simpleAudioMeta.cover.load(meta.coverUrl);
        }

        PlaylistManager* manager = PlaylistManager::getInstance();
        if (manager) {
            manager->addAudio(simpleAudioMeta);
            qDebug() << "[############Test] 备选方案:" << simpleAudioMeta.title << simpleAudioMeta.artist << simpleAudioMeta.album;
        }
    }

    // 6. 刷新UI
    PlaylistManager* manager = PlaylistManager::getInstance();
    if (manager) {
        const QList<AudioUtil::AudioMeta>& currentList = manager->getPlaylist();
        int newIndex = currentList.size() - 1;
        refreshList();
        qDebug() << "添加歌曲:" << meta.name << "，索引:" << newIndex;
    }

    qDebug() << "=== addAudioItem 结束 ===";
    }

    QString correctMusicPath(const QString& originalPath) {
        QFileInfo fileInfo(originalPath);

        // 如果已经是绝对路径且文件存在
        if (fileInfo.exists()) {
            return fileInfo.absoluteFilePath();
        }

        // 处理相对路径
        QStringList possiblePaths;

        // 原始路径
        possiblePaths << originalPath;

        // 修正：../music/ -> ../../music/
        if (originalPath.startsWith("../music/")) {
            possiblePaths << "../../" + originalPath.mid(3);
        }

        // 基于应用程序目录
        QString appDir = QCoreApplication::applicationDirPath();

        // 各种可能的相对路径组合
        possiblePaths << appDir + "/" + originalPath;
        possiblePaths << appDir + "/../" + originalPath;
        possiblePaths << appDir + "/../../" + originalPath;

        // 只使用文件名，在音乐目录中查找
        QString fileName = QFileInfo(originalPath).fileName();
        possiblePaths << appDir + "/music/" + fileName;
        possiblePaths << appDir + "/../music/" + fileName;
        possiblePaths << appDir + "/../../music/" + fileName;

        // 检查每个可能的路径
        for (const QString& path : possiblePaths) {
            QFileInfo testFile(path);
            if (testFile.exists()) {
                qDebug() << "找到有效路径:" << path;
                return testFile.absoluteFilePath();
            }
        }

        qWarning() << "无法找到音频文件，原始路径:" << originalPath;
        return QString();
    }

    void addSongToPlaylist(const QString& songId, const QString& sheetId) {
        qDebug() << "[AudioPlaylistPopup] addSongToPlaylist 被调用：songId=" << songId << ", sheetId=" << sheetId;
        onPlayListManagerRequest(songId, sheetId); // 内部调用私有槽
    }

    QListWidget * getList() const
    {
        return m_listWidget;
    }

    void onPlayListManagerRequest(const QString & m_id,const QString & m_Sheetid);

    void setTopLevelTransientParent(QWidget* topLevelWidget) {
        if (topLevelWidget && windowHandle()) {
            windowHandle()->setTransientParent(topLevelWidget->windowHandle());
            qDebug() << "[AudioPlaylistPopup] 已绑定顶层窗口关联（Wayland兼容）";
        }
    }

    void connectToMusicSheet(MusicSheetUI* musicSheetUi);

    void createContextMenu();

    void setCurrentSongPlaying(const QString& songId)
    {
        // 遍历所有list item
        for (int i = 0; i < m_listWidget->count(); ++i) {
            QListWidgetItem* item = m_listWidget->item(i);
            AudioItemWidget* widget = qobject_cast<AudioItemWidget*>(m_listWidget->itemWidget(item));

            if (widget) {
                // 如果歌曲ID匹配，标红；否则恢复原样
                bool isPlaying = (widget->getMeta().id == songId);
                widget->setPlaying(isPlaying);
            }
        }
    }

signals:
    void playRequested(const AudioMeta& meta); // 点击条目播放
    void closeRequested();                     // Popup关闭

protected:
    void showEvent(QShowEvent* event) override; // 显示时调整尺寸
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override {
        qDebug() << "[AudioPlaylistPopup] 关闭事件";
        hide();
        emit closeRequested();
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) {
        if (event->key() == Qt::Key_Escape) {
            qDebug() << "[AudioPlaylistPopup] ESC键按下，隐藏弹窗";
            hide();
            emit closeRequested();
            event->accept();
        } else {
            QWidget::keyPressEvent(event);
        }
    }
private slots:
    void onItemClicked(QListWidgetItem* item); // 列表项点击
    void onAddClicked();                       // 添加文件按钮
    void onClearClicked();                     // 清空按钮
    void onCloseClicked();                     // 标题栏关闭按钮（新增）

    void onCustomContextMenuRequested(const QPoint& pos);  // 右键菜单请求
    void onMenuPlayAction();                              // 菜单-播放
    void onMenuRemoveAction();                            // 菜单-删除

private:
    void initUI();                 // 初始化界面（网易云风格）
    void initConnections();        // 连接信号槽
    void checkScreenBounds();      // 确保不超出屏幕

private:
    QListWidget* m_listWidget = nullptr;   // 列表控件
    QPushButton* m_btnAdd = nullptr;       // 添加按钮
    QPushButton* m_btnClear = nullptr;     // 清空按钮
    QPushButton* m_btnClose = nullptr;     // 标题栏关闭按钮（新增）
    QLabel* m_titleLabel = nullptr;        // 标题栏文字（新增）
    QPoint m_popupPos;                     // 显示位置
    AudioMetaParser* m_parser;              // 元数据解析器
    const int FIXED_WIDTH = 300;           // 网易云风格：固定宽度420px

    bool installed = false;

    QMenu* m_contextMenu = nullptr;      // 右键菜单
    QAction* m_actionPlay = nullptr;     // 播放动作
    QAction* m_actionRemove = nullptr;   // 删除动作
    QListWidgetItem* m_contextMenuItem = nullptr; // 当前右键的item

    int CurrentPlayingIndex = -1;
};
#endif // RINZEPLAYER_AUDIOPLAYLISTWIDGET_H