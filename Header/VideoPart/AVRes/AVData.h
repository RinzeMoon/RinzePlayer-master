//
// Created by lsy on 2026/1/16.
//

#ifndef RINZEPLAYER_AVRESOURCE_H
#define RINZEPLAYER_AVRESOURCE_H

#include <QString>
#include "../Global/Global.h"
#include <mutex>
#include <queue>
#include <condition_variable>
#include <memory>

extern "C"
{
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
}

using VideoUtil::VideoFrame;
using RinGlobal::AVDataType;

class AVData
{
public:
    AVData(AVDataType type) : type_(type)
    {

    }

    ~AVData()
    {
        cleanup();
    }

    AVData(const AVData&) = delete;
    AVData& operator=(const AVData&) = delete;

    // 移动构造函数
    AVData(AVData&& other) noexcept {
        moveFrom(other);
    }

    AVData& operator=(AVData&& other) noexcept {
        if (this != &other) {
            cleanup();
            moveFrom(other);
        }
        return *this;
    }

    using CleanupFunc = std::function<void()>;

    AVDataType getType() {return type_;}

    bool isAudio() const { return type_ == AVDataType::AUDIO_FRAME; }
    bool isVideo() const { return type_ == AVDataType::VIDEO_FRAME; }
    bool isPacket() const { return type_ == AVDataType::PACKET; }
    bool isEOS() const { return type_ == AVDataType::END_OF_STREAM; }

    // 设置/获取流索引
    void setStreamIndex(int index) { stream_index_ = index; }
    int getStreamIndex() const { return stream_index_; }

    // 时间戳管理
    void setPts(int64_t pts) { pts_ = pts; }
    int64_t pts() const { return pts_; }

    void setDts(int64_t dts) { dts_ = dts; }
    int64_t dts() const { return dts_; }

    AudioUtil::AudioFrame* audioFrame() {
        return isAudio() ? audio_frame_ptr_.get() : nullptr;
    }

    const AudioUtil::AudioFrame* audioFrame() const {
        return isAudio() ? audio_frame_ptr_.get() : nullptr;
    }

    // 获取视频帧（如果是视频数据）
    VideoUtil::VideoFrame* videoFrame() {
        return isVideo() ? video_frame_ptr_.get() : nullptr;
    }

    const VideoUtil::VideoFrame* videoFrame() const {
        return isVideo() ? video_frame_ptr_.get() : nullptr;
    }

    // 获取Packet（如果是编码数据）
    AVPacket* packet() {
        return isPacket() ? packet_ptr_.get() : nullptr;
    }

