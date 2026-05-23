// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mediacontroller.h"
#include "sync_manager.h"
#include "../streaming/room_session.h"
#include "ui/videowidget.h"
#include "ui/pipwindow.h"
#include "clock/globalclock.h"
#include "playbackstats.h"
#include <QFileInfo>
#include <functional>
#include <cmath>
#include <thread>

namespace {
    void clearPktQ(sharedPktQueue pktq) {
        if (!pktq) {
            return;
        }
        AVPktItem tmp;
        while (pktq->pop(tmp)) {
            av_packet_free(&tmp.pkt);
        }
    }
    void clearFrmQ(sharedFrmQueue frmq) {
        if (!frmq) {
            return;
        }
        AVFrmItem tmp;
        while (frmq->pop(tmp)) {
            av_frame_free(&tmp.frm);
        }
    }
}

MediaController::MediaController(QObject *parent)
    : QObject{parent} {

    m_demux = new Demux(parent);
    m_decodeAudio = new DecodeAudio(parent);
    m_decodeVideo = new DecodeVideo(parent);
    m_decodeSubtitl = new DecodeSubtitle(parent);
    m_audioPlayer = new AudioPlayer(parent);
    m_videoPlayer = new VideoPlayer(parent);

    // 初始化流管理器
    m_streamManager = new StreamManager(this);
    StreamingConfig config;
    m_streamManager->init(config);
    m_streamManager->setDemux(m_demux);

    // 连接流媒体信号
    connect(m_streamManager, &StreamManager::stateChanged, this, &MediaController::streamStateChanged);
    connect(m_streamManager, &StreamManager::bufferProgressChanged, this, &MediaController::bufferProgress);
    connect(m_streamManager, &StreamManager::error, this, &MediaController::streamError);
    connect(m_streamManager, &StreamManager::reconnecting, this, &MediaController::reconnecting);

    QObject::connect(m_audioPlayer, &AudioPlayer::seeked, this, &MediaController::seeked);
    QObject::connect(m_videoPlayer, &VideoPlayer::seeked, this, &MediaController::seeked);
    QObject::connect(m_videoPlayer, &VideoPlayer::renderDataReady, this, &MediaController::onRenderDataReady);
    QObject::connect(this, &MediaController::seeked, this, [&]() {
        m_played = false;
    });

    QObject::connect(m_videoPlayer, &VideoPlayer::playedOneFrame, this, [&]() {
        setProgress(getCurrentTime());
        m_progressFallbackTimer.start(1000);
    });
    QObject::connect(m_audioPlayer, &AudioPlayer::playedOneFrame, this, [&]() {
        setProgress(getCurrentTime());
        m_progressFallbackTimer.start(1000);
    });

    QObject::connect(&m_progressFallbackTimer, &QTimer::timeout, this, [&]() {
        setProgress(getCurrentTime());
    });
    m_progressFallbackTimer.start(1000);

    QObject::connect(&m_updatePktAndFrmQueueSizeTimer, &QTimer::timeout, this, [&]() {
        PlaybackStats::instance().audioPacketCount = m_pktAudioBuf->size();
        PlaybackStats::instance().videoPacketCount = m_pktVideoBuf->size();
        PlaybackStats::instance().subtitlePacketCount = m_pktSubtitleBuf->size();
        PlaybackStats::instance().audioFrameCount = m_frmAudioBuf->size();
        PlaybackStats::instance().videoFrameCount = m_frmVideoBuf->size();
        PlaybackStats::instance().subtitleFrameCount = m_frmSubtitleBuf->size();
    });
    m_updatePktAndFrmQueueSizeTimer.start(1000);

    QObject::connect(&m_checkPlayerFinishedTimer, &QTimer::timeout, this, &MediaController::checkPlayerFinished);
    m_checkPlayerFinishedTimer.start(100);
}

MediaController::~MediaController()
{
    m_checkPlayerFinishedTimer.stop();
    m_updatePktAndFrmQueueSizeTimer.stop();
    m_progressFallbackTimer.stop();
    close();
}

void MediaController::setSyncManager(SyncManager *sm)
{
    m_syncManager = sm;
    if (m_syncManager) {
        m_syncManager->setMediaController(this);
        connect(m_syncManager, &SyncManager::remotePlay, this, [this]() {
            if (m_paused) {
                m_videoPlayer->togglePaused();
                m_audioPlayer->togglePaused();
                GlobalClock::instance().togglePaused();
                setPaused(false);
            }
        });
        connect(m_syncManager, &SyncManager::remotePause, this, [this]() {
            if (!m_paused) {
                m_videoPlayer->togglePaused();
                m_audioPlayer->togglePaused();
                GlobalClock::instance().togglePaused();
                setPaused(true);
            }
        });
    }
}

