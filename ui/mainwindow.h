// SPDX-FileCopyrightText: 2025-2026 Xuefei Ai
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QSizePolicy>
#include "core/mediacontroller.h"
#include "ui/videowidget.h"
#include "ui/controlbar.h"
#include "streaming/streaming_types.h"
#include <QJsonObject>
#include "streaming/chatroom_client.h"
#include "streaming/status_client.h"
#include "streaming/room_session.h"
#include "core/sync_manager.h"
#include "visualizer/audiovisualwidget.h"
#include "ui/chatwindow.h"
#include "ui/danmakuoverlay.h"
#include "ui/readyoverlay.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

public slots:
    void openFileDialog();
    void onOpenFile(const QString& filePath);
    void updateDuration(int duration);
    void updateProgress(int progress);
    void updatePaused(bool paused);
    void toggleFullscreen();
    void updateStats();
    void onOpenFailed(const QString& error); // 打开失败槽
    void minimizeWindow();
    void maximizeRestoreWindow();
    void closeWindow();

    // 双人观影
    void onCreateRoom();
    void onJoinRoom();
    void onLeaveRoom();
    void onRoomClosed(const QString &reason);
    void onRoomExportFinished(const QString &filePath);

    // 流媒体状态槽
    void onStreamStateChanged(StreamState oldState, StreamState newState);
    void onBufferProgress(double percent, double bufferMB);
    void onStreamError(const QString& error);

    void toggleVisualizer();
    void nextVisualizerMode();

private:
    void setupUi();
    void setupTitleBar();
    void setupMenuBar();

    QString streamStateToString(StreamState state);
    QString formatTimeForDisplay(int seconds);

    MediaController *m_controller;
    VideoWidget *m_videoWidget;
    ControlBar *m_controlBar;

    // 自定义标题栏
    QWidget* m_titleBar;
    QLabel* m_titleLabel;
    QPushButton* m_minimizeBtn;
    QPushButton* m_maximizeBtn;
    QPushButton* m_closeBtn;
    
    // 菜单栏
    QMenuBar* m_menuBar;

    // 窗口拖动
    QPoint m_dragPosition;
    bool m_isDragging;

    QLabel* m_statusLabel;
    QLabel* m_bufferLabel;
    QLabel* m_statsLabel;

    QTimer* m_statsTimer;

    int m_playbackStartSeconds = 0;
    bool m_isFullscreen = false;
    bool m_visualizerConnected = false;
    
    // 可视化
    AudioVisualWidget *m_visualWidget;
    
    // 效果器面板
    class EffectsPanel *m_effectsPanel;
    
    // 运动向量动作
    QAction* m_motionVectorsAction = nullptr;
    QAction* m_motionHeatmapAction = nullptr;
    QAction* m_motionTrailAction = nullptr;
    QAction* m_motionHistoryAction = nullptr;
#ifdef OPENCV_AVAILABLE
    QAction* m_motionDetectionAction = nullptr;
#endif

    // 报告生成相关
    QAction* m_generateReportAction = nullptr;
    QAction* m_createRoomAction = nullptr;
    QAction* m_joinRoomAction = nullptr;
    QAction* m_leaveRoomAction = nullptr;

    // 双人观影
    ChatRoomClient *m_chatRoomClient = nullptr;
    StatusClient *m_statusClient = nullptr;
    RoomSession *m_roomSession = nullptr;
    SyncManager *m_syncManager = nullptr;
    ChatWindow *m_chatWindow = nullptr;
    DanmakuOverlay *m_danmakuOverlay = nullptr;
    ReadyOverlay *m_readyOverlay = nullptr;
    QWidget *m_roomInfoBar = nullptr;
    QLabel *m_roomCodeLabel = nullptr;
    QLabel *m_connStatusLabel = nullptr;
    QPushButton *m_copyRoomBtn = nullptr;
    QTimer *m_countdownTimer = nullptr;
    int m_countdownRemaining = 0;
    bool m_inRoom = false;
    bool m_iAmReady = false;
    QString m_currentRoomUrl;

    void openForRoom(const QUrl &url);
    void updateRoomInfoBar();
    void updateConnectionIndicator(int level, int pingMs);
    void onReconnectTick(int remaining);
    void copyRoomCode();
};

#endif // MAINWINDOW_H
