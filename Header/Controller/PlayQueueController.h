#ifndef RINZEPLAYER_PLAYQUEUECONTROLLER_H
#define RINZEPLAYER_PLAYQUEUECONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QRandomGenerator>
#include <QtConcurrent/QtConcurrent>

#include "../Global/Global.h"
#include "../Header/Manager/PlaylistManager.h"

using AudioUtil::SongMeta;
using RinGlobal::PlayState;
using RinGlobal::PlayMode;

class PlayQueueController : public QObject
{
    Q_OBJECT

public:
    static PlayQueueController* getInstance()
    {
        static PlayQueueController instance;
        return &instance;
    }

    PlayQueueController(const PlayQueueController&) = delete;
    PlayQueueController& operator=(const PlayQueueController&) = delete;

    // === 队列管理 ===
    void syncFromPlaylist(); // 从播放列表同步到播放队列
    void setPlayQueue(const QList<SongMeta>& songs, int startIndex = 0);
    void clearQueue();

    // === 播放控制 ===
    void play();
    void pause();
    void stop();
    void resume();
    void next();
    void previous();
    void seek(qint64 position);
    void jumpToIndex(int index);

    // === 状态获取 ===
    const SongMeta* getCurrentSong() const;
    PlayMode getPlayMode() const { return m_playMode; }
    void setPlayMode(PlayMode mode);
    PlayState getPlaybackState() const { return m_playbackState; }

    const QList<SongMeta>& getPlayQueue() const { return m_playQueue; }
    int getCurrentIndex() const { return m_currentIndex; }
    qint64 getCurrentPosition() const { return m_currentPosition; }
    qint64 getCurrentDuration() const { return m_currentDuration; }

signals:
    // 核心状态信号
    void currentSongChanged(const SongMeta& song);
    void playbackStateChanged(PlayState state);
    void positionChangedToUi(qint64 position);
    void durationChanged(qint64 duration);

    // 队列和封面信号
    void playQueueUpdated(const QList<SongMeta>& queue);
    void coverChanged(const QPixmap& cover);

    void sendCurrentSongMeta(const SongMeta& sMeta);

    // ================== 新添加的信号 ==================
    // 对应AudioPlayer::progressUpdated
    void progressUpdated(float progress, int64_t currentMs, int64_t totalMs);

    // 对应AudioPlayer::errorOccurred
    void errorOccurred(const QString &errorMsg);

    // 对应AudioPlayer::playbackFinished
    void playbackFinished();

    // 对应AudioPlayer::currentSongFinished
    void currentSongFinished();
    // ================== 结束新添加 ==================

private slots:
    // === 原有槽函数 ===
    void onPlaylistUpdated(const QList<SongMeta>& playlist);
    void onAudioPositionChanged(qint64 position);
    void onAudioDurationChanged(qint64 duration);
    void onAudioStateChanged(PlayState state);

    // === 连接 PlaylistManager 的具体信号 ===
    void onSongAdded(const SongMeta& song, int index);
    void onSongRemoved(int index);
    void onSongsCleared();
    void onPlaylistReplaced(const QList<SongMeta>& newList);

    // ================== 新添加的槽函数 ==================
    // 对应AudioPlayer::progressUpdated
    void onAudioPlayerProgressUpdated(float progress, int64_t currentMs, int64_t totalMs)
    {
        emit progressUpdated(progress, currentMs, totalMs);
    }

    // 对应AudioPlayer::errorOccurred
    void onAudioPlayerErrorOccurred(const QString &errorMsg)
    {
        qDebug() << "[FatalError]" << errorMsg;
    }

    // 对应AudioPlayer::playbackFinished
    void onAudioPlayerPlaybackFinished()
    {
        emit playbackFinished();
    }

    // 对应AudioPlayer::currentSongFinished
    void onAudioPlayerCurrentSongFinished()
    {
        emit currentSongFinished();
    }

    void onCurrentPositonChanged(qint64 positon)
    {

    }
    // ================== 结束新添加 ==================

private:
    PlayQueueController(QObject* parent = nullptr);

    void connectWithAudioPlayer();

    void playCurrentSong();
    void loadCoverForCurrentSong();

    // === 新增：增量更新方法 ===
    void handleSongAdded(const SongMeta& song, int index);
    void handleSongRemoved(int index);
    void handleSongsCleared();

    QList<SongMeta> m_playQueue;      // 播放队列
    int m_currentIndex = -1;
    PlayMode m_playMode = PlayMode::Sequential;
    PlayState m_playbackState = PlayState::Stopped;

    // 进度信息
    qint64 m_currentPosition = 0;
    qint64 m_currentDuration = 0;

    //渐进式改动方案
    bool m_isPlaying = false;
    bool m_isSwitching = false;
    bool m_isLoading = false;
    bool m_Seeking = false;

    // 新增：防抖计时
    QDateTime m_lastSeekTime;
    QDateTime m_lastSwitchTime;
};

#endif // RINZEPLAYER_PLAYQUEUECONTROLLER_H