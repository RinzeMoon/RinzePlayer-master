#include "../Header/Ui/MusicPlayerWidget.h"
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDir>
#include <algorithm>
#include <cmath>
#include <QResizeEvent>

// 静态辅助函数：创建圆角矩形图片
static QPixmap createRoundedPixmap(const QPixmap &source, int radius)
{
    if (source.isNull()) return QPixmap();

    QPixmap result(source.size());
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;
    path.addRoundedRect(0, 0, source.width(), source.height(), radius, radius);
    painter.setClipPath(path);

    painter.drawPixmap(0, 0, source);

    return result;
}

MusicPlayerWidget::MusicPlayerWidget(QWidget *parent)
    : QWidget(parent)
    , m_uiUpdateTimer(new QTimer(this))
    , m_clock(new Clock(this))
    , m_lyricParser(new LyricParser(this))
{
    // 设置毛玻璃效果相关属性
    setAutoFillBackground(true);
    setAttribute(Qt::WA_NoSystemBackground, false);

    // 初始化UI
    initUI();
    initConnections();
    initClock();

    // 设置UI更新定时器
    m_uiUpdateTimer->setInterval(100); // 10FPS
    connect(m_uiUpdateTimer, &QTimer::timeout, this, &MusicPlayerWidget::updateUIFromClock);
}

MusicPlayerWidget::~MusicPlayerWidget()
{
    if (m_blurEffect) delete m_blurEffect;
}

void MusicPlayerWidget::initClock()
{
    // 连接时钟信号
    connect(m_clock, &Clock::timeChanged, this, &MusicPlayerWidget::onClockTimeChanged);
}