    const AVPacket* packet() const {
        return isPacket() ? packet_ptr_.get() : nullptr;
    }

// 创建音频数据（工厂方法） - 基于你的AudioFrame结构，深拷贝数据
static std::shared_ptr<AVData> createAudio(
    uint8_t** data,
    int nb_samples,
    int sample_rate,
    int channels,
    AVSampleFormat fmt,
    int64_t pts = AV_NOPTS_VALUE
) {
    // 前置校验：核心参数不能为空/无效
    if (!data || nb_samples <= 0 || sample_rate <= 0 || channels <= 0 || fmt == AV_SAMPLE_FMT_NONE) {
        qWarning() << "[createAudio] 无效参数：data=" << (void*)data
                  << "nb_samples=" << nb_samples << "fmt=" << av_get_sample_fmt_name(fmt);
        return nullptr;
    }

    auto avdata = std::make_shared<AVData>(AVDataType::AUDIO_FRAME);
    auto audio_frame = std::make_shared<AudioUtil::AudioFrame>();

    // ========== 核心1：深拷贝音频数据（解决电流声的关键） ==========
    // 1. 计算平面数：平面格式（如AV_SAMPLE_FMT_S16P）每个声道一个平面，非平面格式只有1个平面
    int nb_planes = av_sample_fmt_is_planar(fmt) ? channels : 1;
    // 2. 分配平面指针数组（与FFmpeg的data结构对齐）
    audio_frame->data = static_cast<uint8_t**>(av_mallocz(nb_planes * sizeof(uint8_t*)));
    if (!audio_frame->data) {
        qWarning() << "[createAudio] 分配平面指针数组失败";
        return nullptr;
    }

    // 3. 逐平面深拷贝实际音频数据（用linesize作为每个平面的字节数）
    // 先获取FFmpeg解码后的linesize（你的AudioFrame里的linesize字段）
    audio_frame->linesize = av_samples_get_buffer_size(nullptr, channels, nb_samples, fmt, 1);
    for (int i = 0; i < nb_planes; ++i) {
        // 分配与原数据等长的内存
        audio_frame->data[i] = static_cast<uint8_t*>(av_malloc(audio_frame->linesize));
        if (!audio_frame->data[i]) {
            // 内存分配失败，回滚已分配的内存（避免泄漏）
            for (int j = 0; j < i; ++j) {
                av_free(audio_frame->data[j]);
            }
            av_free(audio_frame->data);
            qWarning() << "[createAudio] 分配第" << i << "个平面数据失败";
            return nullptr;
        }

        memcpy(audio_frame->data[i], data[i], audio_frame->linesize);
    }

    // ========== 核心2：填充你的AudioFrame所有字段 ==========
    audio_frame->nb_samples = nb_samples;
    audio_frame->sample_rate = sample_rate;
    audio_frame->channels = channels;
    audio_frame->sample_fmt = fmt;
    audio_frame->pts = pts;

    // ========== 核心3：设置清理函数，确保数据析构时释放 ==========
    avdata->setCleanupFunc([audio_frame]() {
        if (audio_frame->data) {
            int nb_planes = av_sample_fmt_is_planar(audio_frame->sample_fmt)
                ? audio_frame->channels : 1;
            // 释放每个平面的实际数据
            for (int i = 0; i < nb_planes; ++i) {
                if (audio_frame->data[i]) {
                    av_free(audio_frame->data[i]);
                    audio_frame->data[i] = nullptr;
                }
            }
            // 释放平面指针数组
            av_free(audio_frame->data);
            audio_frame->data = nullptr;
        }
        // 重置辅助字段
        audio_frame->linesize = 0;
        audio_frame->nb_samples = 0;
    });

    // 绑定音频帧到AVData
    avdata->audio_frame_ptr_ = audio_frame;
    avdata->pts_ = pts;
    avdata->dts_ = pts;

    return avdata;
}

    // 创建视频数据（工厂方法） - 修改为使用智能指针
    static std::shared_ptr<AVData> createVideo(
    uint8_t** data_array,       // 新增：多平面指针数组（Y/U/V）
    const int linesize[4],      // 新增：每个平面的行大小
    int width,
    int height,
    AVPixelFormat fmt,
    int64_t pts = AV_NOPTS_VALUE,
    bool key_frame = false
) {
        auto avdata = std::make_shared<AVData>(AVDataType::VIDEO_FRAME);

        // 创建VideoFrame智能指针
        auto video_frame = std::make_shared<VideoUtil::VideoFrame>();

        // 【核心修复1】填充多平面指针数组（YUV420P 的核心）
        for (int i = 0; i < 4; ++i) {
            video_frame->data_array[i] = data_array[i];  // 赋值 Y/U/V 指针
            video_frame->linesize[i] = linesize[i];      // 赋值行大小
        }
        // 【核心修复2】单指针 data 仅作为 Y 分量的兜底（保持兼容）
        video_frame->data = data_array[0];

        // 原有字段赋值（不变）
        video_frame->width = width;
        video_frame->height = height;
        video_frame->pix_fmt = fmt;
        video_frame->pts = pts;
        video_frame->key_frame = key_frame;

        avdata->video_frame_ptr_ = video_frame;
        avdata->pts_ = pts;
        avdata->dts_ = pts;
        avdata->is_key_frame_ = key_frame;

        return avdata;
    }

