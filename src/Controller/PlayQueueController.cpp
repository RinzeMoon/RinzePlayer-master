#include "../Header/Controller/PlayQueueController.h"
#include "../Header/AudioPart/AudioPlay/AudioPlayer.h"
#include "../Header/Manager/MusicSheetManager.h"
#include <QDebug>

PlayQueueController::PlayQueueController(QObject* parent)
    : QObject(parent)
{
    auto playlistManager = PlaylistManager::getInstance();

    // === 原有连接 ===
    connect(playlistManager, &PlaylistManager::songPlaylistUpdated,
            this, &PlayQueueController::onPlaylistUpdated);

    // === 新增：连接 PlaylistManager 的具体信号 ===
    connect(playlistManager, &PlaylistManager::songAdded,
            this, &PlayQueueController::onSongAdded);
    connect(playlistManager, &PlaylistManager::songRemoved,
            this, &PlayQueueController::onSongRemoved);
    connect(playlistManager, &PlaylistManager::songsCleared,
            this, &PlayQueueController::onSongsCleared);
    connect(playlistManager, &PlaylistManager::playlistReplaced,
            this, &PlayQueueController::onPlaylistReplaced);

    // 连接音频播放器信号
    auto player = AudioPlayer::getInstance();
    connect(player, &AudioPlayer::positionChanged,
            this, &PlayQueueController::onAudioPositionChanged);
    connect(player, &AudioPlayer::durationChanged,
            this, &PlayQueueController::onAudioDurationChanged);
    connect(player, &AudioPlayer::stateChanged,
            this, &PlayQueueController::onAudioStateChanged);
    connect(player,&AudioPlayer::progressUpdated,this,&PlayQueueController::onAudioPlayerProgressUpdated);

}

// === 原有槽函数实现 ===
void PlayQueueController::onPlaylistUpdated(const QList<SongMeta>& playlist)
{
    qDebug() << "[PlayQueue] 播放列表整体更新，大小:" << playlist.size();
    // 保持原有逻辑：整体替换播放队列
    m_playQueue = playlist;
    if (m_currentIndex >= m_playQueue.size()) {
        m_currentIndex = m_playQueue.isEmpty() ? -1 : 0;
    }
    emit playQueueUpdated(m_playQueue);
}

void PlayQueueController::onAudioPositionChanged(qint64 position)
{
    m_currentPosition = position;
    emit positionChangedToUi(position);
}

void PlayQueueController::onAudioDurationChanged(qint64 duration)
{
    m_currentDuration = duration;
    emit durationChanged(duration);
}

void PlayQueueController::onAudioStateChanged(PlayState state)
{
    qDebug() << "[PlayQueue] 音频状态变化:" << static_cast<int>(state);
    m_playbackState = state;
    emit playbackStateChanged(state);
}


// === 新增：PlaylistManager 具体信号槽 ===
void PlayQueueController::onSongAdded(const SongMeta& song, int index)
{
    qDebug() << "[PlayQueue] 收到歌曲添加信号，索引:" << index << "，歌曲:" << song.name;
    handleSongAdded(song, index);
}

void PlayQueueController::onSongRemoved(int index)
{
    qDebug() << "[PlayQueue] 收到歌曲删除信号，索引:" << index;
    handleSongRemoved(index);
}

void PlayQueueController::onSongsCleared()
{
    qDebug() << "[PlayQueue] 收到清空列表信号";
    handleSongsCleared();
}

void PlayQueueController::onPlaylistReplaced(const QList<SongMeta>& newList)
{
    qDebug() << "[PlayQueue] 收到列表替换信号，新列表大小:" << newList.size();
    // 直接使用整体更新逻辑
    onPlaylistUpdated(newList);
}

// === 新增：增量更新处理 ===
void PlayQueueController::handleSongAdded(const SongMeta& song, int index)
{
    // 边界检查
    if (index < 0 || index > m_playQueue.size()) {
        qDebug() << "[PlayQueue] 无效的添加索引:" << index;
        return;
    }

    // 插入到播放队列
    m_playQueue.insert(index, song);

    // 调整当前播放索引
    if (m_currentIndex >= index) {
        m_currentIndex++;
    }

    qDebug() << "[PlayQueue] 增量添加歌曲，当前队列大小:" << m_playQueue.size();
    emit playQueueUpdated(m_playQueue);
}

