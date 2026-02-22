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

#include "AudioPart/AudioPlay/AudioPlayer.h"

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

    // 绑定到主时钟
    bindToMasterClock(AudioPlayer::getInstance()->getClock());

    // 可选的UI更新定时器（保持原有逻辑）
    m_uiUpdateTimer = new QTimer(this);
    m_uiUpdateTimer->setInterval(1000); // 1秒更新一次（只是为了保险）
    connect(m_uiUpdateTimer, &QTimer::timeout, this, &MusicPlayerWidget::updateUIFromClock);

    m_currentTimeLabel->setText(QString("00:00"));
}

MusicPlayerWidget::~MusicPlayerWidget()
{
    if (m_blurEffect) delete m_blurEffect;
}

void MusicPlayerWidget::initClock()
{
    // 直接连接时钟信号，不使用任何过滤
    connect(m_clock, &Clock::timeChanged, this, &MusicPlayerWidget::onClockTimeChanged, Qt::QueuedConnection);

    // 也可以连接毫秒信号，用于更精确的更新（可选）
    connect(m_clock, &Clock::timeChangedMs, this, [this](qint64 ms) {
        // 只是更新时间显示，不处理歌词
        updateTimeDisplayFromMs(ms);
    }, Qt::QueuedConnection);

    qDebug() << "[MusicPlayerWidget::initClock] 时钟连接完成";
}

// 新增：绑定到主时钟（AudioPlayer单例的m_clock）
void MusicPlayerWidget::bindToMasterClock(Clock* masterClock)
{
    if (!masterClock || !m_clock) {
        qWarning() << "[MusicPlayerWidget] 主时钟为空，绑定失败";
        return;
    }

    qDebug() << "[MusicPlayerWidget::bindToMasterClock] 开始绑定，"
             << "主时钟线程：" << masterClock->thread()->currentThreadId()
             << "UI时钟线程：" << m_clock->thread()->currentThreadId();

    // 关键：直接调用同步函数（但确保线程安全）
    if (m_clock->thread() == QThread::currentThread()) {
        // 同线程直接调用
        m_clock->syncToMaster(masterClock);
    } else {
        // 跨线程使用QueuedConnection
        QMetaObject::invokeMethod(m_clock, "syncToMaster",
                                  Qt::QueuedConnection,
                                  Q_ARG(Clock*, masterClock));
    }

    qDebug() << "[MusicPlayerWidget] 绑定完成";
}

