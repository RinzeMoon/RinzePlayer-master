#ifndef VIDEOPLAYERMW_H
#define VIDEOPLAYERMW_H

#include <QMainWindow>
#include <QSplitter>
#include "VideoPart/BasePlayerFrame.h"
#include "Controller/StandardController.h"
#include "Ui/VideoPlayListWidget.h"
#include "VideoPart/ResManager/ResMgr.h"

class VideoPlayerMW : public QMainWindow
{
    Q_OBJECT

public:
    VideoPlayerMW(QWidget *parent = nullptr);
    ~VideoPlayerMW();

    signals:
    void PlayRequestToFrame(const QString &source, VideoPlayerMode mode);

protected:
    // 鼠标事件支持无边框窗口拖动
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    // 新增：鼠标移出窗口事件，修复拖动状态卡死
    void leaveEvent(QEvent *event) override;

    // 窗口事件
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUi();
    void createMenu();
    void createConnections();

    void updateWindowState();

private slots:
    void onPlaylistItemSelected(const PlaylistItem &item);
    void onPlayRequested(const QString &source, VideoPlayerMode mode);
    void onFullscreenToggled();
    void onMaximizeButtonClicked();


private:
    QSplitter *m_splitter;
    BasePlayerFrame *m_playerFrame = nullptr;
    VideoPlaylistWidget *m_playlistWidget;
    StandardController *m_controller;
    ResMgr *m_resMgr;

    // 窗口状态
    bool m_isMaximized = false;
    bool m_isFullscreen = false;
    bool m_isMoving = false;

    // 拖动相关
    QPoint m_dragStartPosition;
    QPoint m_windowStartPosition;
    bool m_enableBoundaryCheck = true;

    // UI 组件
    QWidget *titleBar = nullptr;
    QToolButton *minimizeBtn = nullptr;
    QToolButton *maximizeBtn = nullptr;
    QToolButton *closeBtn = nullptr;

    // 窗口几何记录
    QRect m_normalGeometry;

    // WSL2 特殊处理
    bool m_isWSL2 = false;
};

#endif // VIDEOPLAYERMW_H