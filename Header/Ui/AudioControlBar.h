#ifndef RINZEPLAYER_AUDIOCONTROLBAR_H
#define RINZEPLAYER_AUDIOCONTROLBAR_H
#include <QWidget>
#include <QWindow>
#include <QMouseEvent>
#include <QGuiApplication>

#include "BaseControlBar.h"
#include "../AudioPart/AudioPlay/AudioPlayer.h"
#include "../Header/Controller/PlayQueueController.h"
#include "../Header/Ui/MusicSheetUI.h"
#include "AudioPart/AudioMetaParser.h"
#include "../Ui/AudioPlayListWidget.h"

class AudioPlaylistPopup;


class AudioControlBar : public BaseControlBar
{
    Q_OBJECT
    explicit AudioControlBar(QWidget *parent = nullptr);
public:
    // 禁用拷贝构造和赋值运算符
    ~AudioControlBar();
    AudioControlBar(const AudioControlBar&) = delete;
    AudioControlBar& operator=(const AudioControlBar&) = delete;

    // 设置当前要播放的文件路径（比如从文件选择器调用）
    void setCurrentFilePath(const QString &filePath) { m_currentFilePath = filePath; }

    void insertInfoByMeta()
    {
        setTitle(m_currentSongMeta.name);
        setCoverImage(m_currentSongMeta.coverUrl);
        setArtist(m_currentSongMeta.singer);
        qDebug() << "向ControlBar填充内容" << m_currentSongMeta.name << "[&]" << m_currentSongMeta.coverUrl;
        emit sentCurrentSongMetaToUi(m_currentSongMeta);
    }

    // 静态方法：获取全局唯一实例
    static AudioControlBar* getInstance(QWidget *parent = nullptr) {
        if (!m_instance) {
            QMutexLocker locker(&m_mutex);
            if (!m_instance) {
                m_instance = new AudioControlBar(parent);
            }
        }
        return m_instance;
    }

    void openPlaylistDlg() {
        onPlayDialogClicked(); // 内部调用私有槽函数
    }

    // 重写基类纯虚函数
    void setProgress(qint64 currentMs, qint64 totalMs) override;
    void setVolume(int volume) override;
    void setPlayState(PlayState state) override;

    // 新增：设置标题和封面的接口
    void setTitle(const QString &title);
    void setCoverImage(const QString &imagePath);
    void setArtist(const QString &artist)
    {
        artistLabel->setText(artist);
    }

    void connectSignals();
    void reinitUI();

    // 重写鼠标事件用于触发动画
    void mousePressEvent(QMouseEvent *event) override
    {
        // 只有点击控件区域才触发（可根据需要限制点击范围）
        if (rect().contains(event->pos())) {
            emit clicked();
        }
        QWidget::mousePressEvent(event);
    }

    // 重写鼠标释放事件（鼠标按下后释放才算"点击"）
    void mouseReleaseEvent(QMouseEvent *event) override {
        // 仅处理左键点击（可根据需求修改，如支持右键）
        if (event->button() == Qt::LeftButton) {
            emit clicked();  // 发射点击信号
        }
        // 让父类继续处理事件（避免影响其他功能）
        QWidget::mouseReleaseEvent(event);
    }

    // 新增：公共接口，返回当前弹窗指针（MainWindow 直接调用）
    AudioPlaylistPopup* getCurrentPlaylistDlg() const {
        return m_playlistDlg;
    }

    QPoint calculatePopupPosition();

    void ParseCoverInfo(AudioMeta& _meta)
    {
        QString msg;
        if(m_parser->parse(m_currentFilePath,_meta,msg))
        {
            qDebug() << msg;
            m_currentSongMeta.fromAudioMeta(_meta);
            qDebug() << "已填充内容" << m_currentSongMeta.name;
        }
    }
signals:
        // 音频专属信号
    void playListRequested();              // 请求显示播放列表
    void volumeRequested(int value);
    void playPauseRequested();
    void prevRequested();
    void nextRequested();
    void progressSliderReleased(int value);

    // 创建与销毁
    void playlistDlgCreated(AudioPlaylistPopup* _m);
    void playlistDlgDestroyed();

    // 自定义clicked信号，供MainWindow连接
    void clicked();

