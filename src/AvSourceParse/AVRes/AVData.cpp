//
// Created by lsy on 2026/1/16.
//

#include "../../../Header/VideoPart/AVRes/AVData.h"

void AVData::cleanup()
{
    if (cleanup_func_) {
        cleanup_func_();
    }

    // 智能指针会自动管理内存，所以这里只需要重置指针
    audio_frame_ptr_.reset();
    video_frame_ptr_.reset();
    packet_ptr_.reset();
}

void AVData::moveFrom(AVData& other) {
    type_ = other.type_;
    pts_ = other.pts_;
    dts_ = other.dts_;
    stream_index_ = other.stream_index_;

    // 移动智能指针
    audio_frame_ptr_ = std::move(other.audio_frame_ptr_);
    video_frame_ptr_ = std::move(other.video_frame_ptr_);
    packet_ptr_ = std::move(other.packet_ptr_);

    // 重置原对象
    other.type_ = AVDataType::END_OF_STREAM;
    other.pts_ = AV_NOPTS_VALUE;
    other.dts_ = AV_NOPTS_VALUE;
    other.stream_index_ = 0;
}

void AVData::setPacket(AVPacket* pkt) {
    if (!pkt) {
        packet_ptr_.reset();
        return;
    }

    // 创建智能指针管理AVPacket
    auto packet = std::shared_ptr<AVPacket>(new AVPacket(),
        [](AVPacket* p) {
            if (p) {
                av_packet_unref(p);
                delete p;
            }
        });

    av_init_packet(packet.get());
    av_packet_ref(packet.get(), pkt);

    packet_ptr_ = packet;
    pts_ = pkt->pts;
    dts_ = pkt->dts;
    stream_index_ = pkt->stream_index;
    is_key_frame_ = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
}


// 设置关键帧标记
void AVData::setKeyFrame(bool is_key_frame) {
    is_key_frame_ = is_key_frame;
}