void MediaController::setRoomSession(RoomSession *rs)
{
    m_roomSession = rs;
}

double MediaController::getBitrate() const {
    if (!m_demux || !m_demux->formatCtx()) {
        return 0.0;
    }
    // 从视频流获取码率
    AVStream* videoStream = m_demux->getStream(MediaType::Video);
    if (videoStream && videoStream->codecpar) {
        if (videoStream->codecpar->bit_rate > 0) {
            return videoStream->codecpar->bit_rate / 1000.0; // 转换为kbps
        }
    }
    // 从音频流获取码率
    AVStream* audioStream = m_demux->getStream(MediaType::Audio);
    if (audioStream && audioStream->codecpar) {
        if (audioStream->codecpar->bit_rate > 0) {
            return audioStream->codecpar->bit_rate / 1000.0; // 转换为kbps
        }
    }
    return 0.0;
}

double MediaController::getSyncDifference() const {
    double audioPts = GlobalClock::instance().audioPts();
    double videoPts = GlobalClock::instance().videoPts();
    if (std::isnan(audioPts) || std::isnan(videoPts)) {
        return 0.0;
    }
    return videoPts - audioPts;
}

bool MediaController::isLocalFile() const {
    if (m_streamManager) {
        return m_streamManager->stats().protocol == StreamProtocol::LocalFile;
    }
    return m_URL.isLocalFile();
}

void MediaController::setRenderCallback(std::function<void(VideoDoubleBuf*, SubtitleDoubleBuf*)> callback) {
    m_renderCallback = callback;
}

void MediaController::onRenderDataReady(VideoDoubleBuf* vid, SubtitleDoubleBuf* sub) {
    m_lastVidBuf = vid;
    m_lastSubBuf = sub;

    if (m_renderCallback) {
        m_renderCallback(vid, sub);
    }

    // 如果画中画窗口存在且可见，也更新画中画
    if (m_pipWindow && m_pipWindow->isVisible()) {
        m_pipWindow->setVideoData(vid, sub);
    }
}

// 需要额外的方法来设置VideoWidget，以便控制画中画
void MediaController::setVideoWidget(VideoWidget* widget) {
    m_videoWidget = widget;
}

