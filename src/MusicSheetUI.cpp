//
// Created by lsy on 2025/10/24.
//

#include "../Header/Ui/MusicSheetUI.h"

#include <QPixmap>
#include <QToolTip>
#include <QTableWidgetItem>
#include <QHBoxLayout>
#include <QComboBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QLineEdit>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QAction>

#include <../Global/Global.h>

#include "Manager/MusicSheetManager.h"

using AudioUtil::MusicSheet;

MusicSheetUI::MusicSheetUI(QWidget *parent) : QWidget(parent)
{
    // 初始化所有控件指针为nullptr
    m_coverLabel = nullptr;
    m_tagLabel = nullptr;
    m_titleLabel = nullptr;
    m_songCountLabel = nullptr;
    m_playCountLabel = nullptr;
    m_descLabel = nullptr;
    m_playAllBtn = nullptr;
    m_shuffleBtn = nullptr;
    m_favBtn = nullptr;
    m_moreBtn = nullptr;
    m_songTable = nullptr;

    initUI();      // 初始化布局
    initStyle();   // 应用样式

    connect(m_moreBtn,&QPushButton::clicked,this,&MusicSheetUI::OnMoreBtnClicked);

    m_songTable->setContextMenuPolicy(Qt::CustomContextMenu); // 启用自定义右键菜单
    connect(
        m_songTable,
        &QTableWidget::customContextMenuRequested, // 右键点击时触发的信号
        this,
        &MusicSheetUI::onShowOperateMenu         // 响应的槽函数
    );

    connect(this,&MusicSheetUI::playSongById,this,&MusicSheetUI::onSongPlayedUpdateUI_ById);
}

// 歌单标签：setter/getter
void MusicSheetUI::setSheetTag(const QString& tag)
{
    if (m_tagLabel) m_tagLabel->setText(tag);
}

QString MusicSheetUI::getSheetTag() const
{
    return m_tagLabel ? m_tagLabel->text() : QString();
}

// 歌单标题：setter/getter
void MusicSheetUI::setSheetTitle(const QString& title)
{
    if (m_titleLabel) m_titleLabel->setText(title);
}

QString MusicSheetUI::getSheetTitle() const
{
    return m_titleLabel ? m_titleLabel->text() : QString();
}

// 歌曲数量：setter/getter
void MusicSheetUI::setSongCount(int count)
{
    if (m_songCountLabel)
        m_songCountLabel->setText(QString("[%1首]").arg(count));
}

int MusicSheetUI::getSongCount() const
{
    if (!m_songCountLabel) return 0;
    QString text = m_songCountLabel->text();
    return text.mid(1, text.indexOf("首") - 1).toInt();
}

// 播放量：setter/getter
void MusicSheetUI::setPlayCount(int count)
{
    if (!m_playCountLabel) return;
    QString suffix = count >= 10000 ? "万" : "";
    double displayCount = count >= 10000 ? count / 10000.0 : count;
    m_playCountLabel->setText(QString("[%1%2次播放]").arg(
        QString::number(displayCount, 'f', 1), suffix));
}

int MusicSheetUI::getPlayCount() const
{
    if (!m_playCountLabel) return 0;
    QString text = m_playCountLabel->text();
    text.remove("[").remove("次播放]");
    if (text.contains("万"))
        return text.remove("万").toDouble() * 10000;
    return text.toInt();
}

// 歌单描述：setter/getter
void MusicSheetUI::setSheetDesc(const QString& desc)
{
    if (m_descLabel) m_descLabel->setText(desc);
}

QString MusicSheetUI::getSheetDesc() const
{
    return m_descLabel ? m_descLabel->text() : QString();
}

// 歌单封面：setter/getter
void MusicSheetUI::setSheetCover(const QPixmap& cover)
{
    if (!m_coverLabel) return;
    m_sheetCover = cover;
    QPixmap scaledCover = cover.scaled(
        m_coverLabel->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );
    m_coverLabel->setPixmap(scaledCover);
}

QPixmap MusicSheetUI::getSheetCover() const
{
    return m_sheetCover;
}

// 添加单首歌曲
void MusicSheetUI::addSong(const SongMeta& song)
{
    m_songList.append(song);
    updateSongTable();
}

// 批量添加歌曲
void MusicSheetUI::addSongs(const QList<SongMeta>& songs)
{
    m_songList.append(songs);
    updateSongTable();
}

// 按ID移除歌曲
void MusicSheetUI::removeSong(const QString& songId)
{
    for (int i = 0; i < m_songList.size(); ++i)
    {
        if (m_songList[i].id == songId)
        {
            m_songList.removeAt(i);
            updateSongTable();
            break;
        }
    }
}

// 清空歌曲列表
void MusicSheetUI::clearSongs()
{
    m_songList.clear();
    if (m_songTable) m_songTable->setRowCount(0);
    setSongCount(0);
}

