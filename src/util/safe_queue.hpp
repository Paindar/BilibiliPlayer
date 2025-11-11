#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class SafeQueue {
public:
    SafeQueue() = default;
    ~SafeQueue() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
        cv.notify_all();
    }

    void enqueue(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        cv.notify_one();
    }

    bool dequeue(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = queue_.front();
        queue_.pop();
        return true;
    }

    void waitAndDequeue(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv.wait(lock, [this]() { return !queue_.empty(); });
        item = queue_.front();
        queue_.pop();
    }

    void clean() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
        cv.notify_all();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::condition_variable cv;
    mutable std::mutex mutex_;
    std::queue<T> queue_;
};