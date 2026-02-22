//
// Created by lsy on 2026/1/17.
//

#ifndef MEDIADATAFETCHER_H
#define MEDIADATAFETCHER_H

#include "AVDataFetcher.h"
#include "../../RingBuffer/RingBuffer.h"
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <deque>
#include <mutex>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

class MediaDataFetcher : public AVDataFetcher
{
    Q_OBJECT

public:
    explicit MediaDataFetcher(QObject *parent = nullptr);
    ~MediaDataFetcher();

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

    AVRational getVideoTimeBase() const override { return video_time_base_; }
    AVRational getAudioTimeBase() const override { return audio_time_base_; }

protected:
    bool doInit() override;
    bool doStart() override;

private:
    struct PacketWrapper {
        AVPacket* packet = nullptr;
        int stream_index = -1;
        int64_t pts = AV_NOPTS_VALUE;
        int64_t dts = AV_NOPTS_VALUE;
        bool is_key_frame = false;

        PacketWrapper() = default;
        PacketWrapper(AVPacket* src_pkt, int idx) {
            if (!src_pkt) return;
            packet = av_packet_alloc();
            av_packet_ref(packet, src_pkt);
            stream_index = idx;
            pts = src_pkt->pts;
            dts = src_pkt->dts;
            is_key_frame = (src_pkt->flags & AV_PKT_FLAG_KEY) != 0;
        }
        PacketWrapper(PacketWrapper&& other) noexcept {
            packet = other.packet;
            stream_index = other.stream_index;
            pts = other.pts;
            dts = other.dts;
            is_key_frame = other.is_key_frame;
            other.packet = nullptr;
        }
        PacketWrapper& operator=(PacketWrapper&& other) noexcept {
            if (this == &other) return *this;
            if (packet) {
                av_packet_unref(packet);
                av_packet_free(&packet);
            }
            packet = other.packet;
            stream_index = other.stream_index;
            pts = other.pts;
            dts = other.dts;
            is_key_frame = other.is_key_frame;
            other.packet = nullptr;
            return *this;
        }
        PacketWrapper(const PacketWrapper&) = delete;
        PacketWrapper& operator=(const PacketWrapper&) = delete;
        ~PacketWrapper() {
            if (packet) {
                av_packet_unref(packet);
                av_packet_free(&packet);
            }
        }
    };

    struct VideoPacketCompare {
        bool operator()(const PacketWrapper& a, const PacketWrapper& b) {
            return a.dts < b.dts;
        }
    };

    void readNetworkThread();
    void processBufferThread();

    void pushVideoPacketToTempQueue(PacketWrapper&& wrapper);
    void flushTempQueueToRingBuffer();
    void cleanup();

private:
    AVFormatContext* fmt_ctx_ = nullptr;
    AVCodecContext*  video_codec_ctx_ = nullptr;
    AVCodecContext*  audio_codec_ctx_ = nullptr;

    int video_stream_idx_ = -1;
    int audio_stream_idx_ = -1;
    AVRational video_time_base_{0, 0};
    AVRational audio_time_base_{0, 0};
    AVCodecParameters* video_codecpar_ = nullptr;
    AVCodecParameters* audio_codecpar_ = nullptr;

    FileInfo file_info_;
    QString url_;

    std::atomic<bool> should_stop_{false};
    std::atomic<bool> is_paused_{false};
    std::atomic<bool> is_eos_{false};

    std::thread read_thread_;
    std::thread process_thread_;

    static constexpr size_t RING_BUFFER_CAPACITY = 400;
    std::unique_ptr<RingBuffer<PacketWrapper>> audio_ring_buffer_;
    std::unique_ptr<RingBuffer<PacketWrapper>> video_ring_buffer_;

    std::deque<PacketWrapper> video_temp_queue_;
    std::mutex video_temp_mutex_;

    std::mutex queue_mutex_;
};

#endif // MEDIADATAFETCHER_H