// ==================== 初始化函数实现 ====================
/*
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

    // 3.2 歌词区域 - 修改为固定中心高亮版本
    QFrame *lyricsFrame = new QFrame(m_rightFrame);
    lyricsFrame->setObjectName("lyricsFrame");
    lyricsFrame->setStyleSheet(R"(
        QFrame#lyricsFrame {
            background-color: rgba(255, 253, 250, 0.95);
            border-radius: 12px;
            border: 1px solid rgba(232, 228, 220, 0.8);
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.05);
        }
    )");

    QVBoxLayout *lyricsLayout = new QVBoxLayout(lyricsFrame);
    lyricsLayout->setContentsMargins(10, 20, 10, 20);
    lyricsLayout->setSpacing(0);

    // 歌词标题
    QLabel *lyricsTitle = new QLabel(tr("歌词"), lyricsFrame);
    lyricsTitle->setObjectName("lyricsTitle");
    lyricsTitle->setStyleSheet(R"(
        QLabel#lyricsTitle {
            color: #3C3A36;
            font-size: 16px;
            font-weight: bold;
            background: transparent;
            padding-bottom: 10px;
            border-bottom: 1px solid rgba(232, 228, 220, 0.6);
            text-align: center;
        }
    )");
    lyricsLayout->addWidget(lyricsTitle);

    // 歌词显示区域容器（带固定中心线）
    m_lyricsViewport = new QFrame(lyricsFrame);
    m_lyricsViewport->setObjectName("lyricsViewport");
    m_lyricsViewport->setStyleSheet(R"(
        QFrame#lyricsViewport {
            background: transparent;
            border: none;
        }
    )");
    m_lyricsViewport->setFixedHeight(360);

    // 创建垂直布局用于歌词列表
    QVBoxLayout *viewportLayout = new QVBoxLayout(m_lyricsViewport);
    viewportLayout->setContentsMargins(0, 0, 0, 0);
    viewportLayout->setSpacing(0);

    // 固定中心线指示器
    m_centerLineIndicator = new QLabel(m_lyricsViewport);
    m_centerLineIndicator->setObjectName("centerLineIndicator");
    m_centerLineIndicator->setFixedHeight(1);
    m_centerLineIndicator->setStyleSheet(R"(
        QLabel#centerLineIndicator {
            background: rgba(200, 107, 90, 0.3);
        }
    )");

    // 歌词列表（不再直接添加到布局，而是使用绝对定位）
    m_lyricsList = new QListWidget(m_lyricsViewport);
    m_lyricsList->setObjectName("lyricsList");
    m_lyricsList->setStyleSheet(R"(
        QListWidget#lyricsList {
            background: transparent;
            border: none;
            color: #5A5650;
            font-size: 14px;
            outline: none;
            padding: 0px 10px;
        }
        QListWidget#lyricsList::item {
            height: 32px;
            padding: 0px 15px;
            text-align: center;
            background: transparent;
            border-radius: 6px;
            border: none;
        }
        QListWidget#lyricsList::item:hover {
            background: rgba(200, 196, 188, 0.12);
        }

        QListWidget#lyricsList::item.current-playing {
            color: #C86B5A;
            font-weight: bold;
            background: rgba(200, 107, 90, 0.08);
        }

        QListWidget#lyricsList::item.center-highlight {
            font-size: 20px;
            color: #C86B5A;
            font-weight: bold;
            height: 44px;
            border-left: 3px solid #C86B5A;
            background: rgba(200, 107, 90, 0.12);
        }
        QScrollBar:vertical {
            width: 8px;
            background: transparent;
            margin-right: 2px;
        }
        QScrollBar::handle:vertical {
            background: rgba(200, 196, 188, 0.8);
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(200, 196, 188, 0.6);
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
            background: none;
        }
    )");

    // 设置列表属性
    m_lyricsList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_lyricsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_lyricsList->setFocusPolicy(Qt::NoFocus);
    m_lyricsList->setSelectionMode(QAbstractItemView::NoSelection);

    // 添加中心线到布局（居中对齐）
    viewportLayout->addStretch();
    viewportLayout->addWidget(m_centerLineIndicator);
    viewportLayout->addStretch();

    lyricsLayout->addWidget(m_lyricsViewport);
    rightLayout->addWidget(lyricsFrame, 1);

    // 4. 初始化歌词滚动相关变量
    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setInterval(16); // 约60fps
    connect(m_scrollTimer, &QTimer::timeout, this, &MusicPlayerWidget::updateScrollAnimation);

    m_targetScrollPosition = 0;
    m_scrollAnimationStep = 0;
    m_isScrolling = false;
    m_currentCenterLine = -1;
    m_highlightColor = QColor("#C86B5A");
    m_normalFontSize = 14;
    m_centerFontSize = 20;

    // 5. 模糊效果
    m_blurEffect = new QGraphicsBlurEffect(this);
    m_blurEffect->setBlurRadius(25);
    m_blurEffect->setBlurHints(QGraphicsBlurEffect::QualityHint);

    // 6. 设置布局
    setLayout(mainLayout);

}
**/