// ==================== 初始化函数实现 ====================
void MusicPlayerWidget::initUI()
{
    // 米白色主题基础样式
    setStyleSheet(R"(
        QWidget {
            font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif;
            background-color: #FAF9F6;
        }
    )");

    // 1. 主布局（左右分栏）
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(40, 40, 40, 40);
    mainLayout->setSpacing(50);
    mainLayout->setAlignment(Qt::AlignCenter);

    // 2. 左侧：专辑封面区域
    m_leftFrame = new QFrame(this);
    m_leftFrame->setObjectName("leftFrame");
    m_leftFrame->setStyleSheet(R"(
        QFrame#leftFrame {
            background-color: rgba(255, 253, 250, 180);
            border-radius: 12px;
            border: 1px solid rgba(232, 228, 220, 150);
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.08);
        }
    )");
    m_leftFrame->setFixedSize(360, 360);
    m_leftFrame->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QVBoxLayout *leftLayout = new QVBoxLayout(m_leftFrame);
    leftLayout->setContentsMargins(40, 40, 40, 40);
    leftLayout->setSpacing(0);
    leftLayout->setAlignment(Qt::AlignCenter);

    // 2.1 专辑封面容器（矩形圆角）
    QWidget *coverContainer = new QWidget(m_leftFrame);
    coverContainer->setObjectName("coverContainer");
    coverContainer->setFixedSize(280, 280);
    coverContainer->setStyleSheet(R"(
        QWidget#coverContainer {
            background: qradialgradient(cx:0.5, cy:0.5, radius: 0.7,
                                        fx:0.5, fy:0.5,
                                        stop:0 rgba(255, 255, 255, 200),
                                        stop:1 rgba(245, 242, 237, 150));
            border-radius: 8px;
            border: 2px solid rgba(232, 228, 220, 200);
            box-shadow: 0 8px 32px rgba(200, 196, 188, 0.3),
                        inset 0 1px 0 rgba(255, 255, 255, 0.6);
        }
    )");

    QVBoxLayout *coverLayout = new QVBoxLayout(coverContainer);
    coverLayout->setContentsMargins(6, 6, 6, 6);
    coverLayout->setAlignment(Qt::AlignCenter);

    // 实际封面标签
    m_coverLabel = new QLabel(coverContainer);
    m_coverLabel->setObjectName("coverLabel");
    m_coverLabel->setFixedSize(268, 268);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setStyleSheet(R"(
        QLabel#coverLabel {
            border-radius: 6px;
            border: none;
            background: transparent;
        }
    )");
    coverLayout->addWidget(m_coverLabel);

    // 封面占位符（用于无封面时显示）
    m_coverPlaceholder = new QLabel(coverContainer);
    m_coverPlaceholder->setObjectName("coverPlaceholder");
    m_coverPlaceholder->setFixedSize(268, 268);
    m_coverPlaceholder->setAlignment(Qt::AlignCenter);
    m_coverPlaceholder->setPixmap(QPixmap(":/icons/music_note.svg").scaled(
        120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_coverPlaceholder->setStyleSheet(R"(
        QLabel#coverPlaceholder {
            border-radius: 6px;
            border: none;
            background: rgba(250, 249, 246, 0.8);
        }
    )");
    coverLayout->addWidget(m_coverPlaceholder);

    leftLayout->addWidget(coverContainer);
    mainLayout->addWidget(m_leftFrame, 0, Qt::AlignCenter);

    // 3. 右侧：信息+歌词区域
    m_rightFrame = new QFrame(this);
    m_rightFrame->setObjectName("rightFrame");
    m_rightFrame->setStyleSheet("QFrame#rightFrame { background-color: transparent; }");
    m_rightFrame->setMinimumWidth(500);

    QVBoxLayout *rightLayout = new QVBoxLayout(m_rightFrame);
    rightLayout->setContentsMargins(20, 20, 20, 20);
    rightLayout->setSpacing(40);
    mainLayout->addWidget(m_rightFrame, 1);

    // 3.1 歌曲信息区
    QFrame *infoFrame = new QFrame(m_rightFrame);
    infoFrame->setObjectName("infoFrame");
    infoFrame->setStyleSheet("QFrame#infoFrame { background-color: transparent; }");

    QVBoxLayout *infoLayout = new QVBoxLayout(infoFrame);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(10);

    // 歌曲标题
    m_titleLabel = new QLabel(tr("未选择歌曲"), infoFrame);
    m_titleLabel->setObjectName("titleLabel");
    QFont titleFont = QFont("Microsoft YaHei", 22, QFont::Bold);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet(R"(
        QLabel#titleLabel {
            color: #3C3A36;
            background: transparent;
            padding: 0;
            margin: 0;
        }
    )");
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    infoLayout->addWidget(m_titleLabel);

    // 歌手名
    m_artistLabel = new QLabel(tr("未知歌手"), infoFrame);
    m_artistLabel->setObjectName("artistLabel");
    QFont artistFont = QFont("Microsoft YaHei", 15, QFont::Normal);
    m_artistLabel->setFont(artistFont);
    m_artistLabel->setStyleSheet(R"(
        QLabel#artistLabel {
            color: #C86B5A;
            background: transparent;
            padding: 0;
            margin: 0;
        }
    )");
    m_artistLabel->setWordWrap(true);
    m_artistLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    infoLayout->addWidget(m_artistLabel);

    // 专辑名
    m_albumLabel = new QLabel(tr("未知专辑"), infoFrame);
    m_albumLabel->setObjectName("albumLabel");
    QFont albumFont = QFont("Microsoft YaHei", 13, QFont::Normal);
    m_albumLabel->setFont(albumFont);
    m_albumLabel->setStyleSheet(R"(
        QLabel#albumLabel {
            color: #7A756E;
            background: transparent;
            padding: 0;
            margin: 0;
        }
    )");
    m_albumLabel->setWordWrap(true);
    m_albumLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    infoLayout->addWidget(m_albumLabel);

    // 时间信息布局
    QHBoxLayout *timeLayout = new QHBoxLayout();
    timeLayout->setContentsMargins(0, 0, 0, 0);
    timeLayout->setSpacing(10);

    // 当前时间
    m_currentTimeLabel = new QLabel("00:00", infoFrame);
    m_currentTimeLabel->setObjectName("currentTimeLabel");
    m_currentTimeLabel->setStyleSheet(R"(
        QLabel#currentTimeLabel {
            color: #C86B5A;
            font-size: 13px;
            font-weight: bold;
            background: transparent;
        }
    )");
    m_currentTimeLabel->setFixedWidth(50);

    // 分隔符
    QLabel *separatorLabel = new QLabel("/", infoFrame);
    separatorLabel->setObjectName("separatorLabel");
    separatorLabel->setStyleSheet(R"(
        QLabel#separatorLabel {
            color: #7A756E;
            font-size: 13px;
            background: transparent;
        }
    )");

    // 歌曲时长
    m_durationLabel = new QLabel("00:00", infoFrame);
    m_durationLabel->setObjectName("durationLabel");
    m_durationLabel->setStyleSheet(R"(
        QLabel#durationLabel {
            color: #7A756E;
            font-size: 13px;
            font-weight: 500;
            background: transparent;
        }
    )");
    m_durationLabel->setFixedWidth(50);

    timeLayout->addWidget(m_currentTimeLabel);
    timeLayout->addWidget(separatorLabel);
    timeLayout->addWidget(m_durationLabel);
    timeLayout->addStretch();

    infoLayout->addLayout(timeLayout);
    rightLayout->addWidget(infoFrame);

    // 3.2 歌词区域
    QFrame *lyricsFrame = new QFrame(m_rightFrame);
    lyricsFrame->setObjectName("lyricsFrame");
    lyricsFrame->setStyleSheet(R"(
        QFrame#lyricsFrame {
            background-color: rgba(255, 253, 250, 220);
            border-radius: 12px;
            border: 1px solid rgba(232, 228, 220, 180);
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.05);
        }
    )");

    QVBoxLayout *lyricsLayout = new QVBoxLayout(lyricsFrame);
    lyricsLayout->setContentsMargins(10, 20, 10, 20);
    lyricsLayout->setSpacing(0);

    // 歌词列表标题
    QLabel *lyricsTitle = new QLabel(tr("歌词"), lyricsFrame);
    lyricsTitle->setObjectName("lyricsTitle");
    lyricsTitle->setStyleSheet(R"(
        QLabel#lyricsTitle {
            color: #3C3A36;
            font-size: 16px;
            font-weight: bold;
            background: transparent;
            padding-bottom: 10px;
            border-bottom: 1px solid rgba(232, 228, 220, 120);
        }
    )");
    lyricsLayout->addWidget(lyricsTitle);

    // 歌词列表
    m_lyricsList = new QListWidget(lyricsFrame);
    m_lyricsList->setObjectName("lyricsList");
    m_lyricsList->setStyleSheet(R"(
        QListWidget#lyricsList {
            background: transparent;
            border: none;
            color: #5A5650;
            font-size: 14px;
            outline: none;
        }
        QListWidget#lyricsList::item {
            height: 36px;
            padding: 6px 10px;
            text-align: center;
            background: transparent;
            border-radius: 6px;
        }
        QListWidget#lyricsList::item:hover {
            background: rgba(200, 196, 188, 30);
        }
        QListWidget#lyricsList::item:selected {
            background: rgba(200, 107, 90, 20);
            color: #C86B5A;
            font-weight: bold;
            border-left: 3px solid #C86B5A;
        }
        QScrollBar:vertical {
            width: 8px;
            background: transparent;
            margin-right: 2px;
        }
        QScrollBar::handle:vertical {
            background: rgba(200, 196, 188, 80);
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(200, 196, 188, 120);
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
            background: none;
        }
    )");
    m_lyricsList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_lyricsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_lyricsList->setFocusPolicy(Qt::NoFocus);

    lyricsLayout->addWidget(m_lyricsList);
    rightLayout->addWidget(lyricsFrame, 1);

    // 4. 模糊效果
    m_blurEffect = new QGraphicsBlurEffect(this);
    m_blurEffect->setBlurRadius(25);
    m_blurEffect->setBlurHints(QGraphicsBlurEffect::QualityHint);

    // 5. 设置布局
    setLayout(mainLayout);
}