    // 创建AVPacket数据 - 修改为使用智能指针
    static std::shared_ptr<AVData> createPacket(
        AVPacket* src_packet = nullptr
    ) {
        auto avdata = std::make_shared<AVData>(AVDataType::PACKET);

        // 创建AVPacket智能指针
        auto packet = std::shared_ptr<AVPacket>(new AVPacket(),
            [](AVPacket* p) {
                if (p) {
                    av_packet_unref(p);
                    delete p;
                }
            });

        av_init_packet(packet.get());

        if (src_packet) {
            av_packet_ref(packet.get(), src_packet);
            avdata->pts_ = src_packet->pts;
            avdata->dts_ = src_packet->dts;
        } else {
            packet->data = nullptr;
            packet->size = 0;
            avdata->pts_ = AV_NOPTS_VALUE;
            avdata->dts_ = AV_NOPTS_VALUE;
        }

        avdata->packet_ptr_ = packet;
        return avdata;
    }

    // 创建流结束标志
    static std::shared_ptr<AVData> createEOS() {
        return std::make_shared<AVData>(AVDataType::END_OF_STREAM);
    }

    void setPacket(AVPacket* pkt);          // 设置数据包指针
    void setKeyFrame(bool is_key_frame);    // 设置是否为关键帧
    bool isKeyFrame() const { return is_key_frame_; } // 获取关键帧状态的方法


    void setCleanupFunc(CleanupFunc func) {
        cleanup_func_ = func;
    }


    // 此部分为音视频包的同步指标

    struct SyncInfo {
        double audio_pts_ms;      // 当前音频PTS（毫秒）
        double video_pts_ms;      // 当前视频PTS（毫秒）
        double drift_ms;          // 音视频漂移（毫秒）
        double display_delay_ms;  // 建议显示延迟（毫秒）
        int sync_status;          // 同步状态
        double target_display_time_ms; // 目标显示时间（绝对时间，毫秒）

        SyncInfo()
            : audio_pts_ms(0.0)
            , video_pts_ms(0.0)
            , drift_ms(0.0)
            , display_delay_ms(0.0)
            , sync_status(0)
            , target_display_time_ms(0.0) {}
    };

    // 设置同步信息
    void setSyncInfo(const SyncInfo& info) {
        sync_info_ = info;
    }

    // 获取同步信息
    const SyncInfo& getSyncInfo() const {
        return sync_info_;
    }

    // 计算应该显示的时间（考虑延迟）
    double getTargetDisplayTimeMs() const {
        return sync_info_.target_display_time_ms;
    }

    // 获取建议延迟（毫秒）
    double getDisplayDelayMs() const {
        return sync_info_.display_delay_ms;
    }

    // 获取当前漂移（毫秒）
    double getDriftMs() const {
        return sync_info_.drift_ms;
    }

    // 检查是否应该立即显示
    bool shouldDisplayImmediately() const {
        return sync_info_.display_delay_ms <= 1.0;
    }
private:
    void cleanup();

    void moveFrom(AVData& other);

private:
    AVDataType type_;
    int64_t pts_ = AV_NOPTS_VALUE;
    int64_t dts_ = AV_NOPTS_VALUE;
    int stream_index_ = 0;

    // 用智能指针替换原来的union
    std::shared_ptr<AudioUtil::AudioFrame> audio_frame_ptr_;
    std::shared_ptr<VideoUtil::VideoFrame> video_frame_ptr_;
    std::shared_ptr<AVPacket> packet_ptr_ = nullptr;

    bool is_key_frame_ = false;                      // 存储是否是关键帧的标记

    CleanupFunc cleanup_func_;

    SyncInfo sync_info_;

};

// AVDataQueue 保持不变，完全适配智能指针
class AVDataQueue {
public:
    void push(std::shared_ptr<AVData> data) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(data));
        cond_.notify_one();
    }

    std::shared_ptr<AVData> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return !queue_.empty(); });

        auto data = std::move(queue_.front());
        queue_.pop();
        return data;
    }

    std::shared_ptr<AVData> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return nullptr;
        }

        auto data = std::move(queue_.front());
        queue_.pop();
        return data;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
    }

private:
    std::queue<std::shared_ptr<AVData>> queue_; //队列
    mutable std::mutex mutex_;      // 锁
    std::condition_variable cond_;  //条件变量
};

#endif //RINZEPLAYER_AVRESOURCE_H