bool MediaController::open(const QUrl &URL) {
    QString openPath;
    m_URL = URL;

    qDebug() << "=== 开始打开媒体 ===";
    qDebug() << "完整URL:" << URL;
    qDebug() << "是否本地文件:" << URL.isLocalFile();

    // 预检查本地文件
    if (URL.isLocalFile()) {
        openPath = URL.toLocalFile();
        qDebug() << "本地路径:" << openPath;
        QFileInfo checkFile(openPath);
        
        if (!checkFile.exists()) {
            QString error = QString("文件不存在: %1").arg(openPath);
            qDebug() << error;
            m_streamManager->onDemuxError(error);
            return false;
        }
        
        if (!checkFile.isReadable()) {
            QString error = QString("没有读取权限: %1").arg(openPath);
            qDebug() << error;
            m_streamManager->onDemuxError(error);
            return false;
        }
        
        if (checkFile.size() == 0) {
            QString error = QString("文件为空: %1").arg(openPath);
            qDebug() << error;
            m_streamManager->onDemuxError(error);
            return false;
        }
        
        qDebug() << "文件大小:" << checkFile.size() / 1024.0 / 1024.0 << "MB";
    } else {
        openPath = URL.toString();
        qDebug() << "网络流URL:" << openPath;
        if (openPath.isEmpty()) {
            QString error = "无效的URL";
            m_streamManager->onDemuxError(error);
            return false;
        }
    }

    // 先让流管理器处理
    m_streamManager->open(URL);

    if (m_opened) {
        close();
    }

    PlaybackStats::instance().reset();

    // 设置低延迟模式
    m_demux->setLowLatencyMode(m_lowLatencyMode);
    m_decodeAudio->setLowLatencyMode(m_lowLatencyMode);
    m_decodeVideo->setLowLatencyMode(m_lowLatencyMode);
    m_decodeSubtitl->setLowLatencyMode(m_lowLatencyMode);

    // 本地文件同步打开，网络流异步打开不阻塞UI
    if (URL.isLocalFile()) {
        bool ok = true;
        qDebug() << "调用Demux::init，传递的路径:" << openPath;
        ok &= m_demux->init(openPath.toUtf8().constData(), true);
        if (!ok) {
            QString error = QString("无法解析媒体文件: %1").arg(openPath);
            qDebug() << "Demux初始化失败!";
            m_streamManager->onDemuxError("Failed to initialize demux for: " + openPath);
            close();
            return false;
        }

        m_streamManager->onDemuxConnected();

        bool haveAudio = m_demux->haveStream(MediaType::Audio), haveVideo = m_demux->haveStream(MediaType::Video);
        qDebug() << "打开媒体" << openPath << "haveAudio:" << haveAudio << "haveVideo:" << haveVideo;
        if (haveAudio) {
            const int audioIdx = 0;
            m_demux->switchAudioStream(audioIdx, m_pktAudioBuf, m_frmAudioBuf);
            m_currentAudioStream = {0, audioIdx};
            ok &= m_decodeAudio->init(m_demux->getStream(MediaType::Audio), m_pktAudioBuf, m_frmAudioBuf, 1);
            ok &= m_audioPlayer->init(m_demux->getStream(MediaType::Audio)->codecpar, m_frmAudioBuf);
        }

        if (haveVideo) {
            const int videoIdx = 0;
            m_currentVideoStream = {0, videoIdx};
            m_demux->switchVideoStream(videoIdx, m_pktVideoBuf, m_frmVideoBuf);
            int cores = std::thread::hardware_concurrency();
            ok &= m_decodeVideo->init(m_demux->getStream(MediaType::Video), m_pktVideoBuf, m_frmVideoBuf, cores >= 6 ? 6 : 0);
        }

        if (haveVideo && m_demux->haveStream(MediaType::Subtitle)) {
            const int subtitleIdx = 0;
            m_currentSubtitleStream = {0, subtitleIdx};
            m_demux->switchSubtitleStream(subtitleIdx, m_pktSubtitleBuf, m_frmSubtitleBuf);
            ok &= m_decodeSubtitl->init(m_demux->getStream(MediaType::Subtitle), m_pktSubtitleBuf, m_frmSubtitleBuf, 1);
        }

        DeviceStatus::instance().setHaveAudio(haveAudio);
        DeviceStatus::instance().setHaveVideo(haveVideo);

        setDuration(m_demux->getDuration());
        setProgress(0);
        GlobalClock::instance().reset();
        // 有音频时主时钟为Audio，否则为Video
        GlobalClock::instance().setMainClockType(haveAudio ? ClockType::AUDIO : ClockType::VIDEO);
        m_audioPlayer->setVolume(m_muted ? 0.0 : m_volume);

        ok &= m_videoPlayer->init(m_frmVideoBuf, m_frmSubtitleBuf);
        if (!ok) {
            close();
            return false;
        }

        m_demux->start();
        m_decodeAudio->start();
        m_decodeVideo->start();
        m_decodeSubtitl->start();
        m_audioPlayer->start();
        m_videoPlayer->start();

        if (m_roomSession) {
            m_videoPlayer->togglePaused();
            m_audioPlayer->togglePaused();
            GlobalClock::instance().togglePaused();
        }
        setOpened(true);
        if (!m_roomSession) setPaused(false);  // 单人: 自动播
        m_played = false;
        qDebug() << "打开成功";
        emit chaptersInfoUpdate();
        return true;
    } else {
        // 网络流：先快速返回，不阻塞UI，异步调用实际的打开
        QMetaObject::invokeMethod(this, [this, openPath]() {
            bool ok = true;
            // 设置低延迟模式
            m_demux->setLowLatencyMode(m_lowLatencyMode);
            m_decodeAudio->setLowLatencyMode(m_lowLatencyMode);
            m_decodeVideo->setLowLatencyMode(m_lowLatencyMode);
            m_decodeSubtitl->setLowLatencyMode(m_lowLatencyMode);
            
            qDebug() << "调用Demux::init，传递的路径:" << openPath;
            ok &= m_demux->init(openPath.toUtf8().constData(), true);
            if (!ok) {
                QString error = QString("无法解析媒体文件: %1").arg(openPath);
                qDebug() << "Demux初始化失败!";
                m_streamManager->onDemuxError("Failed to initialize demux for: " + openPath);
                close();
                return;
            }

            m_streamManager->onDemuxConnected();

            bool haveAudio = m_demux->haveStream(MediaType::Audio), haveVideo = m_demux->haveStream(MediaType::Video);
            qDebug() << "打开媒体" << openPath << "haveAudio:" << haveAudio << "haveVideo:" << haveVideo;
            if (haveAudio) {
                const int audioIdx = 0;
                m_demux->switchAudioStream(audioIdx, m_pktAudioBuf, m_frmAudioBuf);
                m_currentAudioStream = {0, audioIdx};
                ok &= m_decodeAudio->init(m_demux->getStream(MediaType::Audio), m_pktAudioBuf, m_frmAudioBuf, 1);
                ok &= m_audioPlayer->init(m_demux->getStream(MediaType::Audio)->codecpar, m_frmAudioBuf);
            }

            if (haveVideo) {
                const int videoIdx = 0;
                m_currentVideoStream = {0, videoIdx};
                m_demux->switchVideoStream(videoIdx, m_pktVideoBuf, m_frmVideoBuf);
                int cores = std::thread::hardware_concurrency();
                ok &= m_decodeVideo->init(m_demux->getStream(MediaType::Video), m_pktVideoBuf, m_frmVideoBuf, cores >= 6 ? 6 : 0);
            }

            if (haveVideo && m_demux->haveStream(MediaType::Subtitle)) {
                const int subtitleIdx = 0;
                m_currentSubtitleStream = {0, subtitleIdx};
                m_demux->switchSubtitleStream(subtitleIdx, m_pktSubtitleBuf, m_frmSubtitleBuf);
                ok &= m_decodeSubtitl->init(m_demux->getStream(MediaType::Subtitle), m_pktSubtitleBuf, m_frmSubtitleBuf, 1);
            }

            DeviceStatus::instance().setHaveAudio(haveAudio);
            DeviceStatus::instance().setHaveVideo(haveVideo);

            setDuration(m_demux->getDuration());
            setProgress(0);
            GlobalClock::instance().reset();
            GlobalClock::instance().setMainClockType(ClockType::AUDIO);
            m_audioPlayer->setVolume(m_muted ? 0.0 : m_volume);

            if (!haveAudio && !haveVideo) {
                qDebug() << "文件不包含视频和音频";
                close();
                return;
            } else if (!haveAudio && haveVideo) {
                GlobalClock::instance().setMainClockType(ClockType::VIDEO);
            }

            ok &= m_videoPlayer->init(m_frmVideoBuf, m_frmSubtitleBuf);
            if (!ok) {
                close();
                return;
            }

            m_demux->start();
            m_decodeAudio->start();
            m_decodeVideo->start();
            m_decodeSubtitl->start();
            m_audioPlayer->start();
            m_videoPlayer->start();

            if (m_roomSession) {
                m_videoPlayer->togglePaused();
                m_audioPlayer->togglePaused();
                GlobalClock::instance().togglePaused();
            }
            setOpened(true);
            if (!m_roomSession) setPaused(false);
            m_played = false;
            qDebug() << "打开成功";
            emit chaptersInfoUpdate();
        }, Qt::QueuedConnection);
        return true;
    }
}

