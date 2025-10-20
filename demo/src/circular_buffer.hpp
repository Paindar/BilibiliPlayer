#pragma once

#include <vector>
#include <stdexcept>

template <typename T>
class CircularBuffer {
public:
    explicit CircularBuffer(size_t capacity)
        : capacity_(capacity), buffer_(capacity), head_(0), size_(0) {}

    void push(const T& item) {
        buffer_[head_] = item;
        head_ = (head_ + 1) % capacity_;
        if (size_ < capacity_) {
            ++size_;
        }
    }

    T pop() {
        if (size_ == 0) {
            throw std::out_of_range("Buffer is empty");
        }
        size_t tail = (head_ + capacity_ - size_) % capacity_;
        T item = buffer_[tail];
        --size_;
        return item;
    }

    size_t size() const {
        return size_;
    }

    bool isEmpty() const {
        return size_ == 0;
    }

    bool isFull() const {
        return size_ == capacity_;
    }
private:
    size_t capacity_;
    std::vector<T> buffer_;
    size_t head_;
    size_t size_;
};