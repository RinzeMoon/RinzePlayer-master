#ifndef RINZEPLAYER_MUSICPLAYERWIDGET_H
#define RINZEPLAYER_MUSICPLAYERWIDGET_H

#include <QWidget>
#include <QPointer>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSlider>
#include <QListWidget>
#include <QPropertyAnimation>
#include <QGraphicsBlurEffect>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QTimer>
#include <QElapsedTimer>
#include <QScrollBar>

#include "../../Global/Global.h"
#include "../Header/Clock/Clock.h"
#include "../Header/Lrc/LyricParser.h"

using AudioUtil::SongMeta;
using LrcLine::LyricLine;
using LrcLine::LyricsList;

class MusicPlayerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MusicPlayerWidget(QWidget *parent = nullptr);
    ~MusicPlayerWidget() override;

    // ==================== 公开接口 ====================
    // 设置歌曲信息
    void setSongInfo(const SongMeta &info);

    // 设置歌词
    bool loadLyricsFromFile(const QString &filePath);
    bool loadLyricsFromContent(const QString &content);
    void clearLyrics();

    // 设置专辑封面
    void setAlbumCover(const QPixmap &cover);
    void setAlbumCoverFromPath(const QString &coverPath);

    // 设置播放状态和位置
    void setPlaying(bool playing);
    void setPosition(qint64 position);
    void setDuration(qint64 duration);

    // 获取当前数据
    SongMeta getCurrentSongInfo() const { return m_currentSongInfo; }
    LyricsList getLyrics() const;
    LrcLine::LyricLine getCurrentLyric() const;
    int getCurrentLyricIndex() const { return m_currentLyricIndex; }
    qint64 getCurrentPosition() const { return m_clock->getTimeMs(); }
    qint64 getDuration() const { return m_duration; }
    bool isPlaying() const { return !m_clock->isPaused(); }
    bool hasLyrics() const { return m_lyricParser && !m_lyricParser->isEmpty(); }
    bool hasCurrentSong() const { return m_hasCurrentSong; }

    // 播放控制
    void play();
    void pause();
    void stop();
    void seekForward(qint64 ms = 5000);
    void seekBackward(qint64 ms = 5000);

    // 新增：绑定到主时钟（AudioPlayer单例的m_clock）
    void bindToMasterClock(Clock* masterClock);

    // 清空所有数据
    void clearAllData();

signals:
    // ==================== 对外信号 ====================
    // 播放控制信号
    void playRequested();
    void pauseRequested();
    void stopRequested();
    void seekRequested(qint64 position);
    void prevRequested();
    void nextRequested();

    // 歌词点击跳转
    void lyricClicked(qint64 position);

    // 状态变化信号
    void currentSongChanged(const SongMeta &songInfo);
    void lyricsLoaded(bool success);
    void currentLyricChanged(const LrcLine::LyricLine &lyric);
    void positionChanged(qint64 position);
    void playStateChanged(bool isPlaying);
    void coverChanged(const QPixmap &cover);

    // 错误信号
    void errorOccurred(const QString &errorMsg);

public slots:
    // ==================== 公共槽函数 ====================
    void onMusicLoaded(const SongMeta &info);
    void onLyricsLoaded(const QString &filePath);
    void onPositionChanged(qint64 position);
    void onPlayStateChanged(bool isPlaying);
    void onCoverLoaded(const QPixmap &cover);
    void onErrorOccurred(const QString &errorMsg);

    void currentSongUpdated(const SongMeta &songMeta)
    {
        setSongInfo(songMeta);
    }

protected:
    // ==================== 事件重写 ====================
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // ==================== UI交互槽函数 ====================
    void onLyricItemClicked(QListWidgetItem *item);
    void onClockTimeChanged(double time);

    // 内部定时器槽
    void updateUIFromClock();

    // 新增：歌词滚动动画定时器槽
    void updateScrollAnimation();

private slots:
    // 异步歌词解析完成的回调（UI线程执行）
    void onLyricsParseFinished(bool success, const LyricsList& lines, const QString& errorMsg);

private:
    // ==================== 初始化函数 ====================
    void initUI();
    void initConnections();
    void initClock();

    // ==================== 歌词管理 ====================
    void updateLyricsList();
    void highlightCurrentLyric();
    void scrollToCurrentLyric();
    QString formatLyricText(const LrcLine::LyricLine &line) const;

    // 从LyricParser获取所有歌词行
    QVector<LrcLine::LyricLine> getLyricLines() const;

    // ==================== 封面处理 ====================
    QPixmap createRoundedCover(const QPixmap &original, int radius);

    // ==================== 时间处理 ====================
    QString formatTime(qint64 ms) const;
    void updateTimeDisplay();
    void updateTimeDisplayFromMs(qint64 ms);

    // ==================== 新增：固定中心高亮歌词功能 ====================
    // 歌词滚动相关
    void scrollLyricsToCurrentLine();
    void updateLyricsStyle();
    void setupLyricsViewport();

    // ==================== 播放状态 ====================
    bool m_hasCurrentSong = false;

    // ==================== 时间位置 ====================
    qint64 m_duration = 0;
    int m_currentLyricIndex = -1;
    LrcLine::LyricLine m_currentLyric;

    // ==================== 核心组件 ====================
    Clock* m_clock = nullptr;
    LyricParser* m_lyricParser = nullptr;

    // ==================== 歌曲数据 ====================
    SongMeta m_currentSongInfo;
    QPixmap m_currentCover;

    // ==================== 定时器 ====================
    QTimer *m_uiUpdateTimer = nullptr;

    // ==================== 新增：歌词滚动动画定时器 ====================
    QTimer *m_scrollTimer = nullptr;

    // ==================== 新增：歌词滚动动画控制 ====================
    int m_targetScrollPosition = 0;
    int m_scrollAnimationStep = 0;
    bool m_isScrolling = false;

    // ==================== 新增：歌词样式控制 ====================
    int m_currentCenterLine = -1;
    QColor m_highlightColor;
    int m_normalFontSize = 14;
    int m_centerFontSize = 20;

    // ==================== UI组件 ====================
    // 左侧专辑封面区域
    QFrame *m_leftFrame = nullptr;
    QLabel *m_coverLabel = nullptr;
    QLabel *m_coverPlaceholder = nullptr;

    // 右侧信息区域
    QFrame *m_rightFrame = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_artistLabel = nullptr;
    QLabel *m_albumLabel = nullptr;
    QLabel *m_durationLabel = nullptr;
    QLabel *m_currentTimeLabel = nullptr;

    // 歌词区域
    QListWidget *m_lyricsList = nullptr;

    // ==================== 新增：歌词显示区域组件 ====================
    QFrame *m_lyricsViewport = nullptr;      // 歌词显示视口
    QLabel *m_centerLineIndicator = nullptr; // 固定中心线指示器

    // 毛玻璃效果
    QGraphicsBlurEffect *m_blurEffect = nullptr;
    QPixmap m_parentBgCache;

    QPointer<LyricParserThreadPool> m_lyricThreadPool; // 歌词解析线程池（单例）
    LyricsList m_lyricsCache;                         // 缓存解析后的歌词（UI线程访问）
    bool m_loadingLyrics = false;                     // 标记是否正在加载歌词
    QString m_pendingLyricPath;                       // 待加载的歌词文件路径（防重复请求）

};

#endif // RINZEPLAYER_MUSICPLAYERWIDGET_H