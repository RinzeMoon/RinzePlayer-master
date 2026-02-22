//
// Created by lsy on 2026/1/18.
//

#pragma once

#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <utility>  // std::move/std::forward
#include <cassert>  // assert

template<typename T>
class RingBuffer {
private:
    // ========== 核心修改：存储unique_ptr ==========
    std::vector<std::unique_ptr<T>> buffer_;
    size_t capacity_;
    size_t head_ = 0;  // 写位置
    size_t tail_ = 0;  // 读位置
    size_t count_ = 0;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::atomic<bool> stop_ = false; // 停止标志

public:
    // ========== 极简构造函数：只分配指针数组 ==========
    RingBuffer(size_t capacity) : capacity_(capacity) {
        // 只创建指针数组，不创建实际对象
        buffer_.resize(capacity_);

        // 初始化所有指针为nullptr
        for (size_t i = 0; i < capacity_; ++i) {
            buffer_[i] = nullptr;
        }
    }

    // ========== 拷贝push：兼容原有接口 ==========
    bool push(const T& item, int timeout_ms = 10) {
        return emplace(timeout_ms, item);
    }

    // ========== 移动push：高效转移所有权 ==========
    bool push(T&& item, int timeout_ms = 10) {
        return emplace(timeout_ms, std::move(item));
    }

    // ========== 非阻塞try_push ==========
    bool try_push(const T& item) {
        return emplace(0, item);
    }

    // ========== 核心emplace方法 ==========
    template<typename... Args>
    bool emplace(int timeout_ms = 10, Args&&... args) {
        std::unique_lock<std::mutex> lock(mutex_);

        // 等待缓冲区未满
        if (count_ >= capacity_) {
            if (timeout_ms == 0) return false;

            // 定义等待条件：有空间或需要停止
            auto wait_condition = [this]() { return count_ < capacity_ || needStop(); };

            if (timeout_ms < 0) {
                // 无限等待
                not_full_.wait(lock, wait_condition);
            } else {
                // 有限等待
                if (!not_full_.wait_for(lock, std::chrono::milliseconds(timeout_ms), wait_condition)) {
                    return false; // 超时
                }
            }

            // 如果因为 needStop() 唤醒但队列仍满，则返回 false
            if (count_ >= capacity_) {
                return false;
            }
        }

        // 如果当前位置已有对象，先释放
        if (buffer_[head_]) {
            buffer_[head_].reset();
        }

        // 创建新对象（移动构造或拷贝构造）
        buffer_[head_] = std::make_unique<T>(std::forward<Args>(args)...);

        // 更新指针和计数
        head_ = (head_ + 1) % capacity_;
        count_++;

        not_empty_.notify_one();
        return true;
    }
    // ========== 返回unique_ptr的pop（推荐使用） ==========
    std::unique_ptr<T> pop_ptr(int timeout_ms = 10) {
        std::unique_lock<std::mutex> lock(mutex_);

        // ========== 新增：等待条件（含停止判断） ==========
        auto waitCondition = [this]() { return count_ > 0 || needStop(); };

        if (count_ == 0) {
            if (timeout_ms == 0) return nullptr;
            // ========== 新增：支持timeout_ms=-1 无限等待 ==========
            if (timeout_ms == -1) {
                not_empty_.wait(lock, waitCondition);
            } else {
                if (!not_empty_.wait_for(lock, std::chrono::milliseconds(timeout_ms), waitCondition)) {
                    return nullptr;
                }
            }
        }

        // ========== 新增：停止判断 ==========
        if (needStop() && count_ == 0) {
            return nullptr;
        }

        // ========== 关键优化：转移指针所有权 ==========
        std::unique_ptr<T> result = std::move(buffer_[tail_]);

        // 注意：buffer_[tail_]现在已经是nullptr
        // 我们不需要手动释放或重建任何东西

        tail_ = (tail_ + 1) % capacity_;
        count_--;

        not_full_.notify_one();
        return result;
    }

    // ========== 兼容原有接口的pop ==========
    bool pop(T& item, int timeout_ms = 10) {
        auto ptr = pop_ptr(timeout_ms);
        if (!ptr) return false;

        // 移动对象内容到参数
        item = std::move(*ptr);
        return true;
    }

    // ========== 批量弹出多个对象 ==========
    std::vector<std::unique_ptr<T>> pop_batch(size_t max_count, int timeout_ms = 10) {
        std::vector<std::unique_ptr<T>> result;
        std::unique_lock<std::mutex> lock(mutex_);

        // 等待至少有一个元素
        auto waitCondition = [this]() { return count_ > 0 || needStop(); };

        if (count_ == 0) {
            if (timeout_ms == 0) return result;
            if (timeout_ms == -1) {
                not_empty_.wait(lock, waitCondition);
            } else {
                if (!not_empty_.wait_for(lock, std::chrono::milliseconds(timeout_ms), waitCondition)) {
                    return result;
                }
            }
        }

        if (needStop() && count_ == 0) {
            return result;
        }

        // 批量弹出（最多max_count个）
        size_t actual_count = std::min(max_count, count_);
        result.reserve(actual_count);

        for (size_t i = 0; i < actual_count; ++i) {
            result.push_back(std::move(buffer_[tail_]));
            tail_ = (tail_ + 1) % capacity_;
            count_--;
        }

        if (actual_count > 0) {
            not_full_.notify_all();
        }

        return result;
    }

