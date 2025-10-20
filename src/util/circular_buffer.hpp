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

    size_t size() const {
        return size_;
    }
    
    // Alias for size to match expected interface
    size_t available() const {
        return size_;
    }

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