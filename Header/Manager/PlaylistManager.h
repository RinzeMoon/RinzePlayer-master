#ifndef RINZEPLAYER_PLAYLISTMANAGER_H
#define RINZEPLAYER_PLAYLISTMANAGER_H

#include <QObject>
#include <QList>

#include "../Global/Global.h"
#include "../Header/Manager/MusicSheetManager.h"

using AudioUtil::MusicSheet;
using AudioUtil::SongMeta;
using AudioUtil::AudioMeta;

class PlaylistManager : public QObject
{
    Q_OBJECT

public:
    static PlaylistManager* getInstance() {
        static PlaylistManager instance;
        return &instance;
    }

    PlaylistManager(const PlaylistManager&) = delete;
    PlaylistManager& operator=(const PlaylistManager&) = delete;

    // === 数据访问接口 ===
    const QList<AudioMeta>& getPlaylist() const { return m_audioPlaylist; }
    const QList<SongMeta>& getSongPlaylist() const { return m_playlist; }

    // === 音频列表操作 ===
    void addAudio(const AudioMeta& meta);
    void clearAll();
    void refresh();

    // === 播放列表管理 ===
    void setPlaylist(const QList<SongMeta>& playlist);
    void addToPlaylist(const QList<SongMeta>& songs);
    void clearPlaylist();

    // === 歌单导入 ===
    void loadMusicSheetToPlaylist(const MusicSheet& sheet);
    void loadMusicSheetByIdToPlaylist(const QString& sheetId);

    // === 单个歌曲操作 ===
    void addSong(const SongMeta& song);
    void removeSong(int index);
    void removeSongById(const QString& songId);

signals:
    // === 原有信号 ===
    void playlistUpdated();
    void playlistRefreshed();
    void songPlaylistUpdated(const QList<SongMeta>& playlist);
    void songPlaylistCleared();

    // === 新增：具体变更信号 ===
    void songAdded(const SongMeta& song, int index);
    void songRemoved(int index);
    void songRemovedById(const QString& songId);
    void songsCleared();
    void playlistReplaced(const QList<SongMeta>& newList);

private:
    PlaylistManager(QObject* parent = nullptr) : QObject(parent) {}

    // 保持两个数据源的同步
    void syncAudioPlaylistFromSongPlaylist();
    void syncSongPlaylistFromAudioPlaylist();

    QList<AudioMeta> m_audioPlaylist; // 播放用数据源
    QList<SongMeta> m_playlist;       // UI展示用数据源
};

#endif // RINZEPLAYER_PLAYLISTMANAGER_H