//
// Created by lsy on 2026/1/20.
//

#include "ResMgr.h"

#include "ResMgr.h"
#include "VideoPart/AVDataFetch/AVDataFetcher.h"
#include "VideoPart/AVDataFetch/LocalFileDataFetcher.h"
#include "../Header/Factory/AVDataFetcherFactory.h"
#include <QDebug>

ResMgr::ResMgr(QObject *parent)
    : QObject(parent)
    , m_currentMode(VideoPlayerMode::LOCAL_FILE)
{

}

ResMgr::~ResMgr()
{
    cleanup();
}

void ResMgr::setPlayer(BasePlayerFrame *player)
{
    m_player = player;
}

void ResMgr::setPlayerFrame(BasePlayerFrame *playerFrame)
{
    m_playerFrame = playerFrame;
}


bool ResMgr::isResourceLoaded(const QString &source) const
{
    return (m_fetcher && m_currentSource == source);
}

void ResMgr::initResource(const QString &source, VideoPlayerMode mode)
{
    qDebug() << "ResMgr::initResource - source:" << source;

    if (!m_playerFrame) {
        qWarning() << "ResMgr::initResource - 播放器未设置";
        emit resourceError(source, "播放器未设置");
        return;
    }

    // 直接使用工厂创建Fetcher - 工厂返回unique_ptr
    auto fetcher = AVDataFetcherFactory::createFetcher(source);
    auto Result = AVSourceResolver::resolveSourceInfo(source);

    if (!fetcher) {
        qWarning() << "ResMgr::initResource - 工厂创建Fetcher失败";
        emit resourceError(source, "创建资源获取器失败");
        return;
    }

    qDebug() << "ResMgr::initResource - 工厂创建成功，准备传递给播放器";

    // 将unique_ptr传递给播放器
    if (m_playerFrame->setFetcher(std::move(fetcher))) {
        qDebug() << "ResMgr::initResource - 资源初始化成功";
        emit resourceReady(Result);
    } else {
        qWarning() << "ResMgr::initResource - 播放器接收失败";
        emit resourceError(source, "播放器无法接收资源");
    }
}

void ResMgr::playResource(const QString &source, VideoPlayerMode mode)
{
    if (!m_player) {
        qWarning() << "ResMgr: Player not set!";
        return;
    }

    // 如果资源未加载或不是当前资源，先初始化
    if (!m_fetcher || m_currentSource != source) {
        initResource(source, mode);

        // 简单等待后播放（实际应该用信号连接，这里简化）
        QTimer::singleShot(100, this, [this, source, mode]() {
            if (m_fetcher && m_currentSource == source) {
                // 直接调用player的loadMedia方法
                m_player->loadMedia(source, mode);
            }
        });
    } else {
        // 资源已加载，直接播放
        m_player->loadMedia(source, mode);
    }
}

void ResMgr::stopResource()
{
    if (m_player) {
        // 假设BasePlayerFrame有stop()方法
        // 如果没有，需要您提供停止播放的方法
    }

    QString stoppedSource = m_currentSource;
    cleanupCurrentFetcher();
    emit resourceStopped(stoppedSource);
}

void ResMgr::cleanup()
{
    cleanupCurrentFetcher();
    m_player = nullptr;
}


void ResMgr::cleanupCurrentFetcher()
{
    if (m_fetcher) {
        // 停止Fetcher（如果AVDataFetcher有stop方法）
        // 如果没有stop方法，需要停止其内部线程

        delete m_fetcher;
        m_fetcher = nullptr;
    }

    m_currentSource.clear();
}