// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MEDIACONTROLLER_H
#define MEDIACONTROLLER_H

class VideoWidget; // 前向声明
class PiPWindow; // 前向声明

#include "decode/decodeaudio.h"
#include "decode/decodesubtitle.h"
#include "decode/decodevideo.h"
#include "demux/demux.h"
#include "renderer/audioplayer.h"
#include "renderer/videoplayer.h"
#include "types/ptrs.h"
#include "streaming/stream_manager.h"
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariant>

class SyncManager;
class RoomSession;

class MediaController : public QObject {
    Q_OBJECT
public:
    explicit MediaController(QObject *parent = nullptr);
    ~MediaController();

    void setSyncManager(SyncManager *sm);
    void setRoomSession(RoomSession *rs);
    SyncManager* syncManager() const { return m_syncManager; }
    RoomSession* roomSession() const { return m_roomSession; }
    bool isInRoom() const { return m_roomSession != nullptr && m_syncManager != nullptr; }

    // 获取码率（kbps）
    double getBitrate() const;
    // 获取缓冲区队列信息
    int getVideoPacketQueueSize() const { return m_pktVideoBuf ? m_pktVideoBuf->size() : 0; }
    int getVideoFrameQueueSize() const { return m_frmVideoBuf ? m_frmVideoBuf->size() : 0; }
    int getAudioPacketQueueSize() const { return m_pktAudioBuf ? m_pktAudioBuf->size() : 0; }
    int getAudioFrameQueueSize() const { return m_frmAudioBuf ? m_frmAudioBuf->size() : 0; }
    // 获取音视频同步差（秒）
    double getSyncDifference() const;

    bool paused() const;
    void setPaused(bool newPaused);
    void togglePause(); // 新增：切换暂停

    double volume() const;

    bool muted() const;
    void setMuted(bool newMuted);

    int duration() const;
    void setDuration(int newDuration);

    bool opened() const;
    void setOpened(bool newOpened);

    int getCurrentTime() const;
    QVariantList getChaptersInfo() const;

    bool loopOnEnd() const;
    void setLoopOnEnd(bool newLoopOnEnd);
    // 判断当前是否是本地文件
    bool isLocalFile() const;

    int progress() const;
    void setProgress(int newProgress);
    
    AudioPlayer* audioPlayer() const { return m_audioPlayer; }
    QUrl currentUrl() const { return m_URL; }

public slots:
    void setRenderCallback(std::function<void(VideoDoubleBuf*, SubtitleDoubleBuf*)> callback);
    void setVideoWidget(VideoWidget* widget); // 设置视频窗口，用于画中画
    void onRenderDataReady(VideoDoubleBuf* vid, SubtitleDoubleBuf* sub);

    bool open(const QUrl &URL);
    bool close();

    // 低延迟模式
    bool lowLatencyMode() const { return m_lowLatencyMode; }
    void setLowLatencyMode(bool enable);
    void setLowLatencyOptions(bool noBuffer, bool lowDelayFlag, bool discardCorrupt, bool noParse, bool shortProbe);

    void togglePaused();
    void toggleMuted();
    void setVolume(double newVolume);
    void addVolum();
    void subVolum();

    void seekBySec(double ts, double rel);
    void executeRemoteSeek(double pos);  // 远端 seek，不触发同步广播
    void fastForward();
void fastRewind();
void nextFrame();  // 下一帧
void prevFrame();  // 上一帧
void togglePictureInPicture();  // 画中画

    bool switchSubtitleStream(int streamIdx);

signals:
    void pausedChanged(bool paused);
    void volumeChanged(double volume);
    void mutedChanged(bool muted);
    void chaptersInfoUpdate();

    void durationChanged(int duration);
    void seeked();

    void openedChanged(bool opened);
    void played();
    void progressChanged(int progress);
    void openFailed(const QString& error); // 打开文件失败信号
    void lowLatencyModeChanged(bool enabled); // 低延迟模式改变信号

    // 流媒体状态信号
    void streamStateChanged(StreamState oldState, StreamState newState);
    void bufferProgress(double percent, double bufferMB);
    void streamError(const QString& error);
    void reconnecting(int attempt, int maxAttempts);
    // 视频信息信号
    void videoInfoChanged(const QString& pixelFormat, const QString& colorRange, const QString& colorSpace);

private:
    QUrl m_URL{};
    SyncManager* m_syncManager = nullptr;
    RoomSession* m_roomSession = nullptr;
    StreamManager* m_streamManager = nullptr;
    VideoWidget* m_videoWidget = nullptr; // 视频窗口
    PiPWindow* m_pipWindow = nullptr; // 画中画窗口
    VideoDoubleBuf* m_lastVidBuf = nullptr;
    SubtitleDoubleBuf* m_lastSubBuf = nullptr;

    sharedPktQueue m_pktAudioBuf = std::make_shared<AVPktQueue>(2);
    sharedFrmQueue m_frmAudioBuf = std::make_shared<SPSCQueue<AVFrmItem>>(50);
    sharedPktQueue m_pktVideoBuf = std::make_shared<AVPktQueue>(10);
    sharedFrmQueue m_frmVideoBuf = std::make_shared<SPSCQueue<AVFrmItem>>(3);
    sharedPktQueue m_pktSubtitleBuf = std::make_shared<AVPktQueue>(2);
    sharedFrmQueue m_frmSubtitleBuf = std::make_shared<SPSCQueue<AVFrmItem>>(16);

    Demux *m_demux = nullptr;
    std::pair<int, int> m_currentVideoStream{-1, -1};
    std::pair<int, int> m_currentAudioStream{-1, -1};
    std::pair<int, int> m_currentSubtitleStream{-1, -1};

    DecodeAudio *m_decodeAudio = nullptr;
    DecodeVideo *m_decodeVideo = nullptr;
    DecodeSubtitle *m_decodeSubtitl = nullptr;

    AudioPlayer *m_audioPlayer = nullptr;
    VideoPlayer *m_videoPlayer = nullptr;

    QTimer m_checkPlayerFinishedTimer;
    QTimer m_updatePktAndFrmQueueSizeTimer;
    QTimer m_progressFallbackTimer;

    bool m_opened = false;
    bool m_paused = true;
    bool m_muted = false;
    double m_volume = 1.0;
    int m_duration = 0;
    int m_progress = 0;
    bool m_loopOnEnd = true;
    bool m_played = false;
    bool m_lowLatencyMode = false; // 低延迟模式
    std::function<void(VideoDoubleBuf*, SubtitleDoubleBuf*)> m_renderCallback;

private:
    void checkPlayerFinished();
    void seekInternal(double ts, double rel);  // 纯 seek 逻辑，不广播

    // 异步打开的内部方法
    void openAsyncInternal(QString openPath, QUrl URL, bool isLocalFile);
};


#endif // MEDIACONTROLLER_H