void PlayQueueController::handleSongRemoved(int index)
{
    // 边界检查
    if (index < 0 || index >= m_playQueue.size()) {
        qDebug() << "[PlayQueue] 无效的删除索引:" << index;
        return;
    }

    // 检查是否删除的是当前播放的歌曲
    bool removedCurrentSong = (index == m_currentIndex);

    // 从播放队列删除
    m_playQueue.removeAt(index);

    // 调整当前播放索引
    if (removedCurrentSong) {
        // 如果删除的是当前歌曲，停止播放并重置索引
        stop();
        m_currentIndex = -1;
    } else if (m_currentIndex > index) {
        // 如果删除的歌曲在当前歌曲之前，调整索引
        m_currentIndex--;
    }

    qDebug() << "[PlayQueue] 增量删除歌曲，当前队列大小:" << m_playQueue.size()
             << "，当前索引:" << m_currentIndex;
    emit playQueueUpdated(m_playQueue);
}

void PlayQueueController::handleSongsCleared()
{
    // 清空播放队列
    m_playQueue.clear();
    m_currentIndex = -1;

    // 停止播放
    stop();

    qDebug() << "[PlayQueue] 增量清空队列";
    emit playQueueUpdated(m_playQueue);
}

// === 原有方法实现 ===
void PlayQueueController::syncFromPlaylist()
{
    auto playlist = PlaylistManager::getInstance()->getSongPlaylist();
    qDebug() << "[PlayQueue] 手动同步播放列表，大小:" << playlist.size();
    setPlayQueue(playlist);
}

void PlayQueueController::setPlayQueue(const QList<SongMeta>& songs, int startIndex)
{
    m_playQueue = songs;
    m_currentIndex = qBound(-1, startIndex, m_playQueue.size() - 1);
    emit playQueueUpdated(m_playQueue);

    if (m_currentIndex >= 0) {
        playCurrentSong();
    }
}

void PlayQueueController::playCurrentSong()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_playQueue.size()) {
        qDebug() << "[PlayQueue] 播放当前歌曲失败：索引无效" << "当前索引" << m_currentIndex;
        return;
    }

    auto& song = m_playQueue[m_currentIndex];
    auto player = AudioPlayer::getInstance();

    emit sendCurrentSongMeta(song);
    qDebug() << "[PlayQueue] 发送正在播放的songMeta至Ui";
    if (player->play(song.filePath)) {
        m_playbackState = PlayState::Playing;
        emit currentSongChanged(song);
        loadCoverForCurrentSong();

        qDebug() << "[PlayQueue] 开始播放:" << song.name;
    } else {
        qDebug() << "[PlayQueue] 播放失败:" << song.filePath;
    }
}

void PlayQueueController::loadCoverForCurrentSong()
{
    if (m_currentIndex < 0) return;

    auto& song = m_playQueue[m_currentIndex];
    QPixmap cover = QPixmap(song.coverUrl);

    if (cover.isNull()) {
        cover.load(":/images/default_cover.png");
    }

    emit coverChanged(cover);
}


const SongMeta* PlayQueueController::getCurrentSong() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_playQueue.size()) {
        return &m_playQueue[m_currentIndex];
    }
    return nullptr;
}

void PlayQueueController::play()
{
    if (m_playQueue.isEmpty()) {
        qDebug() << "[PlayQueue] 播放失败：队列为空";
        return;
    }

    if (m_currentIndex < 0) {
        m_currentIndex = 0;
    }
    playCurrentSong();
}

void PlayQueueController::resume()
{
    auto player = AudioPlayer::getInstance();
    player->resume();
}

void PlayQueueController::pause()
{
    auto player = AudioPlayer::getInstance();
    player->pause();
    qDebug() << "[PlayQueue] 暂停播放";
}

void PlayQueueController::stop()
{
    auto player = AudioPlayer::getInstance();
    player->stop();
    m_playbackState = PlayState::Stopped;
    emit playbackStateChanged(m_playbackState);
    qDebug() << "[PlayQueue] 停止播放";
}

