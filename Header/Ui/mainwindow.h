#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <QToolBar>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QSlider>
#include <QStackedWidget>

#include "../Util.h"
#include "../Header/Ui/AudioPlayListWidget.h"
#include "HomeWidget.h"
#include "../Header/Ui/MusicSheetUI.h"
#include "../Global/Global.h"
#include "MusicPlayerWidget.h"
#include "../Header/Manager/MusicSheetManager.h"


using namespace RinColor;
using Util::CollapsibleWidget;
using AudioUtil::MusicSheet;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    void initializeSheetUIs();
    void createSheetUiInstance(const QString& sheetId, const MusicSheet& sheet);
    void updateAllSheetPlayingStates(const QString& playingSheetId, const QString& playingSongId);

    void PushSongIntoPlayList(const QString& songId, const QString& sheetId);

    ~MainWindow();
public slots:
    void onControlBarClicked();
    void onItemStateChanged(const QString& sheetId ,const QString& LabelName);
private:
    // 部件创建函数
    void createTopBar();
    void createContentArea();
    void createControlBar();
    void initMusicPlayerWidget();
    void createConnections();

    // 事件重写
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    // 主布局
    QVBoxLayout *m_mainLayout;

    // 顶部栏部件
    QWidget *m_topBar;
    QWidget *m_logoArea;
    QLineEdit *m_searchEdit;
    QPushButton *m_btnSettings;
    QPushButton *m_btnMinimize;
    QPushButton *m_btnMaximize;
    QPushButton *m_btnClose;

    // 内容区部件
    QWidget *m_contentArea;
    QWidget *m_navArea;       // 左侧导航
    QWidget *m_mainContent;   // 右侧主内容
    MusicPlayerWidget * m_musicPlayerWidget;

    // 控制栏
    QWidget *m_controlBar;
    // HomePage
    HomePage * m_homePage;
    QStackedWidget *m_rightStack;   // 右侧页面切换容器（新变量）

private:
    bool m_isMoving = false;
    QPoint m_dragStartPosition;
    QPoint m_windowStartPosition;

    // 添加边界检查标志
    bool m_enableBoundaryCheck = true;
    QPropertyAnimation *m_playerAnim;  // 动画对象
    bool m_isPlayerShown;  // 标记PlayerWidget是否显示

private:
    // 播放列表弹窗成员变量（关键：用成员变量存储，避免被销毁）
    AudioPlaylistPopup* m_playlistDlg = nullptr;
    // 存储弹窗上次关闭时的位置（用于记忆位置）
    QPoint m_playlistLastPos;
private:
    QMap<QString, MusicSheetUI*> m_sheetUiMap;    // 歌单ID -> UI实例
    MusicSheetUI* m_currentSheetUi = nullptr;      // 当前显示的歌单UI
    QString m_currentSheetId;                      // 当前显示的歌单ID

    CollapsibleWidget* playlistWidget;
    CollapsibleWidget* videoListWidget;
private:
    PlaylistManager* m_playlistManager;
    PlayQueueController* m_playQueueController;
    MusicSheetManager* m_musicSheetManager;

    QString m_currentPlayingSongId;
    QString m_currentPlayingSheetId;

private:
    AudioPlayer *player;
private slots:
    void onCreateSheetRequested(); // 处理“创建歌单”请求的槽函数

    void onShowSheetUi(const QString & id); // 处理展示歌单ui的请求

    void onPlaySong(const QString& songId, const QString& sheetId);

    void onrequestAddSongToOtherSheet(const QString& targetSheetId, const SongMeta& song);

    void onSheetDataChanged(const QString& sheetId);

    void onSheetSaved(const QString& sheetId);

    void onRemoveSongFromSheetRequested(const QString& sheetId, const QString& songId);
private:
    void createFavoriteSheet();
};

#endif // MAINWINDOW_H