void MusicPlayerWidget::initConnections()
{
    // 连接歌词列表
    connect(m_lyricsList, &QListWidget::itemClicked,
            this, &MusicPlayerWidget::onLyricItemClicked);
}

// ==================== 公开接口实现 ====================
void MusicPlayerWidget::setSongInfo(const SongMeta &info)
{
    m_currentSongInfo = info;
    m_hasCurrentSong = true;

    // 解析时长
    if (!info.duration.isEmpty()) {
        m_duration = AudioUtil::SongMeta::parseDurationString(info.duration);
        m_durationLabel->setText(formatTime(m_duration));
    } else {
        m_duration = 0;
        m_durationLabel->setText("00:00");
    }

    // 更新UI
    m_titleLabel->setText(info.name.isEmpty() ? tr("未选择歌曲") : info.name);
    m_artistLabel->setText(info.singer.isEmpty() ? tr("未知歌手") : info.singer);
    m_albumLabel->setText(info.album.isEmpty() ? tr("未知专辑") : info.album);

    // 加载封面
    if (!info.coverUrl.isEmpty()) {
        setAlbumCoverFromPath(info.coverUrl);
    } else {
        // 如果没有封面，显示占位符
        m_coverLabel->clear();
        m_coverPlaceholder->show();
    }

    // 重置时钟和位置
    m_clock->stop();
    m_currentTimeLabel->setText("00:00");

    // 尝试自动加载歌词
    if (!info.filePath.isEmpty()) {
        QString lyricPath = LyricParser::guessLyricPath(info.filePath);
        if (QFile::exists(lyricPath)) {
            loadLyricsFromFile(lyricPath);
        }
    }

    // 发出信号
    emit currentSongChanged(info);
}