// 获取当前歌曲列表
QList<SongMeta> MusicSheetUI::getSongList() const
{
    return m_songList;
}

// 标记当前播放歌曲
void MusicSheetUI::setPlayingSong(const QString& songId)
{
    m_playingSongId = songId;
    updateSongTable();
}

// 获取当前播放歌曲ID
QString MusicSheetUI::getPlayingSongId() const
{
    return m_playingSongId;
}

// 初始化UI布局
void MusicSheetUI::initUI()
{
    // 主布局（保持不变）
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    mainLayout->setSpacing(25);

    // 1. 歌单头部（保持不变）
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(25);

    m_coverLabel = new QLabel();
    m_coverLabel->setFixedSize(200, 200);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setStyleSheet("background-color: #f0f0f0;");
    headerLayout->addWidget(m_coverLabel);

    QVBoxLayout* infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(12);
    infoLayout->setAlignment(Qt::AlignTop);

    m_tagLabel = new QLabel("歌单");
    infoLayout->addWidget(m_tagLabel);

    m_titleLabel = new QLabel("我的珍藏");
    infoLayout->addWidget(m_titleLabel);

    QHBoxLayout* metaLayout = new QHBoxLayout();
    metaLayout->setSpacing(15);
    m_songCountLabel = new QLabel("[30首]");
    m_playCountLabel = new QLabel("[25.8万次播放]");
    metaLayout->addWidget(m_songCountLabel);
    metaLayout->addWidget(m_playCountLabel);
    metaLayout->addStretch();
    infoLayout->addLayout(metaLayout);

    m_descLabel = new QLabel("收录经典曲目，适合深夜聆听，治愈疲惫心灵");
    m_descLabel->setWordWrap(true);
    m_descLabel->setMaximumWidth(500);
    infoLayout->addWidget(m_descLabel);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(12);
    m_playAllBtn = new QPushButton("播放全部");
    m_shuffleBtn = new QPushButton("随机");
    m_favBtn = new QPushButton("收藏");
    m_moreBtn = new QPushButton("更多");
    btnLayout->addWidget(m_playAllBtn);
    btnLayout->addWidget(m_shuffleBtn);
    btnLayout->addWidget(m_favBtn);
    btnLayout->addWidget(m_moreBtn);
    btnLayout->addStretch();
    infoLayout->addLayout(btnLayout);

    headerLayout->addLayout(infoLayout);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // 2. 歌曲列表区域（核心调整：#列宽度增至60px，行高60px）
    QVBoxLayout* listLayout = new QVBoxLayout();
    listLayout->setSpacing(15);

    QHBoxLayout* listHeaderLayout = new QHBoxLayout();
    listHeaderLayout->setSpacing(10);
    QLabel* listTitle = new QLabel("歌曲列表");
    listTitle->setObjectName("listTitle");
    listHeaderLayout->addWidget(listTitle);
    listHeaderLayout->addStretch();

    QHBoxLayout* listCtrlLayout = new QHBoxLayout();
    listCtrlLayout->setSpacing(10);
    QComboBox* sortCombo = new QComboBox();
    sortCombo->addItems({"默认排序", "播放量", "最新添加"});
    listCtrlLayout->addWidget(sortCombo);
    listHeaderLayout->addLayout(listCtrlLayout);
    listLayout->addLayout(listHeaderLayout);

    // 歌曲表格（#列宽度调整+行高设置）
    m_songTable = new QTableWidget();
    m_songTable->setColumnCount(5);
    m_songTable->setHorizontalHeaderLabels({"#", "标题", "专辑", "时长", "操作"});
    m_songTable->verticalHeader()->setVisible(false); // 隐藏原生行号
    // 列宽设置（#列加宽到60px，避免图标+序号拥挤）
    m_songTable->setColumnWidth(0, 70);    // #列（序号+播放图标）
    m_songTable->setColumnWidth(1, 400);   // 标题列
    m_songTable->setColumnWidth(2, 220);   // 专辑列
    m_songTable->setColumnWidth(3, 80);    // 时长列
    m_songTable->setColumnWidth(4, 60);    // 操作列
    // 行高设置（60px，确保文字不压缩）
    m_songTable->verticalHeader()->setDefaultSectionSize(60);
    m_songTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    listLayout->addWidget(m_songTable);

    mainLayout->addLayout(listLayout);
}