void MusicPlayerWidget::initConnections()
{
    // 连接歌词列表点击事件
    connect(m_lyricsList, &QListWidget::itemClicked,
            this, &MusicPlayerWidget::onLyricItemClicked);

    // 连接滚动事件
    connect(m_lyricsList->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MusicPlayerWidget::updateLyricsStyle);

    // 获取线程池单例
    m_lyricThreadPool = LyricParserThreadPool::instance(this);
    connect(m_lyricThreadPool, &LyricParserThreadPool::parseFinished,
            this, &MusicPlayerWidget::onLyricsParseFinished,
            Qt::QueuedConnection);
    connect(m_lyricThreadPool, &LyricParserThreadPool::loaded,
            this, &MusicPlayerWidget::lyricsLoaded,
            Qt::QueuedConnection);
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
    if (m_clock) {
        qDebug() << "[MusicPlayerWidget::stop] 停止时钟";
        m_clock->stop();
    }

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

    // 立即滚动到点击的行
    int index = m_lyricsList->row(item);
    if (index >= 0) {
        m_currentLyricIndex = index;
        scrollLyricsToCurrentLine();
    }
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

            // 使用平滑滚动更新歌词位置
            scrollLyricsToCurrentLine();

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
        item->setData(Qt::UserRole + 1, ""); // 用于存储样式类

        // 设置固定高度
        item->setSizeHint(QSize(item->sizeHint().width(), 32));

        m_lyricsList->addItem(item);
    }

    // 更新歌词列表总高度
    int totalHeight = m_lyricsList->count() * (32 + 8) + 16;
    m_lyricsList->setMinimumHeight(totalHeight);

    // 设置视口
    setupLyricsViewport();
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

    // 滚动到当前行
    scrollLyricsToCurrentLine();

    // 更新样式
    updateLyricsStyle();
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

    // 更新歌词视口大小
    if (m_lyricsList && m_lyricsViewport) {
        m_lyricsList->resize(m_lyricsViewport->width(), m_lyricsList->height());
    }
}

void MusicPlayerWidget::updateTimeDisplay()
{
    // 空值保护：避免时钟未初始化导致崩溃
    if (!m_clock || !m_currentTimeLabel) return;

    // 获取时钟当前毫秒数（秒级）
    qint64 currentTime = m_clock->getTimeMs();
    // 格式化时间（补零，保证00:00格式）
    QString timeText = formatTime(currentTime);

    // 仅当文本变化时更新（减少UI重绘）
    if (m_currentTimeLabel->text() != timeText) {
        m_currentTimeLabel->setText(timeText);
        // 强制刷新UI（立即显示，避免延迟）
        m_currentTimeLabel->update();
        // 可选：设置文本对齐/样式，确保显示正常
        m_currentTimeLabel->setAlignment(Qt::AlignCenter);
    }
}

void MusicPlayerWidget::updateTimeDisplayFromMs(qint64 ms)
{
    if (!m_currentTimeLabel) return;

    QString timeText = formatTime(ms);

    // 只在文本改变时更新，避免频繁重绘
    if (m_currentTimeLabel->text() != timeText) {
        m_currentTimeLabel->setText(timeText);
    }
}

void MusicPlayerWidget::onLyricsParseFinished(bool success, const LyricsList& lines, const QString& errorMsg)
{
    // 实现歌词解析完成后的处理逻辑
}

// ==================== 新增：固定中心高亮歌词功能实现 ====================

void MusicPlayerWidget::setupLyricsViewport()
{
    if (!m_lyricsList || !m_lyricsViewport) return;

    // 设置歌词列表为视口的子控件，使用绝对定位
    m_lyricsList->setParent(m_lyricsViewport);
    m_lyricsList->move(0, 0);
    m_lyricsList->resize(m_lyricsViewport->width(), m_lyricsViewport->height());

    // 将中心线置于顶层
    m_centerLineIndicator->raise();
}

void MusicPlayerWidget::scrollLyricsToCurrentLine()
{
    if (!m_lyricsList || m_currentLyricIndex < 0) return;

    // 计算目标滚动位置（使当前行居中）
    int viewportHeight = m_lyricsViewport->height();
    int itemHeight = 32; // 普通行高
    int lineSpacing = 8; // 行间距
    int lineTotalHeight = itemHeight + lineSpacing;

    // 计算当前行应该显示的位置（从顶部开始计算）
    int targetY = m_currentLyricIndex * lineTotalHeight;

    // 计算需要滚动的位置，使当前行在视口中央
    int centerY = viewportHeight / 2 - itemHeight / 2;
    m_targetScrollPosition = qMax(0, targetY - centerY);

    // 开始平滑滚动动画
    m_scrollAnimationStep = 0;
    m_isScrolling = true;
    m_scrollTimer->start();
}

