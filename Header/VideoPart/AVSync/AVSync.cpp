#include "AVSync.h"
#include <chrono>
#include <thread>
#include <cmath>
#include <QDebug>

AVSync::AVSync() {
    sync_state_.last_display_time = std::chrono::steady_clock::now();
}

AVSync::~AVSync() {
    is_running_ = false;
    audio_cv_.notify_all();
    video_cv_.notify_all();
    flush();
}

void AVSync::flush() {
    {
        std::lock_guard<std::mutex> lock(audio_mutex_);
        std::queue<std::shared_ptr<AVData>>().swap(audio_queue_);
    }
    {
        std::lock_guard<std::mutex> lock(video_mutex_);
        std::queue<VideoFrameInfo>().swap(video_queue_);
    }

    // 重置同步状态
    sync_state_ = SyncState();
    sync_state_.last_display_time = std::chrono::steady_clock::now();
    audio_clock_ms_.store(0.0);
}

// ========== 数据放入 ==========
void AVSync::putAudio(std::shared_ptr<AVData> audio_data) {
    if (!audio_data || !audio_data->isAudio()) return;
    std::lock_guard<std::mutex> lock(audio_mutex_);
    audio_queue_.push(audio_data);
    audio_cv_.notify_one();
}

void AVSync::putVideo(std::shared_ptr<AVData> video_data) {
    if (!video_data || !video_data->isVideo()) return;

    VideoFrameInfo info;
    info.frame = video_data;
    info.frame_pts_ms = static_cast<double>(video_data->pts());
    info.is_keyframe = video_data->isKeyFrame();

    std::lock_guard<std::mutex> lock(video_mutex_);
    video_queue_.push(info);
    video_cv_.notify_one();
}

// ========== 对齐音视频 ==========
void AVSync::alignVideoWithAudio(double frame_pts_ms, double audio_time_ms) {
    if (audio_time_ms <= 0) return;

    if (!sync_state_.aligned) {
        sync_state_.av_offset = frame_pts_ms - audio_time_ms;
        sync_state_.aligned = true;
        sync_state_.last_frame_pts = frame_pts_ms;

        qDebug() << QString("音视频对齐 | 视频PTS: %1ms | 音频时间: %2ms | 偏移量: %3ms")
                        .arg(frame_pts_ms, 0, 'f', 2)
                        .arg(audio_time_ms, 0, 'f', 2)
                        .arg(sync_state_.av_offset, 0, 'f', 2);
    }
}

// ========== 计算同步差值 ==========
double AVSync::calculateSyncDiff(double frame_pts_ms, double audio_time_ms) const {
    if (!sync_state_.aligned || audio_time_ms <= 0) {
        return 0.0;
    }

    // 核心公式：expected_audio_time = frame_pts_ms - av_offset
    double expected_audio_time = frame_pts_ms - sync_state_.av_offset;

    // 同步差值：帧应该显示的时间 - 当前音频时间
    return expected_audio_time - audio_time_ms;
}

// ========== 获取下一帧视频（带同步控制） ==========
std::shared_ptr<AVData> AVSync::getNextVideoFrame(int timeout_ms) {
    std::unique_lock<std::mutex> lock(video_mutex_);

    // 等待视频帧
    if (timeout_ms < 0) {
        video_cv_.wait(lock, [this] {
            return !video_queue_.empty() || !is_running_;
        });
    } else {
        video_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
            return !video_queue_.empty() || !is_running_;
        });
    }

    if (!is_running_ || video_queue_.empty()) {
        return nullptr;
    }

    // 获取当前音频时间
    double audio_time_ms = getAudioClock();

    // 循环处理视频帧队列
    while (!video_queue_.empty()) {
        auto video_info = video_queue_.front();
        double frame_pts_ms = video_info.frame_pts_ms;

        // 1. 对齐音视频（如果是第一帧或需要重新对齐）
        alignVideoWithAudio(frame_pts_ms, audio_time_ms);

        // 2. 计算同步差值
        double sync_diff = calculateSyncDiff(frame_pts_ms, audio_time_ms);
        // 调试日志：每100帧输出一次，避免影响性能
        static int debug_frame_count = 0;
        if (++debug_frame_count % 100 == 0) {
            qDebug() << "Diff" << sync_diff;
        }
        // 3. 检查同步状态
        SyncAction action = checkSync(sync_diff, video_info.is_keyframe);

        // 4. 根据同步动作处理
        if (action == SyncAction::Display) {
            // 可以显示
            video_queue_.pop();

            // 更新上一帧信息
            if (sync_state_.aligned) {
                sync_state_.last_frame_pts = frame_pts_ms;
            }

            // 调试输出
            static int frame_count = 0;
            if (frame_count++ % 30 == 0) {
                qDebug() << QString("视频同步 | 帧PTS: %1ms | 音频: %2ms | 差值: %3ms")
                                .arg(frame_pts_ms, 0, 'f', 2)
                                .arg(audio_time_ms, 0, 'f', 2)
                                .arg(sync_diff, 0, 'f', 2);
            }

            return video_info.frame;
        }
        else if (action == SyncAction::Drop) {
            // 丢弃帧
            static int drop_count = 0;
            if (drop_count++ % 10 == 0) {
                qDebug() << QString("丢弃帧 | PTS: %1ms | 音频: %2ms | 差值: %3ms")
                                .arg(frame_pts_ms, 0, 'f', 2)
                                .arg(audio_time_ms, 0, 'f', 2)
                                .arg(sync_diff, 0, 'f', 2);
            }
            video_queue_.pop();
            continue; // 继续处理下一帧
        }
        else { // SyncAction::Wait
            // 视频帧应该在未来显示，需要等待

            // 计算需要等待的时间
            double wait_ms = calculateWaitTime(sync_diff);

            // 临时释放锁，允许其他线程操作
            lock.unlock();

            // 等待
            if (wait_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(wait_ms)));
            }

            // 重新获取锁和音频时间
            lock.lock();
            audio_time_ms = getAudioClock();

            // 重新计算同步差值
            sync_diff = calculateSyncDiff(frame_pts_ms, audio_time_ms);

            // 检查等待后是否可以显示
            if (sync_diff <= sync_threshold_ms_) {
                video_queue_.pop();

                if (sync_state_.aligned) {
                    sync_state_.last_frame_pts = frame_pts_ms;
                }

                return video_info.frame;
            }

            // 如果等待后仍然不能显示，继续循环处理（可能会丢弃）
            continue;
        }
    }

    return nullptr;
}

