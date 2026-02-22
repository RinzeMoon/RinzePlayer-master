#ifndef STANDARDCONTROLLER_H
#define STANDARDCONTROLLER_H

#include "VideoPart/BasePlayerFrame.h"
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QToolButton>

class StandardController : public BaseController
{
    Q_OBJECT

public:
    StandardController(QWidget *parent = nullptr);
    void updateState(bool playing, qint64 position, qint64 duration) override;

    void setMode(VideoPlayerMode mode) override;

private slots:
    void onPlayPauseClicked();
    void onStopClicked();
    void onVolumeChanged(int value);
    void onSpeedChanged(int index);
    void onProgressSliderReleased();
    void onProgressSliderPressed();

private:
    void setupUi();
    QString formatTime(qint64 ms) const;
    void updateProgressBar();

private:
    // 控制元素 - 全部显式初始化nullptr，杜绝野指针
    QPushButton *m_playPauseBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_fullscreenBtn = nullptr;
    QSlider *m_progressSlider = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_durationLabel = nullptr;
    QComboBox *m_speedCombo = nullptr;
    QToolButton *m_volumeBtn = nullptr;
    QToolButton *m_playlistBtn = nullptr;
    QToolButton *m_settingsBtn = nullptr;

    // 状态
    bool m_isPlaying = false;
    qint64 m_duration = 0;
    qint64 m_currentPosition = 0;
    VideoPlayerMode m_currentMode = VideoPlayerMode::LOCAL_FILE;
    bool m_isSliderPressed = false;

    void initLocalHttpUi();  // 初始化 本地/HTTP 播放UI
    void initRtmpUi();       // 初始化 RTMP直播UI(纯观众视角)

private:
    // ✅ 删除无用的开播按钮，只保留直播状态标签 + 双容器，完美匹配需求
    QLabel *m_liveStatusLabel = nullptr;       // 直播状态提示
    QWidget *m_localHttpContainer = nullptr;   // 本地文件+HTTP点播 共用UI容器
    QWidget *m_rtmpContainer = nullptr;        // RTMP直播 专属UI容器

    // ✅ 新增：直播模式的所有控件【成员变量】，生命周期和类一致，无悬空指针
    QPushButton *m_rtmpPlayBtn = nullptr;
    QComboBox *m_rtmpSpeedCombo = nullptr;
    QToolButton *m_rtmpVolumeBtn = nullptr;
    QSlider *m_rtmpVolumeSlider = nullptr;
    QToolButton *m_rtmpSettingsBtn = nullptr;
    QPushButton *m_rtmpFullscreenBtn = nullptr;

signals:
    // 原有信号 全部保留
    void playClicked();
    void pauseClicked();
    void stopClicked();
    void volumeChanged(int value);
    void speedChanged(float speed);
    void seekRequested(qint64 pos);
    // ✅ 直播只保留播放暂停信号，删除开播相关信号，匹配观众视角
    void fullscreenToggled();
};

#endif // STANDARDCONTROLLER_H