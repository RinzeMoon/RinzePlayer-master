#include <QObject>
#include <condition_variable>
#include "../Header/VideoPart/AVRes/AVData.h"

class AVSync
{
public:
    enum class SyncAction {
        Display,
        Wait,
        Drop
    };
    
    AVSync();
    ~AVSync();
    
    // 基本操作
    void putAudio(std::shared_ptr<AVData> audio_data);
    void putVideo(std::shared_ptr<AVData> video_data);
    std::shared_ptr<AVData> getNextAudioFrame(int timeout_ms);
    std::shared_ptr<AVData> getNextVideoFrame(int timeout_ms);
    
    // 时钟管理
    void setAudioClock(double audio_time_ms);
    double getAudioClock() const;
    
    // 重置状态
    void flush();
    
    // 状态查询
    bool hasAudio() const;
    bool hasVideo() const;
    int getVideoQueueSize() const;
    int getAudioQueueSize() const;
    
private:
    // 内部数据结构
    struct VideoFrameInfo {
        std::shared_ptr<AVData> frame;
        double frame_pts_ms;
        bool is_keyframe;
    };
    
    // 对齐和同步
    struct SyncState {
        double av_offset = 0.0;      // 对齐偏移：video_pts = audio_time + offset
        bool aligned = false;        // 是否已对齐
        double last_frame_pts = 0.0; // 上一帧PTS（用于计算帧间隔）
        std::chrono::steady_clock::time_point last_display_time;
    };
    
    // 队列
    std::queue<std::shared_ptr<AVData>> audio_queue_;
    std::queue<VideoFrameInfo> video_queue_;
    
    // 同步状态
    SyncState sync_state_;
    
    // 同步阈值（优化：放宽阈值以适应高比特率AAC）
    double sync_threshold_ms_ = 80.0;   // 同步阈值（毫秒）- 从40ms增加到80ms
    double drop_threshold_ms_ = 1000.0;  // 丢弃阈值（毫秒）- 从300ms增加到1000ms
    
    // 控制变量
    std::atomic<bool> is_running_{true};
    std::atomic<double> audio_clock_ms_{0.0};
    
    // 互斥锁和条件变量
    mutable std::mutex audio_mutex_;
    mutable std::mutex video_mutex_;
    std::condition_variable audio_cv_;
    std::condition_variable video_cv_;
    
    // 私有方法
    void alignVideoWithAudio(double frame_pts_ms, double audio_time_ms);
    double calculateSyncDiff(double frame_pts_ms, double audio_time_ms) const;
    SyncAction checkSync(double sync_diff, bool is_keyframe) const;
    double calculateWaitTime(double sync_diff) const;
    double getVideoFPS() const;
};