#pragma once

#include <vector>
#include <cstddef>
#include <stdexcept>
#include <algorithm>

namespace util {
/**
 * @brief Simple circular buffer (non-thread-safe)
 *
 * @tparam T Type of elements stored
 */
template <typename T>
class CircularBuffer {
public:
    explicit CircularBuffer(size_t capacity)
        : buffer_(capacity), capacity_(capacity), read_pos_(0), write_pos_(0), full_(false) {
        if (capacity == 0) throw std::invalid_argument("CircularBuffer capacity must be > 0");
    }
    
    void push(const T& item) {
        buffer_[write_pos_] = item;
        write_pos_ = (write_pos_ + 1) % capacity_;
        if (full_) {
            read_pos_ = (read_pos_ + 1) % capacity_; // overwrite oldest
        }
        full_ = write_pos_ == read_pos_;
    }

    T pop() {
        if (empty()) throw std::runtime_error("CircularBuffer is empty");
        T item = buffer_[read_pos_];
        read_pos_ = (read_pos_ + 1) % capacity_;
        full_ = false;
        return item;
    }

    void write(const T* data, size_t count) {
        size_t remaining = count;
        while (remaining > 0) {
            // Max contiguous space until end of buffer
            size_t space = full_ ? 0 :
                        (write_pos_ >= read_pos_ ? capacity_ - write_pos_ : read_pos_ - write_pos_);
            
            if (full_ || space == 0) {
                // Buffer full, overwrite oldest element (like push)
                buffer_[write_pos_] = *data;
                write_pos_ = (write_pos_ + 1) % capacity_;
                read_pos_ = write_pos_;  // move read_pos to next oldest
                full_ = true;
                ++data;
                --remaining;
                continue;
            }

            // Copy a contiguous chunk
            size_t chunk = std::min(space, remaining);
            std::copy(data, data + chunk, buffer_.begin() + write_pos_);
            write_pos_ = (write_pos_ + chunk) % capacity_;
            full_ = write_pos_ == read_pos_;
            data += chunk;
            remaining -= chunk;
        }
    }

    size_t read(T* data, size_t count) {
        size_t totalRead = 0;
        while (totalRead < count && !empty()) {
            // Max contiguous data until end of buffer
            size_t available = write_pos_ > read_pos_ ? write_pos_ - read_pos_ :
                            (full_ ? capacity_ - read_pos_ : capacity_ - read_pos_);
            size_t chunk = std::min(count - totalRead, available);

            std::copy(buffer_.begin() + read_pos_, buffer_.begin() + read_pos_ + chunk, data + totalRead);

            read_pos_ = (read_pos_ + chunk) % capacity_;
            full_ = false;
            totalRead += chunk;
        }
        return totalRead;
    }


    bool empty() const { return !full_ && (read_pos_ == write_pos_); }
    bool full() const { return full_; }
    size_t size() const {
        if (full_) return capacity_;
        if (write_pos_ >= read_pos_) return write_pos_ - read_pos_;
        return capacity_ - read_pos_ + write_pos_;
    }

    size_t capacity() const { return capacity_; }

private:
    std::vector<T> buffer_;
    size_t capacity_;
    size_t read_pos_;
    size_t write_pos_;
    bool full_;
};
}