bool MediaController::close() {
    if (!m_opened) {
        return true;
    }

    // Sync: notify room session if leaving
    if (m_roomSession && m_syncManager && m_syncManager->isSyncing()) {
        m_syncManager->setActive(false);
    }

    // 关闭流管理器
    m_streamManager->close();

    bool ok = true;
    ok &= m_demux->uninit();
    ok &= m_decodeAudio->uninit();
    ok &= m_decodeVideo->uninit();
    ok &= m_decodeSubtitl->uninit();
    ok &= m_audioPlayer->uninit();
    ok &= m_videoPlayer->uninit();
    clearPktQ(m_pktAudioBuf);
    clearPktQ(m_pktVideoBuf);
    clearPktQ(m_pktSubtitleBuf);
    clearFrmQ(m_frmAudioBuf);
    clearFrmQ(m_frmVideoBuf);
    clearFrmQ(m_frmSubtitleBuf);
    DeviceStatus::instance().setHaveAudio(false);
    DeviceStatus::instance().setHaveVideo(false);
    GlobalClock::instance().reset();
    setOpened(false);
    m_currentVideoStream = {-1, -1};
    m_currentAudioStream = {-1, -1};
    m_currentSubtitleStream = {-1, -1};
    setPaused(true);
    setDuration(0);
    setProgress(0);
    PlaybackStats::instance().reset();
    qDebug() << "关闭";
    emit chaptersInfoUpdate();
    return ok;
}

