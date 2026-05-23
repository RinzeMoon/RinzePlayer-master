// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HOVERCONTROLBAR_H
#define HOVERCONTROLBAR_H

#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

class HoverControlBar : public QWidget {
    Q_OBJECT
public:
    explicit HoverControlBar(QWidget *parent = nullptr);
    ~HoverControlBar();
    
    bool isControlsVisible() const { return m_controlsVisible; }

signals:
    void togglePause();
    void seekTo(int seconds);
    void volumeChanged(int volume);
    void fastForward();
    void fastRewind();
    void toggleFullscreen();

public slots:
    void setDuration(int seconds);
    void setCurrentTime(int seconds);
    void setPaused(bool paused);
    void setVolume(int volume);
    void showControls();
    void hideControls();
    void autoHide();
    void updatePosition();

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onProgressSliderPressed();
    void onProgressSliderReleased();
    void onProgressSliderMoved(int value);
    void onVolumeSliderChanged(int value);

private:
    void setupUi();
    void applyStyles();
    QString formatTime(int seconds);

    QTimer* m_autoHideTimer;
    
    // 动画相关
    QPropertyAnimation* m_showAnimation;
    QPropertyAnimation* m_hideAnimation;
    
    // UI 组件
    QWidget* m_container;
    QPushButton* m_playPauseBtn;
    QPushButton* m_rewindBtn;
    QPushButton* m_ffwdBtn;
    QPushButton* m_fullscreenBtn;
    QSlider* m_progressSlider;
    QLabel* m_timeLabel;
    QLabel* m_durationLabel;
    QSlider* m_volumeSlider;
    QLabel* m_volumeIconLabel;

    bool m_isDraggingSlider;
    bool m_controlsVisible;
    int m_duration;
    int m_volume;
};

#endif // HOVERCONTROLBAR_H