// 初始化QSS样式（灰白简约风格核心）
void MusicSheetUI::initStyle()
{
    // 全局QSS样式表
    setStyleSheet(R"(
        /* 主窗口背景 */
        MusicSheetUI {
            background-color: #f8f8f8;
        }

        /* 通用标签样式 */
        QLabel {
            color: #333;
            font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
        }

        /* 歌单标签（如"歌单"） */
        QLabel#m_tagLabel {
            color: #666;
            font-size: 12px;
            letter-spacing: 1px;
        }

        /* 歌单标题 */
        QLabel#m_titleLabel {
            font-size: 24px;
            font-weight: 500;
            color: #222;
        }

        /* 元数据标签（歌曲数量/播放量） */
        QLabel#m_songCountLabel, QLabel#m_playCountLabel {
            color: #666;
            font-size: 13px;
        }

        /* 描述文本 */
        QLabel#m_descLabel {
            color: #555;
            font-size: 14px;
            line-height: 1.6;
        }

        /* 列表标题 */
        QLabel#listTitle {
            font-size: 16px;
            font-weight: 500;
            color: #222;
        }

        /* 按钮通用样式 */
        QPushButton {
            border-radius: 4px;
            font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
            font-size: 14px;
            padding: 6px 12px;
        }

        /* 主按钮（播放全部） */
        QPushButton#m_playAllBtn {
            background-color: #333;
            color: white;
            border: none;
        }
        QPushButton#m_playAllBtn:hover {
            background-color: #555;
        }

        /* 次要按钮（随机/收藏/更多） */
        QPushButton#m_shuffleBtn, QPushButton#m_favBtn, QPushButton#m_moreBtn {
            background-color: #fff;
            color: #555;
            border: 1px solid #ddd;
        }
        QPushButton#m_shuffleBtn:hover, QPushButton#m_favBtn:hover, QPushButton#m_moreBtn:hover {
            background-color: #f0f0f0;
        }

        /* 表格整体样式 */
        QTableWidget {
            background-color: white;
            border-radius: 6px;
            border: 1px solid #eee;
            gridline-color: #f0f0f0;
        }

        /* 表格表头 */
        QHeaderView::section {
            background-color: #f5f5f5;
            color: #666;
            font-size: 13px;
            padding: 8px;
            border: none;
            border-bottom: 1px solid #eee;
        }

        /* 表格单元格 */
        QTableWidget::item {
            color: #333;
            font-size: 13px;
            padding: 5px;
            border: none;
        }

        /* 表格内操作按钮 */
        QPushButton#tableMoreBtn {
            color: #999;
            background: transparent;
            border: none;
            padding: 0;
            font-size: 16px;
        }
        QPushButton#tableMoreBtn:hover {
            color: #555;
        }
    )");

    // 设置控件objectName用于QSS定位
    m_tagLabel->setObjectName("m_tagLabel");
    m_titleLabel->setObjectName("m_titleLabel");
    m_songCountLabel->setObjectName("m_songCountLabel");
    m_playCountLabel->setObjectName("m_playCountLabel");
    m_descLabel->setObjectName("m_descLabel");
    m_playAllBtn->setObjectName("m_playAllBtn");
    m_shuffleBtn->setObjectName("m_shuffleBtn");
    m_favBtn->setObjectName("m_favBtn");
    m_moreBtn->setObjectName("m_moreBtn");
}