void MediaController::togglePaused() {
    if (!m_opened) {
        return;
    }
    m_videoPlayer->togglePaused();
    m_audioPlayer->togglePaused();
    GlobalClock::instance().togglePaused();
    setPaused(!m_paused);

    // Sync hook
    if (m_syncManager && m_syncManager->isSyncing()) {
        double pos = getCurrentTime();
        if (m_paused) {
            m_syncManager->onLocalPause(pos);
        } else {
            m_syncManager->onLocalPlay(pos);
        }
        if (m_roomSession) {
            // 事件已由 Server 统一记录
        }
    }
}

void MediaController::toggleMuted() {
    if (m_muted) {
        m_audioPlayer->setVolume(m_volume);
    } else {
        m_audioPlayer->setVolume(0.0);
    }
    setMuted(!m_muted);
}

bool MediaController::muted() const {
    return m_muted;
}

void MediaController::setMuted(bool newMuted) {
    if (m_muted == newMuted)
        return;
    m_muted = newMuted;
    emit mutedChanged(newMuted);
}

double MediaController::volume() const {
    return m_volume;
}

void MediaController::setVolume(double newVolume) {
    if (qFuzzyCompare(m_volume, newVolume))
        return;
    m_audioPlayer->setVolume(newVolume);
    m_volume = newVolume;
    setMuted(false);
    emit volumeChanged(newVolume);
}

void MediaController::addVolum() {
    setVolume(std::min(1.0, m_volume + 0.04));
}

void MediaController::subVolum() {
    setVolume(std::max(0.0, m_volume - 0.04));
}

void MediaController::seekInternal(double ts, double rel) {
    if (!m_opened) return;
    double safeTs = ts;
    if (rel == 0.0) {
        safeTs = std::max(0.0, std::min(safeTs, static_cast<double>(m_duration)));
    }
    m_demux->seekBySec(safeTs, rel);
}

void MediaController::seekBySec(double ts, double rel) {
    seekInternal(ts, rel);
    // 只对用户主动的绝对 seek 广播
    if (rel == 0.0 && m_syncManager && m_syncManager->isSyncing()) {
        double safeTs = std::max(0.0, std::min(ts, static_cast<double>(m_duration)));
        m_syncManager->onLocalSeek(safeTs);
    }
}

void MediaController::executeRemoteSeek(double pos) {
    // 纯执行远端 seek，不触发同步广播
    seekInternal(pos, 0.0);
}

void MediaController::fastForward() {
    seekBySec(std::min((double)m_duration, getCurrentTime() + 5.0), 5.0);
}

void MediaController::fastRewind() {
    seekBySec(std::max(0.0, getCurrentTime() - 5.0), -5.0);
}

void MediaController::nextFrame() {
    if (!m_opened || !m_videoPlayer) {
        return;
    }
    // 确保是暂停状态
    if (!m_paused) {
        togglePaused();
    }
    m_videoPlayer->nextFrame();
}

void MediaController::prevFrame() {
    if (!m_opened || !m_videoPlayer) {
        return;
    }
    // 确保是暂停状态
    if (!m_paused) {
        togglePaused();
    }
    m_videoPlayer->prevFrame();
}

void MediaController::togglePictureInPicture() {
    if (!m_pipWindow) {
        m_pipWindow = new PiPWindow();
        m_pipWindow->setAttribute(Qt::WA_DeleteOnClose);
    }

    if (m_pipWindow->isVisible()) {
        m_pipWindow->hide();
    } else {
        m_pipWindow->show();
        // 如果已经有视频数据，立即传递给画中画
        if (m_lastVidBuf && m_lastSubBuf) {
            m_pipWindow->setVideoData(m_lastVidBuf, m_lastSubBuf);
        }
    }
}

bool MediaController::switchSubtitleStream(int streamIdx) {
    if (m_currentVideoStream.first < 0 || m_currentVideoStream.second < 0)
        return false;

    if (m_currentSubtitleStream == std::pair{0, streamIdx}) {
        return true;
    }

    if (m_currentSubtitleStream.first != -1)
        m_demux->closeStream(MediaType::Subtitle);
    m_decodeSubtitl->uninit();
    m_videoPlayer->clearSubtitle();
    clearPktQ(m_pktSubtitleBuf);
    m_pktSubtitleBuf->addSerial();
    m_frmSubtitleBuf->addSerial();

    m_demux->switchSubtitleStream(streamIdx, m_pktSubtitleBuf, m_frmSubtitleBuf);
    m_decodeSubtitl->init(m_demux->getStream(MediaType::Subtitle), m_pktSubtitleBuf, m_frmSubtitleBuf, 1);
    m_decodeSubtitl->start();

    m_currentSubtitleStream = {0, streamIdx};
    return true;
}

