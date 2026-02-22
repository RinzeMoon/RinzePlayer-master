//
// Created by lsy on 2026/1/16.
//

#ifndef RINZEPLAYER_AVDATAFETCHER_H
#define RINZEPLAYER_AVDATAFETCHER_H

#include "../Global/Global.h"
#include "../Header/VideoPart/AVRes/AVData.h"

using RinGlobal::SourceInfo;
using RinGlobal::TranState;

/**
 *@brief 资源解析的基类,有三种资源解析模式，分别对应三种子类
 **/
class AVDataFetcher : public QObject
{
    Q_OBJECT
public:
    // 构造函数
    explicit AVDataFetcher(QObject *parent = nullptr);
    virtual ~AVDataFetcher();

    // 初始化数据源
    virtual bool init(const SourceInfo& sourceInfo);

    // 开始获取数据
    virtual bool start();

    TranState getState() const { return state_; }

    virtual std::shared_ptr<AVData> getNextAudioPacket() = 0;

    virtual std::shared_ptr<AVData> getNextVideoPacket() = 0;


    // 获取源信息
    const SourceInfo& getSourceInfo() const { return source_info_; }

    signals:
        // 状态变化信号
        void stateChanged(TranState newState);

    // 错误信号
    void errorOccurred(const QString& errorMessage);

    // 进度信号
    void progressChanged(double progress);  // 0.0 ~ 1.0

    // 数据准备好信号
    void dataAvailable();

    // 数据结束信号
    void dataFinished();
public:
    virtual AVCodecParameters* getVideoCodecParams() const = 0;
    virtual AVCodecParameters* getAudioCodecParams() const = 0;
    virtual int getVideoStreamIndex() const = 0;
    virtual int getAudioStreamIndex() const = 0;

    virtual const uint8_t* getVideoExtradata() const = 0;
    virtual int getVideoExtradataSize() const = 0;
    virtual const uint8_t* getAudioExtradata() const = 0;
    virtual int getAudioExtradataSize() const = 0;

    virtual AVRational getVideoTimeBase() const = 0;

    virtual AVRational getAudioTimeBase() const = 0;


protected:
    // 子类需要实现的纯虚函数
    virtual bool doInit() = 0;
    virtual bool doStart() = 0;

protected:
    SourceInfo source_info_;
    TranState state_ = TranState::IDLE;
    std::shared_ptr<AVDataQueue> audio_queue_;
    std::shared_ptr<AVDataQueue> video_queue_;
};


#endif //RINZEPLAYER_AVDATAFETCHER_H