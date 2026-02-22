#ifndef VIDEOPLAYLISTWIDGET_H
#define VIDEOPLAYLISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QButtonGroup>
#include <QMenu>
#include <QAction>
#include <QShortcut>
#include "../Global/Global.h"
#include "../Header/VideoPart/AvSourceParse/AVSourceResolver.h"
#include "VideoPart/AVDataFetch/AVDataFetcher.h"

using RinGlobal::PlaylistItem;
using RinGlobal::VideoPlayerMode;

class VideoPlaylistWidget : public QWidget
{
    Q_OBJECT

public:
    VideoPlaylistWidget(QWidget *parent = nullptr);

    void addItem(const PlaylistItem &item);
    void clear();
    QList<PlaylistItem> items() const;
    PlaylistItem currentItem() const;

signals:
    void itemSelected(const PlaylistItem &item);
    void playRequested(const QString id, VideoPlayerMode mode);
    void initRequested(const QString &source, VideoPlayerMode mode);
    void clearRequested();
    void setMode(VideoPlayerMode mode);

private slots:
    void onAddButtonClicked();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onClearClicked();
    void onModeTabChanged(int index);
    void onRandomClicked();

    // 右键菜单槽函数
    void showContextMenu(const QPoint &pos);
    void onPlayActionTriggered();
    void onRemoveActionTriggered();

private:
    void setupUi();
    void setupKeyboardShortcuts();
    VideoPlayerMode currentTabMode() const;
    PlaylistItem createItemFromInput() const;

    // 右键菜单相关
    void createContextMenu();
    int getSelectedItemIndex() const;
    PlaylistItem getSelectedPlaylistItem() const;

private:
    QListWidget *m_listWidget;
    QLineEdit *m_inputEdit;
    QPushButton *m_addBtn;
    QButtonGroup *m_tabGroup;
    QPushButton *m_clearBtn;
    QPushButton *m_randomBtn;
    QHBoxLayout *m_tabLayout;
    QStringList tabTitles;

    QList<PlaylistItem> m_items;

    // 右键菜单
    QMenu *m_contextMenu;
    QAction *m_playAction;
    QAction *m_removeAction;

    // 当前右键选中的项索引
    int m_contextMenuIndex = -1;
};

#endif // VIDEOPLAYLISTWIDGET_H