void MusicPlayerWidget::updateScrollAnimation()
{
    if (!m_isScrolling) {
        m_scrollTimer->stop();
        return;
    }

    // 获取当前滚动位置
    int currentScroll = m_lyricsList->verticalScrollBar()->value();

    // 计算平滑滚动的目标位置
    int diff = m_targetScrollPosition - currentScroll;

    // 使用缓动函数实现平滑滚动
    if (qAbs(diff) < 2) {
        m_lyricsList->verticalScrollBar()->setValue(m_targetScrollPosition);
        m_isScrolling = false;
        m_scrollTimer->stop();
        updateLyricsStyle();
        return;
    }

    // 每帧移动距离的1/8，实现平滑效果
    int step = diff / 8;
    if (step == 0) step = (diff > 0) ? 1 : -1;

    m_lyricsList->verticalScrollBar()->setValue(currentScroll + step);
    updateLyricsStyle();
}

void MusicPlayerWidget::updateLyricsStyle()
{
    if (!m_lyricsList) return;

    // 获取视口和滚动位置
    int viewportHeight = m_lyricsViewport->height();
    int scrollPosition = m_lyricsList->verticalScrollBar()->value();
    int centerY = viewportHeight / 2;

    // 清除所有行的特殊样式
    for (int i = 0; i < m_lyricsList->count(); ++i) {
        QListWidgetItem* item = m_lyricsList->item(i);
        if (!item) continue;

        // 移除之前的样式类
        QString style = item->data(Qt::UserRole + 1).toString();
        style.remove("current-playing");
        style.remove("center-highlight");
        item->setData(Qt::UserRole + 1, style);
    }

    // 设置当前播放行的样式
    if (m_currentLyricIndex >= 0 && m_currentLyricIndex < m_lyricsList->count()) {
        QListWidgetItem* item = m_lyricsList->item(m_currentLyricIndex);
        if (item) {
            QString style = item->data(Qt::UserRole + 1).toString();
            style += " current-playing";
            item->setData(Qt::UserRole + 1, style);
        }
    }

    // 找到当前在中心位置的行
    m_currentCenterLine = -1;
    int minDistance = viewportHeight;

    for (int i = 0; i < m_lyricsList->count(); ++i) {
        QListWidgetItem* item = m_lyricsList->item(i);
        if (!item) continue;

        // 计算该行在视口中的Y坐标
        int itemY = i * (32 + 8) - scrollPosition + 16; // 32=行高, 8=间距, 16=半行高
        int distance = qAbs(itemY - centerY);

        // 找出距离中心最近的行
        if (distance < minDistance) {
            minDistance = distance;
            m_currentCenterLine = i;
        }
    }

    // 设置中心行的高亮样式
    if (m_currentCenterLine >= 0 && m_currentCenterLine < m_lyricsList->count()) {
        QListWidgetItem* item = m_lyricsList->item(m_currentCenterLine);
        if (item) {
            QString style = item->data(Qt::UserRole + 1).toString();
            style += " center-highlight";
            item->setData(Qt::UserRole + 1, style);

            // 动态调整字体大小（距离中心越近字体越大）
            int itemY = m_currentCenterLine * (32 + 8) - scrollPosition + 16;
            int distance = qAbs(itemY - centerY);
            int fontSize = m_centerFontSize - (distance * 2 / 32); // 根据距离动态计算

            // 确保字体大小在合理范围内
            fontSize = qMax(m_normalFontSize, qMin(m_centerFontSize, fontSize));

            // 创建动态样式表
            QString dynamicStyle = QString(
                "QListWidget::item { font-size: %1px; } "
                "QListWidget::item.center-highlight { font-size: %2px; }"
            ).arg(m_normalFontSize).arg(fontSize);

            // 应用到当前行
            QFont font = item->font();
            font.setPointSize(fontSize);
            item->setFont(font);
        }
    }
}