bool MusicPlayerWidget::loadLyricsFromFile(const QString &filePath)
{
    qDebug() << "updateSyncTimer卡了吗?";
    if (!m_lyricParser) return false;

    bool success = m_lyricParser->loadFromFile(filePath);
    if (success) {
        updateLyricsList();

        // 如果有当前播放位置，更新歌词高亮
        qint64 currentTime = m_clock->getTimeMs();
        if (currentTime > 0) {
            int index = m_lyricParser->findIndexAtTime(currentTime);
            if (index != m_currentLyricIndex) {
                m_currentLyricIndex = index;
                if (index >= 0) {
                    m_currentLyric = m_lyricParser->getLine(index);
                } else {
                    m_currentLyric = LrcLine::LyricLine();
                }
                highlightCurrentLyric();
                scrollToCurrentLyric();
            }
        }
    }

    emit lyricsLoaded(success);
    return success;
}

bool MusicPlayerWidget::loadLyricsFromContent(const QString &content)
{
    if (!m_lyricParser) return false;

    bool success = m_lyricParser->loadFromContent(content);
    if (success) {
        updateLyricsList();

        // 如果有当前播放位置，更新歌词高亮
        qint64 currentTime = m_clock->getTimeMs();
        if (currentTime > 0) {
            int index = m_lyricParser->findIndexAtTime(currentTime);
            if (index != m_currentLyricIndex) {
                m_currentLyricIndex = index;
                if (index >= 0) {
                    m_currentLyric = m_lyricParser->getLine(index);
                } else {
                    m_currentLyric = LrcLine::LyricLine();
                }
                highlightCurrentLyric();
                scrollToCurrentLyric();
            }
        }
    }

    emit lyricsLoaded(success);
    return success;
}

void MusicPlayerWidget::clearLyrics()
{
    if (m_lyricParser) {
        m_lyricParser->clear();
    }

    m_currentLyricIndex = -1;
    m_currentLyric = LrcLine::LyricLine();

    if (m_lyricsList) {
        m_lyricsList->clear();
    }
}