void PlayQueueController::next()
{
    qDebug() << "[PlayQueue] 下一首,请求索引" << m_currentIndex;
    if (m_playQueue.isEmpty())
    {
        qDebug() << "[PlayQueue] 队列为空,无法继续";
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    if (m_lastSwitchTime.isValid() && m_lastSwitchTime.msecsTo(now) < 500) {
        qDebug() << "防抖：500ms内重复切换，忽略";
        return;
    }
    m_lastSwitchTime = now;

    // 2. 状态检查
    if (m_isSwitching || m_Seeking) {
        qDebug() << "状态冲突：正在操作中，忽略";
        qDebug() << "Switch/Seek" << m_isSwitching <<"/" << m_Seeking;
        return;
    }

    // 4. 标记状态
    m_isSwitching = true;
    qDebug() << "开始切换歌曲";

    if (m_playbackState == RinGlobal::PlayState::Playing)
    {
        qDebug() << "将要调用暂停播放";
    }

    int nextIndex = -1;

    switch (m_playMode) {
    case PlayMode::Sequential:
        nextIndex = (m_currentIndex + 1) % m_playQueue.size();
        break;
    case PlayMode::Random:
        nextIndex = QRandomGenerator::global()->bounded(m_playQueue.size());
        break;
    case PlayMode::LoopSingle:
        nextIndex = m_currentIndex;
        break;
    }

    int newIndex = nextIndex;

    if (newIndex >= m_playQueue.size()) {
        newIndex = 0;  // 循环到开头
        qDebug() << "循环播放，回到第一首";
    }

    // 7. 重置当前位置
    m_currentPosition = 0;

    QTimer::singleShot(100, this, [this, newIndex]() {
            // 再次检查索引有效性
            if (newIndex < 0 || newIndex >= m_playQueue.size()) {
                qDebug() << "错误：新索引无效" << newIndex;
                m_isSwitching = false;
                return;
            }

            // 更新索引
            m_currentIndex = newIndex;

            // 获取新歌曲信息
            const SongMeta meta = m_playQueue[newIndex];
            m_currentDuration = SongMeta::parseDurationString(meta.duration);

            qDebug() << "切换到新歌:" << meta.name
                       << "索引:" << newIndex
                       << "时长:" << meta.duration;

            // 关键：按顺序发送信号（避免UI竞争）
            // 1. 先发送元数据
            emit sendCurrentSongMeta(meta);

            // 2. 延迟发送时长信息
            QTimer::singleShot(50, this, [this, meta]() {
                emit durationChanged(m_currentDuration);

                // 3. 延迟恢复播放状态
                QTimer::singleShot(50, this, [this]() {
                    // 恢复之前的播放状态（如果是播放中）
                    if (m_playbackState == RinGlobal::PlayState::Playing) {
                        emit playbackStateChanged(RinGlobal::PlayState::Playing);
                    }

                    playCurrentSong();

                    // 完成切换
                    m_isSwitching = false;
                    m_Seeking = false;
                    qDebug() << "歌曲切换完成";
                });
            });
        });

    /*  需要考虑
    if (nextIndex >= 0) {
        m_currentIndex = nextIndex;
        playCurrentSong();
    }
    */
    m_isSwitching = false;
}

void PlayQueueController::previous()
{
    if (m_playQueue.isEmpty()) return;

    QDateTime now = QDateTime::currentDateTime();
    if (m_lastSwitchTime.isValid() && m_lastSwitchTime.msecsTo(now) < 500) {
        qDebug() << "防抖：500ms内重复切换，忽略";
        return;
    }
    m_lastSwitchTime = now;

    // 2. 状态检查
    if (m_isSwitching || m_Seeking) {
        qDebug() << "状态冲突：正在操作中，忽略";
        qDebug() << "Switch/Seek" << m_isSwitching <<"/" << m_Seeking;
        return;
    }

    // 3. 队列检查
    if (m_playQueue.isEmpty()) {
        qDebug() << "错误：播放队列为空";
        return;
    }

    // 4. 标记状态
    m_isSwitching = true;
    qDebug() << "开始切换歌曲";

    if (m_playbackState == RinGlobal::PlayState::Playing)
    {
        qDebug() << "将要调用暂停播放";
    }

    int nextIndex = -1;

    switch (m_playMode) {
    case PlayMode::Sequential:
        nextIndex = (m_currentIndex - 1) % m_playQueue.size();
        break;
    case PlayMode::Random:
        nextIndex = QRandomGenerator::global()->bounded(m_playQueue.size());
        break;
    case PlayMode::LoopSingle:
        nextIndex = m_currentIndex;
        break;
    }

    int newIndex = nextIndex;

    m_currentIndex = newIndex;

    QTimer::singleShot(100, this, [this, newIndex]() {
            // 再次检查索引有效性
            if (newIndex < 0 || newIndex >= m_playQueue.size()) {
                qDebug() << "错误：新索引无效" << newIndex;
                m_isSwitching = false;
                return;
            }

            // 更新索引
            m_currentIndex = newIndex;

            // 获取新歌曲信息
            const SongMeta meta = m_playQueue[newIndex];
            m_currentDuration = SongMeta::parseDurationString(meta.duration);

            qDebug() << "切换到新歌:" << meta.name
                       << "索引:" << newIndex
                       << "时长:" << meta.duration;

            // 关键：按顺序发送信号（避免UI竞争）
            // 1. 先发送元数据
            emit sendCurrentSongMeta(meta);

            // 2. 延迟发送时长信息
            QTimer::singleShot(50, this, [this, meta]() {
                emit durationChanged(m_currentDuration);

                // 3. 延迟恢复播放状态
                QTimer::singleShot(50, this, [this]() {
                    // 恢复之前的播放状态（如果是播放中）
                    if (m_playbackState == RinGlobal::PlayState::Playing) {
                        emit playbackStateChanged(RinGlobal::PlayState::Playing);
                    }

                    playCurrentSong();

                    // 完成切换
                    m_isSwitching = false;
                    m_Seeking = false;
                    qDebug() << "歌曲切换完成";
                });
            });
        });

    m_isSwitching = false;
}

void PlayQueueController::seek(qint64 position)
{
    qDebug() << "[PlayQueue] 跳转到进度:" << position;
    QDateTime now = QDateTime::currentDateTime();

    if (m_lastSeekTime.isValid() && m_lastSeekTime.msecsTo(now) < 100)
    {
        // 100ms 进行防抖操作
        qDebug() << "触发防抖机制";
        return;
    }
    m_lastSeekTime = now;

    if (m_Seeking)
    {
        qDebug() << "Seeking中";
        return;
    }

    if (m_isSwitching) {
        qDebug() << "状态冲突：正在切换歌曲";
        return;
    }

    if (position < 0) {
        position = 0;
        qDebug() << "修正：负数位置改为0";
    }

    m_Seeking = true;
    qDebug() << "开始Seek";

    m_currentPosition = position;

    float progress = static_cast<float>(m_currentPosition / m_currentDuration);
    emit progressUpdated(progress,m_currentPosition,m_currentDuration);

    QtConcurrent::run([this, position]() {
         qDebug() << "[Seek] 开始音频引擎跳转";

         auto player = AudioPlayer::getInstance();
         player->seek(position); // 假设这是耗时操作

         // 跳转完成后，在主线程更新状态
         QMetaObject::invokeMethod(this, [this]() {
             m_Seeking = false;
             qDebug() << "[Seek] 完成，状态已重置";
         }, Qt::QueuedConnection);
     });
}

void PlayQueueController::jumpToIndex(int index)
{
    if (index >= 0 && index < m_playQueue.size()) {
        qDebug() << "[PlayQueue] 跳转到索引:" << index;
        m_currentIndex = index;
        playCurrentSong();
    } else {
        qDebug() << "[PlayQueue] 跳转索引无效:" << index;
    }
}

void PlayQueueController::setPlayMode(PlayMode mode)
{
    m_playMode = mode;
    qDebug() << "[PlayQueue] 设置播放模式:" << static_cast<int>(mode);
}

void PlayQueueController::clearQueue()
{
    m_playQueue.clear();
    m_currentIndex = -1;
    emit playQueueUpdated(m_playQueue);
    stop();
    qDebug() << "[PlayQueue] 清空播放队列";
}

void PlayQueueController::connectWithAudioPlayer()
{
    AudioPlayer* player = AudioPlayer::getInstance();

    // 连接AudioPlayer::progressUpdated
    connect(player, &AudioPlayer::progressUpdated,
            this, &PlayQueueController::onAudioPlayerProgressUpdated);

    // 连接AudioPlayer::errorOccurred
    connect(player, &AudioPlayer::errorOccurred,
            this, &PlayQueueController::onAudioPlayerErrorOccurred);

    // 连接AudioPlayer::playbackFinished
    connect(player, &AudioPlayer::playbackFinished,
            this, &PlayQueueController::onAudioPlayerPlaybackFinished);

    // 连接AudioPlayer::currentSongFinished
    connect(player, &AudioPlayer::currentSongFinished,
            this, &PlayQueueController::onAudioPlayerCurrentSongFinished);
}