// 更新歌曲表格显示
void MusicSheetUI::updateSongTable()
{
    qDebug() << "=== updateSongTable 开始 ===";
    qDebug() << "歌曲列表大小:" << m_songList.size();

    if (!m_songTable) {
        qDebug() << "错误: 歌曲表格指针为空";
        return;
    }

    m_songTable->clearContents();
    m_songTable->setRowCount(m_songList.size());

    for (int row = 0; row < m_songList.size(); ++row)
    {
        const SongMeta& song = m_songList[row];

        // 1. #列：左侧圆圈 + 序号
        QLabel* indexLabel = new QLabel();
        indexLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        indexLabel->setStyleSheet("font-size: 14px; color: #333;");

        QString circleStyle;
        if (song.id == m_playingSongId)
        {
            circleStyle = "<span style='color: #4CAF50;'>●</span>";
        }
        else
        {
            circleStyle = "<span style='color: #ccc;'>●</span>";
        }

        QString indexText = QString("%1  %2").arg(circleStyle).arg(row + 1);
        indexLabel->setText(indexText);
        m_songTable->setCellWidget(row, 0, indexLabel);

        // 2. 标题列 - 使用 song.name
        QString displayName = song.name;

        qDebug() << "#MusicSheetUi :" << displayName;

        if (displayName.isEmpty()) {
            displayName = "未知歌曲";
        }
        QTableWidgetItem* nameItem = new QTableWidgetItem(displayName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setForeground(QBrush(QColor("#333333")));
        m_songTable->setItem(row, 1, nameItem);

        // 3. 专辑列
        QString displayAlbum = song.album;
        if (displayAlbum.isEmpty()) {
            displayAlbum = "未知专辑";
        }
        QTableWidgetItem* albumItem = new QTableWidgetItem(displayAlbum);
        albumItem->setFlags(albumItem->flags() & ~Qt::ItemIsEditable);
        albumItem->setForeground(QBrush(QColor("#666666")));
        m_songTable->setItem(row, 2, albumItem);

        // 4. 时长列
        QString displayDuration = song.duration;
        if (displayDuration.isEmpty()) {
            displayDuration = "--:--";
        }
        QTableWidgetItem* durationItem = new QTableWidgetItem(displayDuration);
        durationItem->setFlags(durationItem->flags() & ~Qt::ItemIsEditable);
        durationItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        durationItem->setForeground(QBrush(QColor("#666666")));
        m_songTable->setItem(row, 3, durationItem);

        // 5. 操作列
        QTableWidgetItem* actionItem = new QTableWidgetItem("···");
        actionItem->setFlags(actionItem->flags() & ~Qt::ItemIsEditable);
        actionItem->setTextAlignment(Qt::AlignCenter);
        actionItem->setForeground(QBrush(QColor("#999999")));
        m_songTable->setItem(row, 4, actionItem);

        // 设置行背景色
        QColor rowColor = (song.id == m_playingSongId) ? QColor(240, 240, 240) : Qt::white;
        for (int col = 0; col < 5; ++col) {
            QTableWidgetItem* item = m_songTable->item(row, col);
            if (item) {
                item->setBackground(rowColor);
            }
        }
    }

    setSongCount(m_songList.size());
    qDebug() << "歌曲表格更新完成，共" << m_songList.size() << "首歌曲";
}

bool MusicSheetUI::insertInfoBySheet(const MusicSheet& sheet) {
    // ==================== 1. 数据校验与预处理 ====================
    // 校验标题（核心字段）
    if (sheet.title.trimmed().isEmpty()) {
        qWarning() << "歌单标题为空，设置失败";
        return false;
    }

    m_musicSheet = sheet;

    // 修正歌曲数量（确保非负）
    int validSongCount = qMax(0, sheet.songCount);
    // 修正播放量（确保非负）
    int validPlayCount = qMax(0, sheet.playCount);

    // 处理封面图（为空时用默认图）
    QPixmap displayCover = sheet.cover;
    if (displayCover.isNull()) {
        displayCover = QPixmap(":/covers/default_cover.png"); // 默认封面资源
        qDebug() << "歌单封面为空，使用默认图";
    }


    // ==================== 2. 更新UI组件数据 ====================
    // 2.1 基础信息（ID、封面缓存）
    MusicSheetID = sheet.id; // 保存唯一ID
    m_sheetCover = displayCover; // 缓存封面图（供getter使用）

    // 2.2 封面图（固定200x200，保持比例）
    m_coverLabel->setPixmap(displayCover.scaled(
        200, 200,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation // 平滑缩放
    ));

    // 2.3 标签（如"歌单"）
    m_tagLabel->setText(sheet.tag);
    m_tagLabel->setStyleSheet("color: #666; font-size: 12px;"); // 灰色小字

    // 2.4 主标题
    m_titleLabel->setText(sheet.title);
    m_titleLabel->setStyleSheet("font-size: 22px; font-weight: bold; color: #333;"); // 加粗标题

    // 2.5 歌曲数量（格式："30首歌曲"）
    m_songCountLabel->setText(QString("%1首歌曲").arg(validSongCount));
    m_songCountLabel->setStyleSheet("color: #666; font-size: 14px;");

    // 2.6 播放量（格式化：千/万单位）
    QString playCountText;
    if (validPlayCount >= 10000) {
        playCountText = QString::number(validPlayCount / 10000.0, 'f', 1) + "万次播放";
    } else if (validPlayCount >= 1000) {
        playCountText = QString::number(validPlayCount / 1000.0, 'f', 1) + "千次播放";
    } else {
        playCountText = QString("%1次播放").arg(validPlayCount);
    }
    m_playCountLabel->setText(playCountText);
    m_playCountLabel->setStyleSheet("color: #666; font-size: 14px;");

    // 2.7 描述文本（为空时显示默认提示）
    if (sheet.desc.trimmed().isEmpty()) {
        m_descLabel->setText("暂无描述");
        m_descLabel->setStyleSheet("color: #999; font-size: 13px;"); // 灰色提示
    } else {
        m_descLabel->setText(sheet.desc);
        m_descLabel->setStyleSheet("color: #333; font-size: 13px; line-height: 1.5;"); // 正常文本
    }

    // 2.8 按钮状态（可选：根据数据更新，如无歌曲则禁用播放按钮）
    bool hasSongs = !sheet.songs.isEmpty();
    m_playAllBtn->setEnabled(hasSongs);
    m_shuffleBtn->setEnabled(hasSongs);
    m_playAllBtn->setToolTip(hasSongs ? "播放全部歌曲" : "暂无歌曲可播放");
    m_shuffleBtn->setToolTip(hasSongs ? "随机播放" : "暂无歌曲可播放");


    // ==================== 3. 更新歌曲表格数据 ====================
    m_songList = sheet.songs; // 缓存歌曲列表到成员变量
    updateSongTable(); // 同步到表格UI


    return true;
}

void MusicSheetUI::OnMoreBtnClicked()
{
    // 创建编辑对话框
    QDialog* editDialog = new QDialog(this);
    editDialog->setWindowTitle("编辑歌单信息");
    editDialog->setFixedSize(550, 650);
    editDialog->setAttribute(Qt::WA_DeleteOnClose);

    // 保持原有样式不变
    editDialog->setStyleSheet(R"(
        QDialog {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #f8f9fa, stop:1 #e9ecef);
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
        }
        QGroupBox {
            font-weight: bold;
            font-size: 14px;
            color: #495057;
            border: 2px solid #dee2e6;
            border-radius: 8px;
            margin-top: 1ex;
            padding-top: 10px;
            background: white;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 8px 0 8px;
            color: #6c757d;
        }
        QLabel {
            color: #495057;
            font-size: 13px;
        }
        QLineEdit, QPlainTextEdit {
            border: 2px solid #e9ecef;
            border-radius: 6px;
            padding: 8px;
            font-size: 13px;
            background: white;
            selection-background-color: #4dabf7;
        }
        QLineEdit:focus, QPlainTextEdit:focus {
            border-color: #4dabf7;
            background: #f8f9fa;
        }
        QLineEdit {
            height: 24px;
        }
        QPlainTextEdit {
            min-height: 80px;
        }
        QPushButton {
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: bold;
            font-size: 13px;
            min-width: 80px;
            transition: all 0.2s;
        }
        QPushButton:hover {
            transform: translateY(-1px);
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        QPushButton:pressed {
            transform: translateY(0);
        }
    )");

    // 创建主布局（优化间距和边距）
    QVBoxLayout* mainLayout = new QVBoxLayout(editDialog);
    mainLayout->setSpacing(15);  // 减小主布局间距
    mainLayout->setContentsMargins(20, 20, 20, 20);  // 减小外边框

    // 标题栏（优化底部间距）
    QLabel* titleLabel = new QLabel("编辑歌单信息", editDialog);
    titleLabel->setStyleSheet(R"(
        QLabel {
            font-size: 18px;
            font-weight: bold;
            color: #343a40;
            padding: 10px;
            border-bottom: 2px solid #4dabf7;
            margin-bottom: 5px;  // 减小底部距离
        }
    )");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setMinimumHeight(40);

    // 封面选择区域（优化内部布局）
    QGroupBox* coverGroup = new QGroupBox(" 歌单封面", editDialog);
    QHBoxLayout* coverLayout = new QHBoxLayout(coverGroup);
    coverLayout->setSpacing(20);  // 减小封面区域内部间距
    coverLayout->setContentsMargins(20, 20, 20, 20);  // 减小分组框内边距

    QLabel* coverLabel = new QLabel(coverGroup);
    coverLabel->setFixedSize(180, 180);  // 固定尺寸避免拉伸
    coverLabel->setStyleSheet(R"(
        QLabel {
            border: 3px dashed #dee2e6;
            border-radius: 8px;
            background: #f8f9fa;
            color: #6c757d;
        }
    )");
    coverLabel->setAlignment(Qt::AlignCenter);

    // 封面按钮区域（垂直居中对齐）
    QVBoxLayout* coverBtnLayout = new QVBoxLayout();
    coverBtnLayout->setSpacing(12);  // 减小按钮间距
    coverBtnLayout->setAlignment(Qt::AlignVCenter);  // 垂直居中对齐

    QPushButton* coverSelectBtn = new QPushButton(" 选择图片", coverGroup);
    QPushButton* coverClearBtn = new QPushButton(" 清除封面", coverGroup);

    coverSelectBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #4dabf7, stop:1 #339af0);
            color: white;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #339af0, stop:1 #228be6);
        }
    )");

    coverClearBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #fa5252, stop:1 #e03131);
            color: white;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #e03131, stop:1 #c92a2a);
        }
    )");

    coverBtnLayout->addWidget(coverSelectBtn);
    coverBtnLayout->addWidget(coverClearBtn);

    // 封面区域布局调整（增加对称空白）
    coverLayout->addSpacing(10);  // 左侧留白
    coverLayout->addWidget(coverLabel);
    coverLayout->addLayout(coverBtnLayout);
    coverLayout->addSpacing(10);  // 右侧留白

    // 基本信息区域（优化表单布局）
    QGroupBox* infoGroup = new QGroupBox(" 基本信息", editDialog);
    QFormLayout* formLayout = new QFormLayout(infoGroup);
    formLayout->setSpacing(15);  // 减小表单间距
    formLayout->setContentsMargins(20, 20, 20, 20);  // 减小内边距
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);  // 标签垂直居中
    formLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);  // 禁止自动换行

    QLineEdit* tagEdit = new QLineEdit(infoGroup);
    QLineEdit* titleEdit = new QLineEdit(infoGroup);
    QPlainTextEdit* descEdit = new QPlainTextEdit(infoGroup);

    // 设置输入框固定宽度
    tagEdit->setMinimumWidth(300);
    titleEdit->setMinimumWidth(300);
    descEdit->setMinimumWidth(300);
    descEdit->setMinimumHeight(80);  // 增加描述框高度

    // 设置占位符文本
    tagEdit->setPlaceholderText("例如：流行、摇滚、我的最爱...");
    titleEdit->setPlaceholderText("请输入歌单标题");
    descEdit->setPlaceholderText("描述这个歌单的特点、风格或背后的故事...");

    // 设置当前值
    tagEdit->setText(getSheetTag());
    titleEdit->setText(getSheetTitle());
    descEdit->setPlainText(getSheetDesc());

    // 创建带图标的标签（统一宽度）
    auto createLabel = [](const QString& text, const QString& icon) -> QLabel* {
        QLabel* label = new QLabel(QString("%1 %2").arg(icon).arg(text));
        label->setStyleSheet("font-weight: bold; color: #495057;");
        label->setFixedWidth(80);  // 固定标签宽度，确保对齐
        return label;
    };

    formLayout->addRow(createLabel("标签", ""), tagEdit);
    formLayout->addRow(createLabel("标题", ""), titleEdit);

    // 描述区域单独处理
    QLabel* descLabel = createLabel("描述", "");
    formLayout->addRow(descLabel, descEdit);

    // 按钮区域（优化对齐和间距）
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(15);  // 减小按钮间距
    buttonLayout->setContentsMargins(0, 10, 0, 10);  // 增加上下边距

    QPushButton* saveBtn = new QPushButton(" 保存", editDialog);
    QPushButton* cancelBtn = new QPushButton(" 取消", editDialog);

    // 统一按钮尺寸
    saveBtn->setMinimumWidth(100);
    cancelBtn->setMinimumWidth(100);

    saveBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #51cf66, stop:1 #40c057);
            color: white;
            padding: 10px 20px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #40c057, stop:1 #37b24d);
        }
        QPushButton:disabled {
            background: #adb5bd;
            color: #868e96;
        }
    )");

    cancelBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #868e96, stop:1 #495057);
            color: white;
            padding: 10px 20px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #495057, stop:1 #343a40);
        }
    )");

    buttonLayout->addStretch();
    buttonLayout->addWidget(saveBtn);
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addStretch();  // 两侧拉伸，使按钮居中

    // 组装主布局（增加合理间隔）
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(coverGroup);
    mainLayout->addWidget(infoGroup);
    mainLayout->addStretch(1);  // 增加弹性空间
    mainLayout->addLayout(buttonLayout);

    // 存储当前选择的封面
    QPixmap selectedCover = getSheetCover();

    // 更新封面显示的函数
    auto updateCoverDisplay = [coverLabel](const QPixmap& cover) {
        if (!cover.isNull()) {
            coverLabel->setPixmap(cover.scaled(174, 174, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            coverLabel->setText("");
        } else {
            coverLabel->setPixmap(QPixmap());
            coverLabel->setText("暂无封面");
            coverLabel->setStyleSheet(coverLabel->styleSheet() + "color: #6c757d; font-size: 12px;");
        }
    };

    updateCoverDisplay(selectedCover);

    // 选择封面按钮
    connect(coverSelectBtn, &QPushButton::clicked, editDialog, [coverLabel, &selectedCover, updateCoverDisplay,this]() {
        QString fileName = QFileDialog::getOpenFileName(coverLabel,
            "选择歌单封面", "", "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)");

        if (!fileName.isEmpty()) {
            m_coverPath = fileName;
            QPixmap cover(fileName);
            if (!cover.isNull()) {
                selectedCover = cover;
                updateCoverDisplay(selectedCover);
                QToolTip::showText(coverLabel->mapToGlobal(QPoint(0, 0)),
                                  "封面设置成功！", coverLabel, QRect(), 2000);
            }
        }
    });

    // 清除封面按钮
    connect(coverClearBtn, &QPushButton::clicked, editDialog, [&selectedCover, updateCoverDisplay]() {
        selectedCover = QPixmap();
        updateCoverDisplay(selectedCover);
    });

    // 实时验证标题
    connect(titleEdit, &QLineEdit::textChanged, editDialog, [saveBtn, titleEdit]() {
        bool isValid = !titleEdit->text().trimmed().isEmpty();
        saveBtn->setEnabled(isValid);

        if (isValid) {
            titleEdit->setStyleSheet("border: 2px solid #51cf66; border-radius: 6px; padding: 8px; background: white;");
        } else {
            titleEdit->setStyleSheet("border: 2px solid #fa5252; border-radius: 6px; padding: 8px; background: white;");
        }
    });

    // 保存按钮 - 添加保存到单例管理类的逻辑
    connect(saveBtn, &QPushButton::clicked, editDialog, [this, editDialog, tagEdit, titleEdit, descEdit, &selectedCover]() {
        if (titleEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(editDialog, "提示", " 歌单标题不能为空！");
            titleEdit->setFocus();
            return;
        }

        // 更新UI显示的歌单信息
        setSheetTag(tagEdit->text());
        setSheetTitle(titleEdit->text());
        setSheetDesc(descEdit->toPlainText());
        setSheetCover(selectedCover);

        MusicSheet updatedSheet = m_musicSheet; // 复制原歌单所有字段
        // 仅更新被编辑的元信息
        updatedSheet.id = MusicSheetID;                     //ID
        updatedSheet.tag = tagEdit->text().trimmed();       // 标签
        updatedSheet.title = titleEdit->text().trimmed();   // 标题
        updatedSheet.desc = descEdit->toPlainText().trimmed(); // 描述
        updatedSheet.cover = selectedCover;                 // 封面图片
        updatedSheet.coverPath = m_coverPath;             // 封面路径（选择时已记录）

        // 调用单例管理类保存歌单信息到本地
        bool saveSuccess = false;

        // 假设您的单例管理类名为 MusicSheetManager，并且有静态实例方法
        // 请根据实际情况修改类名和方法名
        if (auto* manager = MusicSheetManager::getInstance()) {
            qDebug() << "进行本地存储" << "ID:" << MusicSheetID;
            manager->changeSheetById(updatedSheet,updatedSheet.id);
            emit item_stateChanged(updatedSheet.id,updatedSheet.title);
            saveSuccess = manager->saveSheetsToLocalById(this->MusicSheetID);
        }

        if (saveSuccess) {
            QMessageBox::information(editDialog, "成功", " 歌单信息已更新并保存！");
            editDialog->accept();
        } else {
            QMessageBox::warning(editDialog, "警告", " 歌单信息更新成功，但保存到本地失败！");
            // 可以选择是否仍然关闭对话框
            // editDialog->accept(); // 如果希望即使保存失败也关闭对话框
            // 或者保持对话框打开让用户重试
            // editDialog->reject(); // 如果希望保存失败时不关闭对话框
        }
    });

    // 取消按钮
    connect(cancelBtn, &QPushButton::clicked, editDialog, &QDialog::reject);

    // 初始验证
    saveBtn->setEnabled(!titleEdit->text().trimmed().isEmpty());

    // 显示对话框
    editDialog->exec();
}

void MusicSheetUI::onShowOperateMenu(const QPoint& pos) {
    QModelIndex index = m_songTable->indexAt(pos);
    if (!index.isValid() || index.column() != 4) {
        qDebug() << "右键点击位置无效或不在操作列";
        return;
    }

    int clickedRow = index.row();
    qDebug() << "右键点击行:" << clickedRow;

    // 检查歌曲列表边界
    if (clickedRow < 0 || clickedRow >= m_songList.size()) {
        qDebug() << "错误: 歌曲索引越界";
        return;
    }

    QString songId = m_songList[clickedRow].id;
    QString songName = m_songTable->item(clickedRow, 1)->text();

    // 如果表格中名称为空，尝试从歌曲数据中获取
    if (songName.isEmpty()) {
        songName = m_songList[clickedRow].name;
        qDebug() << "从歌曲数据中获取名称:" << songName;
    }

    qDebug() << "处理歌曲:" << songName << "ID:" << songId;

    // --------------------------
    // 1. 创建主菜单 + 应用样式
    // --------------------------
    QMenu menu(this);
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
    menu.setStyleSheet(menuQSS);

    // --------------------------
    // 2. 添加菜单项
    // --------------------------
    // 常规菜单项
    QAction *actPlay = menu.addAction("播放");
    QAction *actNextPlay = menu.addAction("下一首播放");
    QAction *actCollect = menu.addAction("收藏到我喜欢");

    // 新增：「收藏到歌单」子菜单 - 动态加载左侧歌单列表
    QMenu *subMenuCollectToPlaylist = menu.addMenu("收藏到歌单");

    // 动态获取所有歌单（排除当前歌单）
    MusicSheetManager* manager = MusicSheetManager::getInstance();
    auto allSheetInfos = manager->getAllSheetInfos();

    QList<QAction*> sheetActions;
    for (const auto& sheetInfo : allSheetInfos) {
        QString sheetId = sheetInfo.first;
        QString sheetTitle = sheetInfo.second;

        // 排除当前歌单（不添加到自身）
        if (sheetId == MusicSheetID) {
            continue;
        }

        QAction* sheetAction = subMenuCollectToPlaylist->addAction(sheetTitle);
        sheetAction->setData(sheetId); // 存储歌单ID
        sheetActions.append(sheetAction);

        qDebug() << "添加歌单选项:" << sheetTitle << "ID:" << sheetId;
    }

    // 如果没有其他歌单，显示提示
    if (sheetActions.isEmpty()) {
        QAction* noSheetAction = subMenuCollectToPlaylist->addAction("暂无其他歌单");
        noSheetAction->setEnabled(false);
    }

    // 分隔线
    menu.addSeparator();

    // 删除选项（只在当前歌单中显示）
    QAction *actDelete = menu.addAction("从当前歌单删除");

    // --------------------------
    // 3. 显示菜单 + 处理选择逻辑
    // --------------------------
    QAction *selectedAction = menu.exec(m_songTable->mapToGlobal(pos));
    if (!selectedAction) {
        qDebug() << "用户取消选择";
        return;
    }

    QString actionText = selectedAction->text();
    qDebug() << "歌单操作：" << songName << "-" << actionText;

    // 常规菜单项逻辑
    if (actionText == "播放") {
        qDebug() << "开始播放歌曲：" << songName << "（ID：" << songId << "）";
        emit playSongById(songId, MusicSheetID);
    } else if (actionText == "下一首播放") {
        qDebug() << "将歌曲「" << songName << "」添加到下一首播放队列";
        // TODO: 调用播放队列接口
        QMessageBox::information(this, "提示", QString("已将「%1」添加到下一首播放").arg(songName));

    } else if (actionText == "收藏到我喜欢") {
        qDebug() << "收藏歌曲「" << songName << "」到我喜欢";
        SongMeta songToAdd = m_songList[clickedRow];

        // 发射信号，添加到"我喜欢的音乐"歌单（ID: 0000）
        emit requestAddSongToOtherSheet(QString("0000"), songToAdd);
        // TODO: 调用个人收藏接口
        QMessageBox::information(this, "提示", QString("已将「%1」添加到「我喜欢的音乐」").arg(songName));

    } else if (actionText == "从当前歌单删除") {
        qDebug() << "从当前歌单删除歌曲：" << songName;

        // 确认对话框
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "确认删除",
                                    QString("确定要从歌单中删除「%1」吗？").arg(songName),
                                    QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            qDebug() << "=== 准备发射删除信号 ===";
            qDebug() << "信号: requestRemoveSongFromSheet";
            qDebug() << "歌单ID:" << MusicSheetID;
            qDebug() << "歌曲ID:" << songId;

            // 发射删除信号
            emit requestRemoveSongFromSheet(MusicSheetID, songId);

            qDebug() << "=== 删除信号发射完成 ===";
        }

    } else {
        // 处理「收藏到歌单」子菜单的选择
        QString targetSheetId = selectedAction->data().toString();
        QString targetSheetTitle = selectedAction->text();

        if (!targetSheetId.isEmpty()) {
            qDebug() << "收藏歌曲「" << songName << "」到歌单：" << targetSheetTitle << "(" << targetSheetId << ")";

            // 获取当前行的完整歌曲信息
            SongMeta songToAdd = m_songList[clickedRow];
            qDebug() << "===************** 准备发射信号 ***************===";
            qDebug() << "信号: requestAddSongToOtherSheet";
            qDebug() << "目标歌单: " << targetSheetId;
            qDebug() << "歌曲: " << songToAdd.name << "ID:" << songToAdd.id;
            qDebug() << "发射对象地址: " << this;

            // 发射信号，让MusicSheetManager处理添加歌曲到目标歌单
            emit requestAddSongToOtherSheet(targetSheetId, songToAdd);
            qDebug() << "完成";
            // 显示提示
            QMessageBox::information(this, "成功",
                QString("已将「%1」添加到歌单「%2」").arg(songName).arg(targetSheetTitle));
        }
    }
}

void MusicSheetUI::onSongPlayedUpdateUI_ById(const QString& songId)
{
    for (const auto& songInfo: m_songList)
    {
        if (songInfo.id == songId)
        {
            m_playingSongId = songId;
            updateSongTable();
            qDebug() << "当前播放歌曲ID: " << songId;
            return;
        }
    }
    qDebug() << "歌单"<< MusicSheetID << "中未发现该歌曲";
}

void MusicSheetUI::setSheetId(const QString& id)
{
    MusicSheetID = id;
}

QString MusicSheetUI::getSheetId() const
{
    return MusicSheetID;
}

void MusicSheetUI::updatePlayingState(const QString& targetSheetId, const QString& playingSongId)
{
    if (MusicSheetID == targetSheetId) {
        m_playingSongId = playingSongId;
    } else {
        m_playingSongId = ""; // 清空其他歌单的播放状态
    }
    updateSongTable(); // 刷新表格以更新指示灯
}