void MusicPlayerWidget::setAlbumCover(const QPixmap &cover)
{
    if (cover.isNull()) {
        m_coverLabel->clear();
        m_coverPlaceholder->show();
        return;
    }

    m_currentCover = cover;
    m_coverPlaceholder->hide();

    // 创建圆角矩形封面
    QPixmap roundedCover = createRoundedPixmap(cover, 6);

    // 缩放并设置
    QPixmap scaledCover = roundedCover.scaled(
        m_coverLabel->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );

    m_coverLabel->setPixmap(scaledCover);

    // 发出信号
    emit coverChanged(cover);
}

void MusicPlayerWidget::setAlbumCoverFromPath(const QString &coverPath)
{
    if (coverPath.isEmpty() || !QFile::exists(coverPath)) {
        m_coverLabel->clear();
        m_coverPlaceholder->show();
        return;
    }

    QPixmap cover(coverPath);
    if (!cover.isNull()) {
        setAlbumCover(cover);
    } else {
        m_coverLabel->clear();
        m_coverPlaceholder->show();
    }
}

void MusicPlayerWidget::setPlaying(bool playing)
{
    if (playing) {
        if (!m_hasCurrentSong) {
            emit errorOccurred(tr("没有可播放的歌曲"));
            return;
        }

        if (m_clock->isPaused()) {
            m_clock->resume();
            m_uiUpdateTimer->start();
            emit playRequested();
        } else {
            // 如果还没开始，从0开始播放
            m_clock->start(0);
            m_uiUpdateTimer->start();
            emit playRequested();
        }
    } else {
        m_clock->pause();
        m_uiUpdateTimer->stop();
        emit pauseRequested();
    }

    emit playStateChanged(playing);
}

void MusicPlayerWidget::setPosition(qint64 position)
{
    if (position < 0) position = 0;
    if (m_duration > 0 && position > m_duration) position = m_duration;

    // 设置时钟位置
    double timeSeconds = static_cast<double>(position) / 1000.0;
    m_clock->setTime(timeSeconds);

    // 更新时间显示
    updateTimeDisplay();

    // 更新歌词高亮
    if (m_lyricParser && !m_lyricParser->isEmpty()) {
        int newIndex = m_lyricParser->findIndexAtTime(position);

        if (newIndex != m_currentLyricIndex) {
            m_currentLyricIndex = newIndex;

            if (m_currentLyricIndex >= 0) {
                m_currentLyric = m_lyricParser->getLine(m_currentLyricIndex);
            } else {
                m_currentLyric = LrcLine::LyricLine();
            }

            highlightCurrentLyric();
            scrollToCurrentLyric();

            emit currentLyricChanged(m_currentLyric);
        }
    }

    emit positionChanged(position);
}

void MusicPlayerWidget::setDuration(qint64 duration)
{
    m_duration = duration;
    m_durationLabel->setText(formatTime(duration));
}

LrcLine::LyricLine MusicPlayerWidget::getCurrentLyric() const
{
    if (m_lyricParser && m_currentLyricIndex >= 0 && m_currentLyricIndex < m_lyricParser->totalLines()) {
        return m_lyricParser->getLine(m_currentLyricIndex);
    }
    return LrcLine::LyricLine();
}

LyricsList MusicPlayerWidget::getLyrics() const
{
    LyricsList lyrics;
    if (m_lyricParser) {
        QVector<LrcLine::LyricLine> lines = getLyricLines();
        lyrics.reserve(lines.size());
        for (const auto& line : lines) {
            lyrics.append(line);
        }
    }
    return lyrics;
}

void MusicPlayerWidget::play()
{
    setPlaying(true);
}

void MusicPlayerWidget::pause()
{
    setPlaying(false);
}

