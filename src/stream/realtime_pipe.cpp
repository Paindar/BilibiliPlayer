#include "realtime_pipe.hpp"
#include <algorithm>

// ---------------- PipeBuffer ----------------

RealtimePipe::PipeBuffer::PipeBuffer(size_t capacity)
    : buffer_(capacity) {}

void RealtimePipe::PipeBuffer::closeInput() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        input_closed_ = true;
    }
    space_available_.notify_all();
}

void RealtimePipe::PipeBuffer::closeOutput() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        output_closed_ = true;
    }
    data_available_.notify_all();
}

size_t RealtimePipe::PipeBuffer::available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.size();
}

std::streamsize RealtimePipe::PipeBuffer::xsputn(const char* s, std::streamsize n) {
    std::unique_lock<std::mutex> lock(mutex_);
    std::streamsize written = 0;

    while (written < n) {
        while (buffer_.full() && !input_closed_) space_available_.wait(lock);
        if (input_closed_) break;  // reader gone
        buffer_.push(s[written++]);
        data_available_.notify_one();
    }
    return written;
}

std::streamsize RealtimePipe::PipeBuffer::xsgetn(char* s, std::streamsize n) {
    std::streamsize total = 0;
    for (; total < n; ++total) {
        int c = underflow();
        if (c == traits_type::eof()) break;
        s[total] = traits_type::to_char_type(c);
    }
    return total;
}

std::streambuf::int_type RealtimePipe::PipeBuffer::underflow() {
    std::unique_lock<std::mutex> lock(mutex_);
    data_available_.wait(lock, [&] {
        return !buffer_.empty() || output_closed_;
    });

    if (buffer_.empty() && output_closed_) {
        return traits_type::eof();
    }

    current_char_ = buffer_.pop();
    space_available_.notify_one();
    setg(&current_char_, &current_char_, &current_char_ + 1);
    return traits_type::to_int_type(current_char_);
}

// ---------------- RealtimePipe ----------------

RealtimePipe::RealtimePipe(size_t capacity)
    : buffer_(std::make_shared<PipeBuffer>(capacity)) {}

std::shared_ptr<std::istream> RealtimePipe::getInputStream() {
    return std::make_shared<InputStream>(buffer_);
}

std::shared_ptr<std::ostream> RealtimePipe::getOutputStream() {
    return std::make_shared<OutputStream>(buffer_);
}

void RealtimePipe::close() {
    buffer_->closeInput();   // wake writers
    buffer_->closeOutput();  // signal EOF to readers
}

void RealtimePipe::closeInput() {
    buffer_->closeInput();
}

void RealtimePipe::closeOutput() {
    buffer_->closeOutput();
}

size_t RealtimePipe::available() const {
    return buffer_->available();
}