    void setPlayStatusToPlayList_Ui(const QString & SongId);

    void sentCurrentSongMetaToUi(const SongMeta& meta);


public:
    // 添加获取弹窗的方法
    AudioPlaylistPopup* getPlaylistPopup();

    // 添加初始化连接的方法
    void initializeMusicSheetConnection(MusicSheetUI* musicSheetUi);


private slots:
    void changeTitleScrollStateByPlayState()
    {
        if (!m_titleLabel) return;

        if (m_currentState == PlayState::Playing) {
            m_titleLabel->startScroll();
        } else if (m_currentState == PlayState::Paused) {
            m_titleLabel->stopScroll();
        }
    }

    // 音量控制相关槽函数
    void onVolumeBtnClicked();             // 音量按钮点击（显示/隐藏滑块）
    void onVolumeSliderChanged(int value); // 音量滑块值变化
    void onVolumeSliderReleased();         // 音量滑块释放

    // 音频功能按钮槽函数
    void onPlayListClicked();              // 播放列表按钮点击
    void onVolumeButtonClicked() override; // 实现基类纯虚函数

    // 新增：内部控件信号转发槽（连接到具体按钮）
    void onPlayPauseBtnClicked() { emit playPauseRequested(); }  // 转发播放/暂停信号
    void onPrevBtnClicked() { emit prevRequested(); }            // 转发上一曲信号
    void onNextBtnClicked() { emit nextRequested(); }            // 转发下一曲信号
    void onProgressSliderMoved(int value) { emit progressChanged(value); }  // 转发进度拖动信号
    void onProgressSliderReleased(int value) { emit progressSliderReleased(value); }  // 转发进度确认信号
    void onPlayDialogClicked();

    void showPopupNearby(QWidget *target);
    void onModeButtonClicked();

    // 模式按钮的槽函数
    void onbtnListPlayClicked();
    void onbtnLoopClicked();
    void onbtnRandomClicked();

    void onProgressUpdated(float progress, int64_t currentMs, int64_t totalMs);

    // 进度条用户交互槽函数（无参数版本，用于实际处理用户操作）
    void onProgressSliderPressed();             // 滑块按下时
    void onProgressSliderReleasedInternal();    // 滑块释放时 - 改名为避免冲突

private:
    // 初始化函数
    void initAudioControls();              // 初始化音频专属控件
    void initVolumePopup();                // 初始化音量弹窗
    void adjustBaseControls();             // 调整基类控件样式

    // 信息展示控件
    QLabel *m_coverLabel;                  // 封面显示标签
    ScrollingLabel *m_titleLabel;          // 标题显示标签
    QLabel* artistLabel;

    // 功能按钮
    QPushButton *m_btnPlayList;            // 播放列表按钮

    // 音量弹窗相关（重复了重复了去掉这一块）
    QWidget *m_volumePopup;                // 音量弹窗
    QSlider *m_volumeSlider;               // 音量滑块
    QPoint m_dragStartPos;                 // 拖动起始位置

    bool m_isProgressSliderDragging = false;    // 标记是否正在拖动

    // 静态成员：单例实例和线程锁
    static AudioControlBar* m_instance;
    static QMutex m_mutex;

    QString m_currentFilePath;

    QMetaObject::Connection m_focusConn;
    quint16 m_currentDuration;
private:
    // 播放列表弹窗成员变量（关键：用成员变量存储，避免被销毁）
    AudioPlaylistPopup* m_playlistDlg = nullptr;
    QPoint m_playlistLastPos;

private:
    QPushButton *m_btnModel;
    QWidget *m_ModelPopup;
    QLabel *m_ModelLabel;
    QVBoxLayout *m_popupLayout;

    QPushButton * m_btnListPlay;
    QPushButton * m_btnLoopPlay;
    QPushButton * m_btnRandomPlay;

private:
    // 音频Meta解析部分
    AudioMetaParser* m_parser;
    SongMeta m_currentSongMeta;

    // 歌曲总时长
    qint64 m_totalDurationMs = 0;

    PlayState m_CurrentState = PlayState::Stopped;
};
#endif //RINZEPLAYER_AUDIOCONTROLBAR_H