    // ========== 批量插入多个对象 ==========
    template<typename InputIt>
    size_t push_batch(InputIt first, InputIt last, int timeout_ms = 10) {
        std::unique_lock<std::mutex> lock(mutex_);
        size_t inserted = 0;

        // 计算可插入数量
        size_t available = capacity_ - count_;
        size_t to_insert = std::distance(first, last);
        size_t actual_insert = std::min(available, to_insert);

        // 如果没有空间且需要等待
        if (actual_insert == 0 && timeout_ms != 0) {
            // 等待至少有一个空位
            auto waitCondition = [this]() { return count_ < capacity_ || needStop(); };

            if (timeout_ms == -1) {
                not_full_.wait(lock, waitCondition);
            } else {
                if (!not_full_.wait_for(lock, std::chrono::milliseconds(timeout_ms), waitCondition)) {
                    return 0;
                }
            }

            if (needStop()) return 0;

            // 重新计算可插入数量
            available = capacity_ - count_;
            actual_insert = std::min(available, to_insert);
        }

        // 批量插入
        for (size_t i = 0; i < actual_insert; ++i) {
            if (buffer_[head_]) {
                buffer_[head_].reset();
            }

            buffer_[head_] = std::make_unique<T>(*first);
            ++first;
            ++inserted;

            head_ = (head_ + 1) % capacity_;
            count_++;
        }

        if (inserted > 0) {
            not_empty_.notify_all();
        }

        return inserted;
    }

    // ========== 查看但不弹出（peek） ==========
    std::unique_ptr<T> peek(int timeout_ms = 10) {
        std::unique_lock<std::mutex> lock(mutex_);

        auto waitCondition = [this]() { return count_ > 0 || needStop(); };

        if (count_ == 0) {
            if (timeout_ms == 0) return nullptr;
            if (timeout_ms == -1) {
                not_empty_.wait(lock, waitCondition);
            } else {
                if (!not_empty_.wait_for(lock, std::chrono::milliseconds(timeout_ms), waitCondition)) {
                    return nullptr;
                }
            }
        }

        if (needStop() && count_ == 0) {
            return nullptr;
        }

        // 创建对象的副本
        if (!buffer_[tail_]) return nullptr;
        return std::make_unique<T>(*buffer_[tail_]);
    }

    // ========== 辅助方法 ==========
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    size_t capacity() const {
        return capacity_;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ == 0;
    }

    bool full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ == capacity_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);

        // 释放所有指针
        for (size_t i = 0; i < capacity_; ++i) {
            buffer_[i].reset();
        }

        head_ = tail_ = count_ = 0;
        not_full_.notify_all();
    }

    bool needStop() const {
        return stop_.load(std::memory_order_acquire);
    }

    void stop() {
        stop_.store(true, std::memory_order_release);
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_.store(false, std::memory_order_release);
        clear();
    }

    // ========== 直接访问方法（谨慎使用） ==========
    const T* peek_raw() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ == 0) return nullptr;
        return buffer_[tail_].get();
    }

    T* peek_raw() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ == 0) return nullptr;
        return buffer_[tail_].get();
    }

    // ========== 获取统计信息 ==========
    struct Stats {
        size_t capacity;
        size_t size;
        size_t head;
        size_t tail;
        size_t null_count;  // 空指针数量
    };

    Stats get_stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        Stats stats;
        stats.capacity = capacity_;
        stats.size = count_;
        stats.head = head_;
        stats.tail = tail_;

        // 统计空指针数量
        stats.null_count = 0;
        for (const auto& ptr : buffer_) {
            if (!ptr) stats.null_count++;
        }

        return stats;
    }

    // ========== 调试方法 ==========
    bool validate() const {
        std::lock_guard<std::mutex> lock(mutex_);

        // 验证计数与指针状态一致
        size_t actual_count = 0;
        for (size_t i = 0; i < capacity_; ++i) {
            if (buffer_[i]) actual_count++;
        }

        if (actual_count != count_) {
            return false;
        }

        // 验证指针位置
        if (count_ > 0) {
            if (!buffer_[tail_]) return false;
        }

        return true;
    }

    // ========== 预留额外容量（动态扩容） ==========
    bool reserve(size_t new_capacity) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (new_capacity <= capacity_) {
            return false;  // 只能扩容，不能缩容
        }

        // 创建新缓冲区
        std::vector<std::unique_ptr<T>> new_buffer(new_capacity);

        // 复制现有元素（保持顺序）
        for (size_t i = 0; i < count_; ++i) {
            size_t src_idx = (tail_ + i) % capacity_;
            size_t dst_idx = i;  // 在新缓冲区中从0开始排列

            if (buffer_[src_idx]) {
                new_buffer[dst_idx] = std::move(buffer_[src_idx]);
            }
        }

        // 更新状态
        buffer_ = std::move(new_buffer);
        capacity_ = new_capacity;
        head_ = count_;  // 写位置在最后一个元素之后
        tail_ = 0;       // 读位置在开头

        not_full_.notify_all();
        return true;
    }

    // ========== 迭代器支持（只读） ==========
    class const_iterator {
    private:
        const RingBuffer* buffer_;
        size_t index_;
        size_t count_;

    public:
        const_iterator(const RingBuffer* buffer, size_t index, size_t count)
            : buffer_(buffer), index_(index), count_(count) {}

        bool operator!=(const const_iterator& other) const {
            return index_ != other.index_;
        }

        const_iterator& operator++() {
            index_++;
            return *this;
        }

        const T& operator*() const {
            size_t buffer_index = (buffer_->tail_ + index_) % buffer_->capacity_;
            return *buffer_->buffer_[buffer_index];
        }
    };

    const_iterator begin() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return const_iterator(this, 0, count_);
    }

    const_iterator end() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return const_iterator(this, count_, count_);
    }
};