void MusicPlayerWidget::stop()
{
    m_clock->stop();
    m_uiUpdateTimer->stop();

    m_currentLyricIndex = -1;
    m_currentLyric = LrcLine::LyricLine();

    updateTimeDisplay();
    highlightCurrentLyric();

    emit stopRequested();
    emit playStateChanged(false);
}

void MusicPlayerWidget::seekForward(qint64 ms)
{
    qint64 currentTime = m_clock->getTimeMs();
    qint64 newPosition = currentTime + ms;
    if (m_duration > 0 && newPosition > m_duration) {
        newPosition = m_duration;
    }

    setPosition(newPosition);
    emit seekRequested(newPosition);
}

void MusicPlayerWidget::seekBackward(qint64 ms)
{
    qint64 currentTime = m_clock->getTimeMs();
    qint64 newPosition = currentTime - ms;
    if (newPosition < 0) newPosition = 0;

    setPosition(newPosition);
    emit seekRequested(newPosition);
}

void MusicPlayerWidget::clearAllData()
{
    // 清空歌曲信息
    m_currentSongInfo = SongMeta();
    m_hasCurrentSong = false;

    // 清空歌词
    clearLyrics();

    // 清空封面
    m_currentCover = QPixmap();
    m_coverLabel->clear();
    m_coverPlaceholder->show();

    // 重置UI文本
    m_titleLabel->setText(tr("未选择歌曲"));
    m_artistLabel->setText(tr("未知歌手"));
    m_albumLabel->setText(tr("未知专辑"));
    m_currentTimeLabel->setText("00:00");
    m_durationLabel->setText("00:00");

    // 重置时间和进度
    m_duration = 0;

    // 停止播放
    stop();
}

// ==================== 公共槽函数实现 ====================
void MusicPlayerWidget::onMusicLoaded(const SongMeta &info)
{
    setSongInfo(info);
}

void MusicPlayerWidget::onLyricsLoaded(const QString &filePath)
{
    loadLyricsFromFile(filePath);
}

void MusicPlayerWidget::onPositionChanged(qint64 position)
{
    setPosition(position);
}

void MusicPlayerWidget::onPlayStateChanged(bool isPlaying)
{
    setPlaying(isPlaying);
}

void MusicPlayerWidget::onCoverLoaded(const QPixmap &cover)
{
    setAlbumCover(cover);
}

void MusicPlayerWidget::onErrorOccurred(const QString &errorMsg)
{
    QMessageBox::critical(this, tr("播放错误"), errorMsg);
    emit errorOccurred(errorMsg);
}

// ==================== UI交互槽函数实现 ====================
void MusicPlayerWidget::onLyricItemClicked(QListWidgetItem *item)
{
    if (!item || !m_hasCurrentSong) return;

    qint64 position = item->data(Qt::UserRole).toLongLong();
    setPosition(position);
    emit lyricClicked(position);
    emit seekRequested(position);
}

void MusicPlayerWidget::onClockTimeChanged(double time)
{
    qint64 timeMs = static_cast<qint64>(time * 1000);

    // 检查是否播放完毕
    if (m_duration > 0 && timeMs >= m_duration) {
        stop();
        return;
    }

    // 更新时间显示
    updateTimeDisplay();

    // 更新歌词高亮
    if (m_lyricParser && !m_lyricParser->isEmpty()) {
        int newIndex = m_lyricParser->findIndexAtTime(timeMs);

        if (newIndex != m_currentLyricIndex) {
            m_currentLyricIndex = newIndex;

            if (m_currentLyricIndex >= 0) {
                m_currentLyric = m_lyricParser->getLine(m_currentLyricIndex);
            } else {
                m_currentLyric = LrcLine::LyricLine();
            }

            highlightCurrentLyric();
            scrollToCurrentLyric();

            emit currentLyricChanged(m_currentLyric);
        }
    }

    emit positionChanged(timeMs);
}

void MusicPlayerWidget::updateUIFromClock()
{
    // 通过时钟信号更新UI，这个定时器主要用于确保UI在时钟暂停时也能停止更新
    // 实际的时间更新在onClockTimeChanged槽函数中处理
}

