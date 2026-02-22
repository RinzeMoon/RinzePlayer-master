//
// Created by lsy on 2026/1/16.
//

#include "../../../Header/VideoPart/AVDataFetch/AVDataFetcher.h"

AVDataFetcher::AVDataFetcher(QObject *parent) : QObject(parent)
{
    qDebug() << "[AVDataFetcher] 构造函数";
    state_ = TranState::STOPPED;  // 明确设置初始状态
}

// 补全析构函数实现
AVDataFetcher::~AVDataFetcher()
{
    // 这里写析构逻辑，释放资源等，空的话留空即可
}

// 补全所有声明的虚函数实现 (按.h里的声明补)
// AVDataFetcher.cpp
bool AVDataFetcher::init(const SourceInfo& sourceInfo)
{
    qDebug() << "[AVDataFetcher::init] === 开始 ===";
    qDebug() << "  this 地址:" << this;

    // 打印传入的源信息
    qDebug() << "  传入的 sourceInfo:";
    qDebug() << "    isValid:" << sourceInfo.isValid;
    qDebug() << "    mode:" << static_cast<int>(sourceInfo.mode);
    qDebug() << "    url:" << sourceInfo.url;
    qDebug() << "    nativePath:" << sourceInfo.nativePath;

    // 保存传入的源信息
    source_info_ = sourceInfo;

    // 打印保存后的源信息
    qDebug() << "  保存后的 source_info_:";
    qDebug() << "    isValid:" << source_info_.isValid;
    qDebug() << "    mode:" << static_cast<int>(source_info_.mode);
    qDebug() << "    url:" << source_info_.url;
    qDebug() << "    nativePath:" << source_info_.nativePath;

    state_ = TranState::INITIALIZING;
    emit stateChanged(state_);

    qDebug() << "  调用子类的 doInit()";
    bool result = doInit();
    qDebug() << "  doInit() 返回:" << result;

    if (result) {
        state_ = TranState::IDLE;
        qDebug() << "  初始化成功，状态变为IDLE";
    } else {
        state_ = TranState::ERROR;
        qDebug() << "  初始化失败，状态变为ERROR";
    }

    emit stateChanged(state_);
    qDebug() << "[AVDataFetcher::init] === 结束，返回:" << result << " ===";
    return result;
}

bool AVDataFetcher::start()
{
    // 空实现即可
    qDebug() << "[start] in AVDataFetcher Base Class";
    doStart();
    return true;
}
