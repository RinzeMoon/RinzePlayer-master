//
// Created by lsy on 2025/9/22.
//

#ifndef RINZEPLAYER_BASECONTROLBAR_H
#define RINZEPLAYER_BASECONTROLBAR_H
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>

#include "../../Global/Global.h"
#include "../Util.h"

using namespace RinGlobal;
using namespace RinColor;
using Util::ScrollingLabel;

class BaseControlBar : public QWidget
{
    Q_OBJECT
public:
    explicit BaseControlBar(QWidget *parent = nullptr);
    virtual ~BaseControlBar() = default;

    // 纯虚函数（子类实现）
    virtual void setProgress(qint64 current, qint64 total) = 0;
    virtual void setVolume(int volume) = 0;
    virtual void setPlayState(PlayState state) = 0;

    // 获取当前播放状态
    PlayState currentState() const;

    // 设置控件启用状态
    virtual void setControlsEnabled(bool enabled);

    // 子类需要的接口方法
    QPushButton* getVolumeButton() const;
    QPushButton* getPlayPauseButton() const;
    QSlider* getProgressSlider() const;
    QLabel* getCurrentTimeLabel() const;
    QLabel* getTotalTimeLabel() const;
    QWidget* getBaseControlArea() const;
    QPushButton* getPrevButton() const
    {
        return m_btnPrev;
    }
    QPushButton* getNextButton() const
    {
        return m_btnNext;
    }
    void setBaseProgress(qint64 currentMs, qint64 totalMs);

    QWidget* getTopLeftArea() const;    // 新增：左区域访问接口
    QWidget* getTopRightArea() const;   // 新增：右区域访问接口

protected:
    // 保护成员变量
    QWidget* m_topLeftArea;  // 上层左区域（供子类扩展）
    QWidget* m_topRightArea; // 上层右区域（供子类扩展）
    //QWidget *m_volumePopup;  // 音量弹窗
    //QSlider *m_popupVolumeSlider; // 音量滑块
    //bool m_isVolumePopupVisible;  // 音量弹窗可见状态
    QHBoxLayout *m_mainLayout;    // 主布局
    QPushButton *m_btnPrev;       // 上一曲按钮
    QPushButton *m_btnPlayPause;  // 播放/暂停按钮
    QPushButton *m_btnNext;       // 下一曲按钮
    QSlider *m_progressSlider;    // 进度条
    QLabel *m_timeLabel;          // 时间显示标签
    QPushButton *m_btnMute;       // 静音/音量按钮
    int m_currentVolume;          // 当前音量
    bool m_isMuted;               // 是否静音
    PlayState m_currentState;     // 播放状态

    // 时间格式化（毫秒转mm:ss）
    QString formatTime(qint64 milliseconds);

signals:
    void playClicked();
    void pauseClicked();
    void stopClicked();
    void prevClicked();
    void nextClicked();
    void progressChanged(qint64 position);
    void volumeChanged(int volume);
    void muteClicked(bool muted);

public slots:
    virtual void onPlayStateChanged(PlayState state);
    virtual void onVolumeChanged(int volume);
    virtual void onMutedChanged(bool muted);

private slots:
    virtual void onPlayPauseClicked();
    virtual void onMuteBtnClicked();
    virtual void onVolumeButtonClicked() = 0;

protected:
    virtual void initUI();
    //virtual void initVolumePopup();
    virtual void connectSignals();
    virtual void updatePlayPauseIcon();
    virtual void updateVolumeIcon();
};


#endif //RINZEPLAYER_BASECONTROLBAR_H