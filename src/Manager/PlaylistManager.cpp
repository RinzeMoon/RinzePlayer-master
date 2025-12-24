#include "../../Header/Manager/PlaylistManager.h"
#include <QDebug>
#include <algorithm>

// === 私有同步方法 ===
void PlaylistManager::syncAudioPlaylistFromSongPlaylist() {
    m_audioPlaylist.clear();
    for (const auto& song : m_playlist) {
        m_audioPlaylist.append(song.getOriginalAudioMeta());
    }
}

void PlaylistManager::syncSongPlaylistFromAudioPlaylist() {
    m_playlist.clear();
    for (const auto& audio : m_audioPlaylist) {
        m_playlist.append(SongMeta(audio));
    }
}

// === 音频列表操作 ===
void PlaylistManager::addAudio(const AudioUtil::AudioMeta& meta) {
    // 1. 去重检查
    for (const auto& existing : m_audioPlaylist) {
        if (existing.filePath == meta.filePath) {
            qDebug() << "[PlaylistManager] 歌曲已存在";
            return;
        }
    }

    // 2. 添加到两个数据源
    m_audioPlaylist.append(meta);
    AudioUtil::SongMeta songMeta(meta);
    m_playlist.append(songMeta);

    int addedIndex = m_playlist.size() - 1;

    qDebug() << "[PlaylistManager] 添加歌曲成功：" << meta.title
             << "，索引：" << addedIndex
             << "，音频列表大小：" << m_audioPlaylist.size()
             << "，歌曲列表大小：" << m_playlist.size();

    // 3. 发射信号
    emit songAdded(songMeta, addedIndex);
    emit playlistUpdated();
    emit songPlaylistUpdated(m_playlist);
}

void PlaylistManager::clearAll() {
    m_playlist.clear();
    m_audioPlaylist.clear();

    qDebug() << "[PlaylistManager] 清空所有列表";

    // 发射信号
    emit songsCleared();
    emit playlistUpdated();
    emit songPlaylistUpdated(m_playlist);
    emit songPlaylistCleared();
}

void PlaylistManager::refresh() {
    // 保留原有刷新逻辑
    qDebug() << "[PlaylistManager] 刷新播放列表";
    emit playlistRefreshed();
}

// === 播放列表管理 ===
void PlaylistManager::setPlaylist(const QList<SongMeta>& playlist) {
    m_playlist = playlist;
    syncAudioPlaylistFromSongPlaylist();

    qDebug() << "[PlaylistManager] 设置播放列表，歌曲数量：" << m_playlist.size();

    emit playlistReplaced(m_playlist);
    emit songPlaylistUpdated(m_playlist);
}

void PlaylistManager::addToPlaylist(const QList<SongMeta>& songs) {
    int startIndex = m_playlist.size();
    m_playlist.append(songs);

    // 同步到音频列表
    for (const auto& song : songs) {
        m_audioPlaylist.append(song.getOriginalAudioMeta());
    }

    qDebug() << "[PlaylistManager] 添加" << songs.size() << "首歌曲到播放列表";

    // 发射逐个添加信号
    for (int i = 0; i < songs.size(); ++i) {
        emit songAdded(songs[i], startIndex + i);
    }
    emit songPlaylistUpdated(m_playlist);
}

void PlaylistManager::clearPlaylist() {
    m_playlist.clear();
    m_audioPlaylist.clear();

    qDebug() << "[PlaylistManager] 清空播放列表";

    emit songsCleared();
    emit songPlaylistCleared();
    emit songPlaylistUpdated(m_playlist);
}

// === 歌单导入 ===
void PlaylistManager::loadMusicSheetToPlaylist(const MusicSheet& sheet) {
    if (sheet.songs.isEmpty()) {
        qDebug() << "[PlaylistManager] 歌单为空，不加载";
        return;
    }

    m_playlist = sheet.songs;
    syncAudioPlaylistFromSongPlaylist();

    qDebug() << "[PlaylistManager] 加载歌单到播放列表：" << sheet.title
             << "，歌曲数量：" << m_playlist.size();

    emit playlistReplaced(m_playlist);
    emit songPlaylistUpdated(m_playlist);
}

void PlaylistManager::loadMusicSheetByIdToPlaylist(const QString& sheetId) {
    auto sheet = MusicSheetManager::getInstance()->getSheetById(sheetId);
    if (!sheet.id.isEmpty()) {
        loadMusicSheetToPlaylist(sheet);
    } else {
        qDebug() << "[PlaylistManager] 未找到ID为" << sheetId << "的歌单";
    }
}

// === 单个歌曲操作 ===
void PlaylistManager::addSong(const SongMeta& song) {
    int index = m_playlist.size();
    m_playlist.append(song);
    m_audioPlaylist.append(song.getOriginalAudioMeta());

    qDebug() << "[PlaylistManager] 添加单曲：" << song.name << "，索引：" << index;

    emit songAdded(song, index);
    emit songPlaylistUpdated(m_playlist);
}

void PlaylistManager::removeSong(int index) {
    if (index >= 0 && index < m_playlist.size()) {
        SongMeta removedSong = m_playlist.at(index);

        m_playlist.removeAt(index);
        m_audioPlaylist.removeAt(index);

        qDebug() << "[PlaylistManager] 删除索引" << index << "的歌曲：" << removedSong.name;

        emit songRemoved(index);
        emit songRemovedById(removedSong.id);
        emit songPlaylistUpdated(m_playlist);
    } else {
        qDebug() << "[PlaylistManager] 删除索引无效：" << index;
    }
}

void PlaylistManager::removeSongById(const QString& songId) {
    for (int i = 0; i < m_playlist.size(); ++i) {
        if (m_playlist[i].id == songId) {
            removeSong(i);
            return;
        }
    }
    qDebug() << "[PlaylistManager] 未找到ID为" << songId << "的歌曲";
}