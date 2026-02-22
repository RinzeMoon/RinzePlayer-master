//
// Created by lsy on 2026/1/20.
//

#ifndef RESMGR_H
#define RESMGR_H

#include <QObject>
#include <QString>
#include "../Global/Global.h"
#include "../BasePlayerFrame.h"

using RinGlobal::VideoPlayerMode;

// 前向声明
class BasePlayerFrame;
class AVDataFetcher;

class ResMgr : public QObject
{
    Q_OBJECT

public:
    explicit ResMgr(QObject *parent = nullptr);
    ~ResMgr();

    // 设置播放器（必须调用）
    void setPlayer(BasePlayerFrame *player);

    // 获取当前播放器
    BasePlayerFrame* player() const { return m_player; }

    // 获取当前Fetcher
    AVDataFetcher* fetcher() const { return m_fetcher; }

    // 检查资源是否已加载
    bool isResourceLoaded(const QString &source) const;

    void setPlayerFrame(BasePlayerFrame *playerFrame);

public slots:
    // 初始化资源
    void initResource(const QString &source, VideoPlayerMode mode);

    // 播放资源
    void playResource(const QString &source, VideoPlayerMode mode);

    // 停止当前资源
    void stopResource();

    // 清理所有资源
    void cleanup();

    signals:
        // 资源状态
        void resourceReady(const SourceInfo &Result);
    void resourceError(const QString &source, const QString &error);
    void resourceStopped(const QString &source);

private:
    // 清理当前Fetcher
    void cleanupCurrentFetcher();

private:
    BasePlayerFrame *m_player = nullptr;
    AVDataFetcher *m_fetcher = nullptr;
    QString m_currentSource;
    VideoPlayerMode m_currentMode;
private:
    BasePlayerFrame *m_playerFrame = nullptr;
};

#endif // RESMGR_H