// ==================== 歌词管理函数实现 ====================
void MusicPlayerWidget::updateLyricsList()
{
    if (!m_lyricsList || !m_lyricParser) return;

    m_lyricsList->clear();

    QVector<LrcLine::LyricLine> lines = getLyricLines();

    for (const LrcLine::LyricLine &line : lines) {
        QListWidgetItem *item = new QListWidgetItem();

        QString text = formatLyricText(line);
        item->setText(text);
        item->setTextAlignment(Qt::AlignCenter);
        item->setData(Qt::UserRole, line.timeMs);

        m_lyricsList->addItem(item);
    }
}

QString MusicPlayerWidget::formatLyricText(const LrcLine::LyricLine &line) const
{
    QString text = line.getText("original");
    if (text.isEmpty()) {
        text = line.getText();
    }

    if (line.hasText("translation")) {
        QString translation = line.getText("translation");
        if (!translation.isEmpty()) {
            text += "\n" + translation;
        }
    }

    return text;
}

QVector<LrcLine::LyricLine> MusicPlayerWidget::getLyricLines() const
{
    if (!m_lyricParser) return QVector<LrcLine::LyricLine>();

    QVector<LrcLine::LyricLine> lines;
    int total = m_lyricParser->totalLines();
    lines.reserve(total);

    for (int i = 0; i < total; ++i) {
        lines.append(m_lyricParser->getLine(i));
    }

    return lines;
}

void MusicPlayerWidget::highlightCurrentLyric()
{
    if (!m_lyricsList || m_currentLyricIndex < 0) return;

    // 清除所有选中状态
    for (int i = 0; i < m_lyricsList->count(); ++i) {
        m_lyricsList->item(i)->setSelected(false);
    }

    // 高亮当前歌词
    if (m_currentLyricIndex < m_lyricsList->count()) {
        QListWidgetItem *item = m_lyricsList->item(m_currentLyricIndex);
        item->setSelected(true);
    }
}

void MusicPlayerWidget::scrollToCurrentLyric()
{
    if (!m_lyricsList || m_currentLyricIndex < 0 || m_currentLyricIndex >= m_lyricsList->count()) {
        return;
    }

    QListWidgetItem *item = m_lyricsList->item(m_currentLyricIndex);
    m_lyricsList->scrollToItem(item, QAbstractItemView::PositionAtCenter);
}

// ==================== 工具函数实现 ====================
QString MusicPlayerWidget::formatTime(qint64 ms) const
{
    int totalSeconds = static_cast<int>(ms / 1000);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    return QString("%1:%2")
           .arg(minutes, 2, 10, QChar('0'))
           .arg(seconds, 2, 10, QChar('0'));
}

void MusicPlayerWidget::updateTimeDisplay()
{
    qint64 currentTime = m_clock->getTimeMs();
    m_currentTimeLabel->setText(formatTime(currentTime));
}

QPixmap MusicPlayerWidget::createRoundedCover(const QPixmap &original, int radius)
{
    return createRoundedPixmap(original, radius);
}

// ==================== 事件处理 ====================
void MusicPlayerWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制背景
    if (parentWidget() && !m_parentBgCache.isNull() && m_parentBgCache.size() == size()) {
        painter.drawPixmap(rect(), m_parentBgCache);
    } else {
        // 绘制渐变色背景
        QLinearGradient gradient(0, 0, width(), height());
        gradient.setColorAt(0, QColor(250, 249, 246, 200));
        gradient.setColorAt(1, QColor(245, 242, 237, 200));
        painter.fillRect(rect(), gradient);
    }

    // 绘制子控件
    QWidget::paintEvent(event);
}

void MusicPlayerWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    // 尺寸变化时清空背景缓存
    if (event->oldSize().width() != size().width() ||
        event->oldSize().height() != size().height()) {
        m_parentBgCache = QPixmap();
    }
}