#pragma once

#include <vector>
#include <atomic>

template<typename T>
class CircularBuffer {
private:
    std::vector<T> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
    std::atomic<size_t> size_{0};
    const size_t capacity_;

public:
    explicit CircularBuffer(size_t capacity) 
        : buffer_(capacity), capacity_(capacity) {}

    // Write a single element (producer)
    bool write(const T& item) {
        size_t current_size = size_.load();
        if (current_size >= capacity_) {
            return false; // Buffer full
        }

        size_t current_tail = tail_.load();
        buffer_[current_tail] = item;
        
        tail_.store((current_tail + 1) % capacity_);
        size_.fetch_add(1);
        
        return true;
    }

    // Read a single element (consumer)
    bool read(T& item) {
        size_t current_size = size_.load();
        if (current_size == 0) {
            return false; // Buffer empty
        }

        size_t current_head = head_.load();
        item = buffer_[current_head];
        
        head_.store((current_head + 1) % capacity_);
        size_.fetch_sub(1);
        
        return true;
    }

    // Get number of available elements
    size_t available() const {
        return size_.load();
    }

    // Get buffer capacity
    size_t capacity() const {
        return capacity_;
    }

    // Check if buffer is empty
    bool empty() const {
        return size_.load() == 0;
    }

    // Check if buffer is full
    bool full() const {
        return size_.load() >= capacity_;
    }

    // Clear the buffer
    void clear() {
        head_.store(0);
        tail_.store(0);
        size_.store(0);
    }
};