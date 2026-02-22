//
// Created by lsy on 2026/1/17.
//

#ifndef LOCALFILEDATAFETCHER_H
#define LOCALFILEDATAFETCHER_H

#include "AVDataFetcher.h"
#include "../Global/Global.h"
#include "../../RingBuffer/RingBuffer.h"
#include <memory>
#include <vector>
#include <thread>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

using RinGlobal::VideoPlayerMode;


class LocalFileDataFetcher : public AVDataFetcher
{
    Q_OBJECT

public:
    explicit LocalFileDataFetcher(QObject *parent = nullptr);
    ~LocalFileDataFetcher();

    // 获取文件信息
    struct FileInfo {
        int64_t durationMs = 0;
        int videoWidth = 0;
        int videoHeight = 0;
        int videoStreamIndex = -1;
        int audioStreamIndex = -1;
        QString videoCodec;
        QString audioCodec;
        double fps = 0;
        int64_t bitrate = 0;
    };

    FileInfo getFileInfo() const { return file_info_; }

    std::shared_ptr<AVData> getNextAudioPacket() override {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return audio_queue_->tryPop();
    }

    std::shared_ptr<AVData> getNextVideoPacket() override {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return video_queue_->tryPop();
    }

    AVCodecParameters* getVideoCodecParams() const override;
    AVCodecParameters* getAudioCodecParams() const override;
    int getVideoStreamIndex() const override;
    int getAudioStreamIndex() const override;

    const uint8_t* getVideoExtradata() const override;
    int getVideoExtradataSize() const override;
    const uint8_t* getAudioExtradata() const override;
    int getAudioExtradataSize() const override;

    AVRational getVideoTimeBase() const override
    {
        return video_time_base_;
    }

    AVRational getAudioTimeBase() const override
    {
        return audio_time_base_;
    }


protected:
    bool doInit() override;
    bool doStart() override;

private:
    // AVPacket包装结构（用于环形缓冲区）
    struct PacketWrapper {
        AVPacket* packet = nullptr; // 仅自己管理的指针
        int stream_index = -1;
        int64_t pts = AV_NOPTS_VALUE;
        int64_t dts = AV_NOPTS_VALUE;
        bool is_key_frame = false;

        PacketWrapper() = default;
        // 构造函数：深拷贝Packet，完全接管资源（核心修复）
        PacketWrapper(AVPacket* src_pkt, int idx) {
            if (!src_pkt) return;

            // 1. 分配新的Packet对象（独立于原packet）
            packet = av_packet_alloc();
            // 2. 深拷贝数据（引用计数+1，资源和原packet解耦）
            av_packet_ref(packet, src_pkt);

            // 3. 拷贝元信息（不再依赖原packet）
            stream_index = idx;
            pts = src_pkt->pts;
            dts = src_pkt->dts;
            is_key_frame = (src_pkt->flags & AV_PKT_FLAG_KEY) != 0;
        }

        // 移动构造：避免拷贝，转移资源所有权（必须有）
        PacketWrapper(PacketWrapper&& other) noexcept {
            packet = other.packet;
            stream_index = other.stream_index;
            pts = other.pts;
            dts = other.dts;
            is_key_frame = other.is_key_frame;

            // 清空原对象，避免重复释放
            other.packet = nullptr;
        }

        PacketWrapper& operator=(PacketWrapper&& other) noexcept {
            if (this == &other) return *this; // 防止自赋值

            // 1. 释放当前对象的资源
            if (packet) {
                av_packet_unref(packet);
                av_packet_free(&packet);
            }

            // 2. 移动其他对象的资源
            packet = other.packet;
            stream_index = other.stream_index;
            pts = other.pts;
            dts = other.dts;
            is_key_frame = other.is_key_frame;

            // 3. 清空源对象，避免重复释放
            other.packet = nullptr;
            return *this;
        }

        // 禁止拷贝：彻底杜绝浅拷贝问题
        PacketWrapper(const PacketWrapper&) = delete;
        PacketWrapper& operator=(const PacketWrapper&) = delete;

        // 析构函数：自动释放资源（资源持续时间和Wrapper一致）
        ~PacketWrapper() {
            if (packet) {
                av_packet_unref(packet); // 释放数据缓冲区
                av_packet_free(&packet); // 释放Packet对象
            }
        }
    };

    struct VideoPacketCompare {
        bool operator()(const PacketWrapper& a, const PacketWrapper& b) {
            return a.dts < b.dts; // 按DTS升序排序
        }
    };

    // 工作线程函数
    void readFileThread();              // 读取线程（生产者）
    void processBufferThread();         // 处理线程（消费者）

    // 解码数据包
    std::vector<std::shared_ptr<AVData>> decodePacket(AVPacket* packet);

    // 清理资源
    void cleanup();

private:
    // FFmpeg上下文
    AVFormatContext* format_ctx_ = nullptr;
    AVCodecContext* video_codec_ctx_ = nullptr;
    AVCodecContext* audio_codec_ctx_ = nullptr;
    const AVCodec* video_codec_ = nullptr;
    const AVCodec* audio_codec_ = nullptr;

    // 文件信息
    FileInfo file_info_;
    QString file_path_;

    // 控制标志
    std::atomic<bool> should_stop_{false};
    std::atomic<bool> is_paused_{false};
    std::atomic<bool> is_eos_{false};  // 文件结束标志

    // 工作线程
    std::thread read_thread_;           // 读取线程
    std::thread process_thread_;        // 处理线程

    // 环形缓冲区（数据中转站）
    std::unique_ptr<RingBuffer<PacketWrapper>> ring_buffer_;

    // 缓冲区配置
    static constexpr size_t RING_BUFFER_CAPACITY = 300;  // 环形缓冲区容量

    // 原有队列和锁保持不变
    std::mutex queue_mutex_;            // data_queue_的锁

    std::deque<PacketWrapper> video_temp_queue_; // 视频DTS排序临时队列
    std::mutex video_temp_mutex_;               // 临时队列锁
    AVRational video_time_base_;                // 视频时间基（原有声明补全）
    AVRational audio_time_base_;                // 音频时间基（新增）

    void pushVideoPacketToTempQueue(PacketWrapper&& wrapper)
    {
        std::lock_guard<std::mutex> lock(video_temp_mutex_);
        // ========== 核心修改：强制从AVPacket提取关键帧标记 ==========
        if (wrapper.packet) {
            wrapper.is_key_frame = (wrapper.packet->flags & AV_PKT_FLAG_KEY) != 0;
            // 加日志验证：找到关键帧包
            if (wrapper.is_key_frame) {
                qDebug() << "[PushVideo] 找到关键帧包！ DTS=" << wrapper.dts << " PTS=" << wrapper.pts;
            }
        } else {
            wrapper.is_key_frame = false;
        }
        video_temp_queue_.push_back(std::move(wrapper));
        std::sort(video_temp_queue_.begin(), video_temp_queue_.end(), VideoPacketCompare());
    }

    void flushTempQueueToRingBuffer() {
        std::lock_guard<std::mutex> lock(video_temp_mutex_);
        if (video_temp_queue_.empty()) return;

        // 按DTS升序刷入环形缓冲区（复用原有ring_buffer_）
        for (auto& wrapper : video_temp_queue_) {
            ring_buffer_->emplace(5, std::move(wrapper));
        }
        video_temp_queue_.clear();
    }
};


#endif // LOCALFILEDATAFETCHER_H