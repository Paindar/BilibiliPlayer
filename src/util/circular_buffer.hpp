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
    
    // Alias for push to match expected interface
    bool write(const T& item) {
        if (isFull()) {
            return false;
        }
        push(item);
        return true;
    }

    // Bulk write: write up to count elements from data, return number written
    size_t write(const T* data, size_t count) {
        size_t written = 0;
        while (written < count && !isFull()) {
            buffer_[head_] = data[written];
            head_ = (head_ + 1) % capacity_;
            if (size_ < capacity_) ++size_;
            ++written;
        }
        return written;
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
    
    // Alias for pop to match expected interface
    bool read(T& item) {
        if (isEmpty()) {
            return false;
        }
        item = pop();
        return true;
    }

    // Bulk read: read up to count elements into out, return number read
    size_t read(T* out, size_t count) {
        size_t readCount = 0;
        while (readCount < count && !isEmpty()) {
            size_t tail = (head_ + capacity_ - size_) % capacity_;
            out[readCount] = buffer_[tail];
            // Pop element
            --size_;
            ++readCount;
        }
        return readCount;
    }

    size_t size() const {
        return size_;
    }
    
    // Alias for size to match expected interface
    size_t available() const {
        return size_;
    }

    size_t capacity() const { return capacity_; }

    bool isEmpty() const {
        return size_ == 0;
    }

    bool isFull() const {
        return size_ == capacity_;
    }
    
    void clear() {
        head_ = 0;
        size_ = 0;
    }
private:
    size_t capacity_;
    std::vector<T> buffer_;
    size_t head_;
    size_t size_;
};