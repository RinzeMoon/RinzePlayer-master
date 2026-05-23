// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONTROLBAR_H
#define CONTROLBAR_H

#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QFrame>
#include <QFocusEvent>
#include "ui/urlplaybackconfigdialog.h"

// 自定义侧边栏类
class SidebarWidget : public QWidget {
    Q_OBJECT
public:
    explicit SidebarWidget(QWidget *parent = nullptr) : QWidget(parent) {}

signals:
    void closed();

protected:
    void focusOutEvent(QFocusEvent *) override {
        emit closed();
        hide();
    }
};

class ControlBar : public QWidget {
    Q_OBJECT
public:
    explicit ControlBar(QWidget *parent = nullptr);
    ~ControlBar();

signals:
    void openFile();
    void openUrlWithConfig(const QUrl &url, bool lowLatency, bool noBuffer, bool lowDelayFlag, bool discardCorrupt, bool noParse, bool shortProbe);
    void togglePause();
    void seekTo(int seconds);
    void volumeChanged(int volume);
    void fastForward();
    void fastRewind();
    void rotateLeft();   // 旋转90度向左
    void rotateRight(); // 旋转90度向右
    void horizontalMirror(); // 水平镜像
    void verticalMirror(); // 垂直镜像
    void resetTransform(); // 重置所有变换
    void brightnessChanged(float val);
    void contrastChanged(float val);
    void saturationChanged(float val);
    void resetFilters();
    void toggleFullscreen(); // 切换全屏
    void showFPSChanged(bool show); // 切换帧率显示
    void colorSpaceChanged(int colorSpace); // 切换色彩空间 (0 = BT.601, 1 = BT.709, 2 = BT.2020)
    void lowLatencyModeChanged(bool enabled); // 切换低延迟模式
    void chatToggle(); // 聊天窗口切换

public slots:
    void setDuration(int seconds);
    void setCurrentTime(int seconds);
    void setPaused(bool paused);
    void setVolume(int volume);
    void setRoomMode(bool inRoom);
    void updateVideoInfo(const QString& pixelFormat, const QString& colorRange, const QString& colorSpace);

private slots:
    void onProgressSliderPressed();
    void onProgressSliderReleased();
    void onProgressSliderMoved(int value);
    void onVolumeSliderChanged(int value);
    void onOpenUrlClicked(); // 新增：点击打开URL
    void onUrlReturnPressed(); // 新增：回车播放URL
    void onToggleSidebar(); // 新增：切换侧边栏显示/隐藏

private:
    void setupUi();
    void setupSidebar(); // 新增：初始化侧边栏
    QString formatTime(int seconds);

    QPushButton *m_openBtn;
    QLineEdit *m_urlEdit; // 新增：URL输入框
    QPushButton *m_openUrlBtn; // 新增：打开URL按钮
    QPushButton *m_chatBtn;    // 聊天按钮
    QPushButton *m_playPauseBtn;
    QPushButton *m_ffwdBtn;
    QPushButton *m_rewindBtn;
    QSlider *m_progressSlider;
    QLabel *m_timeLabel;
    QLabel *m_durationLabel;
    QSlider *m_volumeSlider;
    QLabel *m_volumeLabel;
    QPushButton *m_rotateLeftBtn;
    QPushButton *m_rotateRightBtn;
    QPushButton *m_hMirrorBtn;
    QPushButton *m_vMirrorBtn;
    QPushButton *m_resetBtn;
    QPushButton* m_sidebarToggleBtn; // 新增：侧边栏切换按钮
    SidebarWidget* m_sidebar; // 新增：侧边栏窗口
    QLabel* m_pixelFormatLabel; // 像素格式显示
    QLabel* m_colorRangeLabel; // 色彩范围显示
    QLabel* m_colorSpaceLabel; // 色彩空间显示

    bool m_isDraggingSlider;
    bool m_sidebarVisible; // 新增：侧边栏是否可见
    bool m_inRoom = false;
    int m_duration;
    int m_volume;
};

#endif // CONTROLBAR_H