int MediaController::progress() const {
    return m_progress;
}

void MediaController::setProgress(int newProgress) {
    if (m_progress == newProgress)
        return;
    m_progress = newProgress;
    emit progressChanged(newProgress);
}

bool MediaController::loopOnEnd() const { return m_loopOnEnd; }

void MediaController::setLoopOnEnd(bool newLoopOnEnd) { m_loopOnEnd = newLoopOnEnd; }

bool MediaController::opened() const { return m_opened; }

void MediaController::setOpened(bool newOpened) {
    if (m_opened == newOpened)
        return;
    m_opened = newOpened;
    emit openedChanged(newOpened);
}

void MediaController::checkPlayerFinished() {
    if (!m_opened || m_played || !m_demux->isEOF())
        return;

    bool haveAudio = m_currentAudioStream.second != -1;
    bool audioQueueIsEmpty = m_pktAudioBuf->size() == 0 && m_frmAudioBuf->size() == 0;
    bool videoQueueIsEmpty = m_pktVideoBuf->size() == 0 && m_frmVideoBuf->size() == 0;
    bool playerFinished = false;

    if (haveAudio) {
        if (audioQueueIsEmpty) {
            playerFinished = true;
        }
    } else if (videoQueueIsEmpty) {
        playerFinished = true;
    }

    if (playerFinished) {
        if (m_loopOnEnd) {
            seekBySec(0.0, 0.0);
        } else {
            if (!m_paused)
                togglePaused();
            m_played = true;
            emit played();
        }
    }
}

int MediaController::getCurrentTime() const {
    double ptsSecond = GlobalClock::instance().getMainPts();
    int res = std::isnan(ptsSecond) ? 0 : static_cast<int>(ptsSecond);
    // 确保时间在0到duration范围内
    return std::max(0, std::min(res, m_duration));
}

QVariantList MediaController::getChaptersInfo() const {
    QVariantList list;
    if (!m_demux)
        return list;

    const std::vector<ChapterInfo> &chapters = m_demux->chaptersInfo();
    for (auto &v : chapters) {
        int total = static_cast<int>(v.pts * 1000.0 + 0.5);
        int ms = total % 1000;
        total /= 1000;
        int sec = total % 60;
        total /= 60;
        int min = total % 60;
        int hour = total / 60;

        QString timeStr = QString("[%1:%2:%3:%4]")
                              .arg(hour, 2, 10, QLatin1Char('0'))
                              .arg(min, 2, 10, QLatin1Char('0'))
                              .arg(sec, 2, 10, QLatin1Char('0'))
                              .arg(ms, 3, 10, QLatin1Char('0'));
        QString title = QString::fromStdString(v.title);
        QVariantMap item;
        item["pts"] = QVariant(v.pts);
        item["text"] = timeStr + " " + title;
        list.push_back(item);
    }
    return list;
}

int MediaController::duration() const {
    return m_duration;
}

void MediaController::setDuration(int newDuration) {
    if (m_duration == newDuration)
        return;
    m_duration = newDuration;
    emit durationChanged(newDuration);
}

bool MediaController::paused() const {
    return m_paused;
}

void MediaController::setPaused(bool newPaused) {
    if (m_paused == newPaused)
        return;
    m_paused = newPaused;
    emit pausedChanged(newPaused);
}

void MediaController::setLowLatencyMode(bool enable) {
    if (m_lowLatencyMode == enable)
        return;
    m_lowLatencyMode = enable;
    if (m_demux) {
        m_demux->setLowLatencyMode(enable);
    }
    if (m_decodeAudio) {
        m_decodeAudio->setLowLatencyMode(enable);
    }
    if (m_decodeVideo) {
        m_decodeVideo->setLowLatencyMode(enable);
    }
    if (m_decodeSubtitl) {
        m_decodeSubtitl->setLowLatencyMode(enable);
    }
    emit lowLatencyModeChanged(enable);
}

void MediaController::setLowLatencyOptions(bool noBuffer, bool lowDelayFlag, bool discardCorrupt, bool noParse, bool shortProbe) {
    if (m_demux) {
        m_demux->setNoBuffer(noBuffer);
        m_demux->setLowDelayFlag(lowDelayFlag);
        m_demux->setDiscardCorrupt(discardCorrupt);
        m_demux->setNoParse(noParse);
        m_demux->setShortProbe(shortProbe);
    }
}