// ========== 同步决策 ==========
AVSync::SyncAction AVSync::checkSync(double sync_diff, bool is_keyframe) const {
    // sync_diff = 期望显示时间 - 当前音频时间
    // 正值：帧应该在未来显示（需要等待）
    // 负值：帧应该在过去显示（可能过时）

    // 音频未开始，直接显示
    if (audio_clock_ms_.load() <= 0) {
        return SyncAction::Display;
    }

    // 1. 严重过时（落后超过丢弃阈值）
    if (sync_diff < -drop_threshold_ms_) {
        if (is_keyframe) {
            // 关键帧即使过时也显示，避免花屏
            return SyncAction::Display;
        } else {
            return SyncAction::Drop;
        }
    }
    // 2. 稍微过时（落后但在阈值内）
    if (sync_diff < 0) {
        return SyncAction::Display;
    }

    // 3. 同步良好（在阈值内）
    if (sync_diff <= sync_threshold_ms_) {
        return SyncAction::Display;
    }

    // 4. 需要等待（提前超过阈值）
    if (sync_diff <= drop_threshold_ms_) {
        return SyncAction::Wait;
    }
    // 5. 严重提前（超过丢弃阈值）
    if (is_keyframe) {
        return SyncAction::Wait;
    } else {
        return SyncAction::Drop;
    }
}

// ========== 计算等待时间 ==========
double AVSync::calculateWaitTime(double sync_diff) const {
    // 确保sync_diff是正数（需要等待）
    if (sync_diff <= 0) return 0.0;

    // 优化：如果sync_diff过大（超过2秒），直接返回0，让checkSync决定丢弃
    if (sync_diff > 2000.0) {
        return 0.0;
    }

    // 获取视频帧率
    double fps = getVideoFPS();
    if (fps <= 0) fps = 25.0;
    double frame_interval = 1000.0 / fps;

    // 计算等待时间（不超过1帧的时间，但不超过100ms）
    double wait_ms = std::min(sync_diff, frame_interval * 1.0);
    wait_ms = std::min(wait_ms, 100.0);

    // 保证等待时间合理
    wait_ms = std::max(1.0, std::min(wait_ms, 50.0));

    return wait_ms;
}

// ========== 音频时钟管理 ==========
void AVSync::setAudioClock(double audio_time_ms) {
    audio_clock_ms_.store(audio_time_ms);
}

double AVSync::getAudioClock() const {
    return audio_clock_ms_.load();
}

// ========== 获取视频FPS（简化实现） ==========
double AVSync::getVideoFPS() const {
    // 这里应该从视频流信息获取实际FPS
    // 简化实现，返回一个默认值
    return 30;
}

// ========== 状态查询 ==========
bool AVSync::hasAudio() const {
    std::lock_guard<std::mutex> lock(audio_mutex_);
    return !audio_queue_.empty();
}

bool AVSync::hasVideo() const {
    std::lock_guard<std::mutex> lock(video_mutex_);
    return !video_queue_.empty();
}

int AVSync::getVideoQueueSize() const {
    std::lock_guard<std::mutex> lock(video_mutex_);
    return static_cast<int>(video_queue_.size());
}

int AVSync::getAudioQueueSize() const {
    std::lock_guard<std::mutex> lock(audio_mutex_);
    return static_cast<int>(audio_queue_.size());
}

// ========== 获取下一帧音频 ==========
std::shared_ptr<AVData> AVSync::getNextAudioFrame(int timeout_ms) {
    std::unique_lock<std::mutex> lock(audio_mutex_);

    int real_timeout = std::max(1, timeout_ms);

    if (real_timeout < 0) {
        audio_cv_.wait(lock, [this] {
            return !audio_queue_.empty() || !is_running_;
        });
    } else {
        audio_cv_.wait_for(lock, std::chrono::milliseconds(real_timeout), [this] {
            return !audio_queue_.empty() || !is_running_;
        });
        /*
        if (audio_queue_.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        */
    }

    if (!is_running_ || audio_queue_.empty()) {
        return nullptr;
    }

    auto audio_data = audio_queue_.front();
    audio_queue_.pop();

    return audio_data;
}