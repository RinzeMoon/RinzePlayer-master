//
// Created by lsy on 2025/10/24.
//

#ifndef RINZEPLAYER_MUSICSHEETUI_H
#define RINZEPLAYER_MUSICSHEETUI_H

#include <../Global/Global.h>
using AudioUtil::SongMeta;
using AudioUtil::MusicSheet;

#include <QEvent>
#include <QMouseEvent>
#include <QList>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QTableWidgetItem>

class MusicSheetUI : public QWidget
{
    Q_OBJECT
public:
    explicit MusicSheetUI(QWidget *parent = nullptr);

    // 歌单信息：getter/setter（严格对应UI元素）
    void setSheetTag(const QString& tag);       // 如"歌单"标签
    QString getSheetTag() const;


    void setSheetId(const QString& id);         // 歌单ID
    QString getSheetId() const;

    void setSheetTitle(const QString& title);   // 歌单标题
    QString getSheetTitle() const;

    void setSongCount(int count);               // 歌曲数量（如30首）
    int getSongCount() const;

    void setPlayCount(int count);               // 播放量（如25.8k）
    int getPlayCount() const;

    void setSheetDesc(const QString& desc);     // 歌单描述文本
    QString getSheetDesc() const;

    void setSheetCover(const QPixmap& cover);   // 歌单封面图（直接传QPixmap，避免路径问题）
    QPixmap getSheetCover() const;

    // 歌曲列表操作
    void addSong(const SongMeta& song);         // 添加单首歌曲
    void addSongs(const QList<SongMeta>& songs);// 批量添加歌曲
    void removeSong(const QString& songId);     // 按id移除歌曲
    void clearSongs();                          // 清空列表
    QList<SongMeta> getSongList() const;        // 获取当前列表

    // 播放状态
    void setPlayingSong(const QString& songId); // 标记当前播放歌曲
    QString getPlayingSongId() const;           // 获取当前播放歌曲id

    bool insertInfoBySheet(const MusicSheet& sheet);

    void updatePlayingState(const QString& targetSheetId, const QString& playingSongId);

signals:
    void item_stateChanged(const QString& sheetId,const QString& LabelName);

    void playSongById(const QString& songId,const QString& sheetId);

    // 新增：右键菜单操作信号
    void requestRemoveSongFromSheet(const QString& sheetId, const QString& songId);
    void requestAddSongToOtherSheet(const QString& targetSheetId, const SongMeta& song);

public slots:
    void onSongPlayedUpdateUI_ById(const QString& songId);
private slots:
    void OnMoreBtnClicked();

    void onShowOperateMenu(const QPoint& pos);

    // void onSongRowClicked(int row, int column);
private:
    // UI组件（严格对应HTML结构）
    QString MusicSheetID;           // 歌单的唯一性ID
    QLabel* m_coverLabel;       // 歌单封面（200x200）
    QLabel* m_tagLabel;         // 标签（如"歌单"）
    QLabel* m_titleLabel;       // 主标题
    QLabel* m_songCountLabel;   // 歌曲数量文本
    QLabel* m_playCountLabel;   // 播放量文本
    QLabel* m_descLabel;        // 描述文本
    QPushButton* m_playAllBtn;  // 全部播放按钮
    QPushButton* m_shuffleBtn;  // 随机播放按钮
    QPushButton* m_favBtn;      // 收藏按钮
    QPushButton* m_moreBtn;     // 更多按钮
    QTableWidget* m_songTable;  // 歌曲列表表格

    // 数据
    QList<SongMeta> m_songList; // 歌曲数据列表
    QString m_playingSongId;    // 当前播放歌曲id
    QPixmap m_sheetCover;       // 缓存歌单封面，用于getter
    MusicSheet m_musicSheet;
    QString m_coverPath;

    // 初始化UI（严格还原HTML布局和样式）
    void initUI();

    void initStyle();

    // 更新歌曲表格（同步数据到UI）
    void updateSongTable();
};

#endif //RINZEPLAYER_MUSICSHEETUI_H