void MusicPlayerWidget::initUI()
{
    // 整体背景（HTML body的样式）
    setStyleSheet(R"(
        QWidget {
            background: #f9f7f5;
            font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif;
        }
    )");

    // 主布局（水平左右分栏，类似HTML的.player-container）
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(20, 30, 20, 40);
    mainLayout->setSpacing(0);
    mainLayout->setAlignment(Qt::AlignCenter);

    // 计算尺寸：右半部分为正方形（放大1.25倍）
    float scaleFactor = 1.25f; // 放大1.25倍
    int leftWidth = 300; // 左侧固定宽度
    int spacing = 60;    // 左右容器之间的间距
    int padding = 30;    // playerContainer的内边距
    int rightSize = 400 * scaleFactor; // 右半部分正方形边长放大

    // 主容器尺寸：宽度 = 左宽 + 间距 + 右宽 + 内边距*2
    // 高度 = 右半部分高度 + 内边距*2
    int containerWidth = leftWidth + spacing + rightSize + padding * 2;
    int containerHeight = rightSize + padding * 2;

    // 播放器主容器（对应HTML的.player-container）
    QFrame *playerContainer = new QFrame(this);
    playerContainer->setObjectName("playerContainer");
    playerContainer->setStyleSheet(R"(
        QFrame#playerContainer {
            background: #FAF9F6;
            border-radius: 16px;
            border: 1px solid rgba(232, 228, 220, 0.8);
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.08);
            padding: 30px;
        }
    )");
    playerContainer->setFixedSize(containerWidth, containerHeight);

    QHBoxLayout *containerLayout = new QHBoxLayout(playerContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(spacing);

    // ==================== 左侧：专辑封面区域 ====================
    // 对应HTML的.album-section
    QFrame *leftSection = new QFrame(playerContainer);
    leftSection->setObjectName("leftSection");
    leftSection->setStyleSheet(R"(
        QFrame#leftSection {
            background: rgba(255, 253, 250, 0.9);
            border-radius: 12px;
            border: 1px solid rgba(232, 228, 220, 0.8);
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.05);
        }
    )");
    leftSection->setFixedWidth(leftWidth); // 固定宽度
    leftSection->setMinimumHeight(rightSize); // 高度与右侧正方形保持一致

    QVBoxLayout *leftLayout = new QVBoxLayout(leftSection);
    leftLayout->setContentsMargins(25, 25, 25, 25);
    leftLayout->setSpacing(20);
    leftLayout->setAlignment(Qt::AlignCenter);

    // 专辑封面容器（对应HTML的.album-cover）
    QWidget *albumCover = new QWidget(leftSection);
    albumCover->setObjectName("albumCover");
    albumCover->setStyleSheet(R"(
        QWidget#albumCover {
            background: radial-gradient(circle at center, rgba(255,255,255,0.8), rgba(245,242,237,0.6));
            border-radius: 8px;
            border: 2px solid rgba(232, 228, 220, 0.9);
            box-shadow: 0 8px 32px rgba(200, 196, 188, 0.3),
                        inset 0 1px 0 rgba(255,255,255,0.6);
        }
    )");
    albumCover->setFixedSize(240, 240);

    QVBoxLayout *coverLayout = new QVBoxLayout(albumCover);
    coverLayout->setContentsMargins(0, 0, 0, 0);
    coverLayout->setAlignment(Qt::AlignCenter);

    // 使用原有的封面标签
    m_coverLabel = new QLabel(albumCover);
    m_coverLabel->setObjectName("coverLabel");
    m_coverLabel->setFixedSize(240, 240);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setStyleSheet(R"(
        QLabel#coverLabel {
            border-radius: 8px;
            border: none;
            background: transparent;
        }
    )");
    coverLayout->addWidget(m_coverLabel);

    // 封面占位符
    m_coverPlaceholder = new QLabel(albumCover);
    m_coverPlaceholder->setObjectName("coverPlaceholder");
    m_coverPlaceholder->setFixedSize(240, 240);
    m_coverPlaceholder->setAlignment(Qt::AlignCenter);
    m_coverPlaceholder->setPixmap(QPixmap(":/icons/music_note.svg").scaled(
        120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_coverPlaceholder->setStyleSheet(R"(
        QLabel#coverPlaceholder {
            border-radius: 8px;
            border: none;
            background: rgba(250, 249, 246, 0.8);
        }
    )");
    coverLayout->addWidget(m_coverPlaceholder);

    leftLayout->addWidget(albumCover);

    // 歌曲信息区域（对应HTML的.song-info）
    QWidget *songInfo = new QWidget(leftSection);
    songInfo->setObjectName("songInfo");
    songInfo->setStyleSheet("QWidget#songInfo { background: transparent; }");

    QVBoxLayout *infoLayout = new QVBoxLayout(songInfo);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(8);
    infoLayout->setAlignment(Qt::AlignCenter);

    // 使用原有的标题标签
    m_titleLabel = new QLabel(tr("未选择歌曲"), songInfo);
    m_titleLabel->setObjectName("songTitle");
    QFont titleFont = QFont("Microsoft YaHei", 22, QFont::Bold);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet(R"(
        QLabel#songTitle {
            color: #3C3A36;
            background: transparent;
            text-align: center;
        }
    )");
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    infoLayout->addWidget(m_titleLabel);

    // 使用原有的歌手标签
    m_artistLabel = new QLabel(tr("未知歌手"), songInfo);
    m_artistLabel->setObjectName("songArtist");
    QFont artistFont = QFont("Microsoft YaHei", 16, QFont::Normal);
    m_artistLabel->setFont(artistFont);
    m_artistLabel->setStyleSheet(R"(
        QLabel#songArtist {
            color: #C86B5A;
            background: transparent;
            text-align: center;
        }
    )");
    m_artistLabel->setWordWrap(true);
    m_artistLabel->setAlignment(Qt::AlignCenter);
    infoLayout->addWidget(m_artistLabel);

    // 使用原有的专辑标签
    m_albumLabel = new QLabel(tr("未知专辑"), songInfo);
    m_albumLabel->setObjectName("songAlbum");
    QFont albumFont = QFont("Microsoft YaHei", 14, QFont::Normal);
    m_albumLabel->setFont(albumFont);
    m_albumLabel->setStyleSheet(R"(
        QLabel#songAlbum {
            color: #7A756E;
            background: transparent;
            text-align: center;
        }
    )");
    m_albumLabel->setWordWrap(true);
    m_albumLabel->setAlignment(Qt::AlignCenter);
    infoLayout->addWidget(m_albumLabel);

    // 时间指示器（对应HTML的.time-indicator）
    QWidget *timeIndicator = new QWidget(songInfo);
    timeIndicator->setObjectName("timeIndicator");
    timeIndicator->setStyleSheet("QWidget#timeIndicator { background: transparent; }");

    QHBoxLayout *timeLayout = new QHBoxLayout(timeIndicator);
    timeLayout->setContentsMargins(0, 0, 0, 0);
    timeLayout->setSpacing(10);
    timeLayout->setAlignment(Qt::AlignCenter);

    // 使用原有的时间标签
    m_currentTimeLabel = new QLabel("00:00", timeIndicator);
    m_currentTimeLabel->setObjectName("currentTime");
    m_currentTimeLabel->setStyleSheet(R"(
        QLabel#currentTime {
            color: #C86B5A;
            font-size: 14px;
            font-weight: bold;
            background: transparent;
        }
    )");
    m_currentTimeLabel->setAlignment(Qt::AlignCenter);

    QLabel *separator = new QLabel("/", timeIndicator);
    separator->setObjectName("timeSeparator");
    separator->setStyleSheet(R"(
        QLabel#timeSeparator {
            color: #7A756E;
            font-size: 14px;
            background: transparent;
        }
    )");

    m_durationLabel = new QLabel("00:00", timeIndicator);
    m_durationLabel->setObjectName("totalTime");
    m_durationLabel->setStyleSheet(R"(
        QLabel#totalTime {
            color: #7A756E;
            font-size: 14px;
            background: transparent;
        }
    )");
    m_durationLabel->setAlignment(Qt::AlignCenter);

    timeLayout->addWidget(m_currentTimeLabel);
    timeLayout->addWidget(separator);
    timeLayout->addWidget(m_durationLabel);

    infoLayout->addWidget(timeIndicator);
    leftLayout->addWidget(songInfo);
    leftLayout->addStretch();

    containerLayout->addWidget(leftSection);

    // ==================== 右侧：歌词区域 ====================
    // 对应HTML的.lyrics-section
    QFrame *rightSection = new QFrame(playerContainer);
    rightSection->setObjectName("rightSection");
    rightSection->setStyleSheet("QFrame#rightSection { background: transparent; }");
    rightSection->setFixedSize(rightSize, rightSize); // 正方形，已放大

    QVBoxLayout *rightLayout = new QVBoxLayout(rightSection);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    // 歌词容器（对应HTML的.lyrics-container）
    QFrame *lyricsContainer = new QFrame(rightSection);
    lyricsContainer->setObjectName("lyricsContainer");
    lyricsContainer->setStyleSheet(R"(
        QFrame#lyricsContainer {
            background: rgba(255, 253, 250, 0.95);
            border-radius: 12px;
            border: 1px solid rgba(232, 228, 220, 0.8);
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.05);
        }
    )");

    QVBoxLayout *lyricsContainerLayout = new QVBoxLayout(lyricsContainer);
    lyricsContainerLayout->setContentsMargins(20, 20, 20, 20);
    lyricsContainerLayout->setSpacing(0);

    // 歌词标题（对应HTML的.lyrics-header）
    QFrame *lyricsHeader = new QFrame(lyricsContainer);
    lyricsHeader->setObjectName("lyricsHeader");
    lyricsHeader->setStyleSheet(R"(
        QFrame#lyricsHeader {
            background: transparent;
            border-bottom: 1px solid rgba(232, 228, 220, 0.6);
            padding-bottom: 10px;
            margin-bottom: 15px;
        }
    )");
    lyricsHeader->setFixedHeight(60); // 增加高度到60

    QVBoxLayout *headerLayout = new QVBoxLayout(lyricsHeader);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *lyricsTitle = new QLabel(tr("歌词"), lyricsHeader);
    lyricsTitle->setObjectName("lyricsTitle");
    QFont lyricsTitleFont = QFont("Microsoft YaHei", 18, QFont::Bold);
    lyricsTitle->setFont(lyricsTitleFont);
    lyricsTitle->setStyleSheet(R"(
        QLabel#lyricsTitle {
            color: #3C3A36;
            background: transparent;
            text-align: center;
        }
    )");
    lyricsTitle->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(lyricsTitle);

    lyricsContainerLayout->addWidget(lyricsHeader);

    // 歌词显示视口（对应HTML的.lyrics-viewport）
    m_lyricsViewport = new QFrame(lyricsContainer);
    m_lyricsViewport->setObjectName("lyricsViewport");
    m_lyricsViewport->setStyleSheet(R"(
        QFrame#lyricsViewport {
            background: transparent;
            border: none;
        }
    )");
    // 计算歌词视口高度：正方形高度(500) - 上下内边距(40) - 歌词标题高度(40) - 标题下边距(15) = 405
    m_lyricsViewport->setFixedHeight(rightSize - 40 - 40 - 15);

    // 使用网格布局来叠加歌词列表和中心线
    QGridLayout *viewportLayout = new QGridLayout(m_lyricsViewport);
    viewportLayout->setContentsMargins(0, 0, 0, 0);
    viewportLayout->setSpacing(0);

    // 使用原有的歌词列表
    m_lyricsList = new QListWidget(m_lyricsViewport);
    m_lyricsList->setObjectName("lyricsList");
    m_lyricsList->setStyleSheet(R"(
        QListWidget#lyricsList {
            background: transparent;
            border: none;
            color: #5A5650;
            font-size: 14px;
            outline: none;
            padding: 0px 10px;
        }
        QListWidget#lyricsList::item {
            height: 32px;
            padding: 4px 10px;
            text-align: center;
            background: transparent;
            border-radius: 6px;
        }
        QListWidget#lyricsList::item:hover {
            background: rgba(200, 196, 188, 30);
        }
        /* 当前播放歌词 */
        QListWidget#lyricsList::item:selected {
            background: rgba(200, 107, 90, 20);
            color: #C86B5A;
            font-weight: bold;
            border-left: 3px solid #C86B5A;
        }
        /* 中心高亮歌词（新增样式） */
        QListWidget#lyricsList::item.center-highlight {
            font-size: 18px !important;
            color: #C86B5A !important;
            font-weight: bold !important;
            height: 38px !important;
            border-left: 3px solid #C86B5A;
            background: rgba(200, 107, 90, 0.12);
        }
        QScrollBar:vertical {
            width: 6px;
            background: transparent;
            margin-right: 2px;
        }
        QScrollBar::handle:vertical {
            background: rgba(200, 196, 188, 80);
            border-radius: 3px;
            min-height: 25px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(200, 196, 188, 120);
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
            background: none;
        }
    )");

    // 设置列表属性
    m_lyricsList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_lyricsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_lyricsList->setFocusPolicy(Qt::NoFocus);
    m_lyricsList->setSelectionMode(QAbstractItemView::SingleSelection);

    // 固定中心线指示器（对应HTML的.center-line）
    m_centerLineIndicator = new QLabel(m_lyricsViewport);
    m_centerLineIndicator->setObjectName("centerLineIndicator");
    m_centerLineIndicator->setStyleSheet(R"(
        QFrame#centerLineIndicator {
            background: transparent;
            border: none;
        }
    )");
    m_centerLineIndicator->setFixedHeight(1);

    // 中心线（实际是一条线）
    QFrame *centerLine = new QFrame(m_centerLineIndicator);
    centerLine->setObjectName("centerLine");
    centerLine->setStyleSheet(R"(
        QFrame#centerLine {
            background: rgba(200, 107, 90, 0.3);
            border: none;
        }
    )");
    centerLine->setFixedHeight(1);

    // 中心点（使用QLabel模拟圆点）
    QLabel *centerDot = new QLabel(m_centerLineIndicator);
    centerDot->setObjectName("centerDot");
    centerDot->setStyleSheet(R"(
        QLabel#centerDot {
            background: #C86B5A;
            border-radius: 4px;
            border: none;
        }
    )");
    centerDot->setFixedSize(8, 8);

    // 布局中心线
    QHBoxLayout *centerLineLayout = new QHBoxLayout(m_centerLineIndicator);
    centerLineLayout->setContentsMargins(0, 0, 0, 0);
    centerLineLayout->setSpacing(0);
    centerLineLayout->addStretch();
    centerLineLayout->addWidget(centerLine);
    centerLineLayout->addStretch();

    // 布局中心点
    QVBoxLayout *dotLayout = new QVBoxLayout();
    dotLayout->setContentsMargins(0, -4, 0, 0);
    dotLayout->addWidget(centerDot);
    dotLayout->setAlignment(Qt::AlignCenter);

    // 将歌词列表和中心线叠加显示
    viewportLayout->addWidget(m_lyricsList, 0, 0);
    viewportLayout->addWidget(m_centerLineIndicator, 0, 0);

    // 设置中心线垂直居中
    viewportLayout->setAlignment(m_centerLineIndicator, Qt::AlignCenter);

    lyricsContainerLayout->addWidget(m_lyricsViewport);
    rightLayout->addWidget(lyricsContainer);
    containerLayout->addWidget(rightSection);

    // 将播放器容器添加到主布局
    mainLayout->addWidget(playerContainer);

    // 初始化歌词滚动相关变量
    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setInterval(16);
    connect(m_scrollTimer, &QTimer::timeout, this, &MusicPlayerWidget::updateScrollAnimation);

    m_targetScrollPosition = 0;
    m_scrollAnimationStep = 0;
    m_isScrolling = false;
    m_currentCenterLine = -1;
    m_highlightColor = QColor("#C86B5A");
    m_normalFontSize = 14;
    m_centerFontSize = 18;

    // 设置布局
    setLayout(mainLayout);
}