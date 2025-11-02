#include "streaming_audio_buffer.h"
#include <thread>
#include <chrono>

// StreamingAudioBuffer implementation
StreamingAudioBuffer::StreamingAudioBuffer(size_t capacity) 
    : buffer_(capacity) {}


StreamingAudioBuffer::~StreamingAudioBuffer()
{
    destroy();
}


void StreamingAudioBuffer::write(const char* data, size_t size) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (destroyed_.load()) return;
    size_t written = 0;
    while (written < size) {
        // predicate: space available OR download complete OR destroyed
        cv_not_full_.wait(lock, [this]() {
            return !buffer_.isFull() || download_complete_ || destroyed_.load();
        });

        if (destroyed_.load()) break;

        // Write as many bytes as possible in a block
        if (!destroyed_.load()) {
            size_t space = buffer_.isFull() ? 0 : (buffer_.capacity() - buffer_.available());
            // convert to bytes we can write
            size_t to_write = std::min(size - written, space);
            if (to_write > 0) {
                // CircularBuffer stores uint8_t, so we can write raw bytes
                size_t actual = buffer_.write(reinterpret_cast<const uint8_t*>(data + written), to_write);
                written += actual;
            }
        }

        // Notify readers that data is available
        lock.unlock();
        cv_not_empty_.notify_all();
        lock.lock();
    }

    total_written_ += written;
}

size_t StreamingAudioBuffer::read(uint8_t* data, size_t size) {
    // Non-cancellable read implementation (blocks until data available, EOF, or destroyed)
    std::unique_lock<std::mutex> lock(mutex_);
    size_t bytes_read = 0;

    auto pred = [this]() {
        return buffer_.available() > 0 || download_complete_ || destroyed_.load();
    };

    while (bytes_read < size) {
        cv_not_empty_.wait(lock, pred);

        if (destroyed_.load()) break;

        // Drain available bytes in a block
        if (buffer_.available() > 0) {
            size_t can_read = std::min(size - bytes_read, buffer_.available());
            size_t got = buffer_.read(data + bytes_read, can_read);
            bytes_read += got;
        }

        // Notify potential writers that space is available
        lock.unlock();
        cv_not_full_.notify_all();
        lock.lock();

        // If no bytes read and download complete, break
        if (bytes_read == 0 && download_complete_) break;
    }

    return bytes_read;
}

bool StreamingAudioBuffer::hasEnoughData(size_t min_bytes) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.available() >= min_bytes;
}

void StreamingAudioBuffer::destroy()
{
    std::lock_guard<std::mutex> lock(mutex_);
    destroyed_ = true;
    // Wake both readers and writers so they can exit
    cv_not_empty_.notify_all();
    cv_not_full_.notify_all();
}

bool StreamingAudioBuffer::waitForEnoughData(size_t min_bytes, int timeout_ms)
{
    std::unique_lock<std::mutex> lock(mutex_);
    auto pred = [this, min_bytes]() { return buffer_.available() >= min_bytes || download_complete_; };
    if (timeout_ms < 0) {
        cv_not_empty_.wait(lock, pred);
        return buffer_.available() >= min_bytes;
    } else {
        if (cv_not_empty_.wait_for(lock, std::chrono::milliseconds(timeout_ms), pred)) {
            return buffer_.available() >= min_bytes;
        }
        return buffer_.available() >= min_bytes; // timeout, return whether condition met
    }
}

void StreamingAudioBuffer::setDownloadComplete() {
    std::lock_guard<std::mutex> lock(mutex_);
    download_complete_ = true;
    // Wake both readers and writers so they can exit or finish
    cv_not_empty_.notify_all();
    cv_not_full_.notify_all();
}

size_t StreamingAudioBuffer::available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.available();
}

size_t StreamingAudioBuffer::totalWritten() const { 
    return total_written_; 
}

bool StreamingAudioBuffer::isDownloadComplete() const { 
    return download_complete_; 
}

void StreamingAudioBuffer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
    download_complete_ = false;
    total_written_ = 0;
}
// StreamingInputStream implementation
StreamingInputStream::StreamingStreamBuf::StreamingStreamBuf(std::shared_ptr<StreamingAudioBuffer> buffer) 
    : buffer_(buffer), read_buffer_(8192), current_pos_(0) {}

StreamingInputStream::StreamingStreamBuf::int_type 
StreamingInputStream::StreamingStreamBuf::underflow() {
    if (gptr() == egptr()) {
        // read() will block until data is available or download completes
        size_t bytes_read = buffer_->read(
            reinterpret_cast<uint8_t*>(read_buffer_.data()),
            read_buffer_.size()
        );

        if (bytes_read == 0) {
            // No data and download complete -> EOF
            return traits_type::eof();
        }

        setg(read_buffer_.data(), read_buffer_.data(),
             read_buffer_.data() + bytes_read);
        current_pos_ += bytes_read;
    }
    
    return traits_type::to_int_type(*gptr());
}

std::streampos StreamingInputStream::StreamingStreamBuf::seekoff(
    std::streamoff off, std::ios_base::seekdir way, std::ios_base::openmode which) {
    // For streaming, seeking is not supported
    // Return current position for SEEK_CUR with offset 0, otherwise fail
    if (way == std::ios_base::cur && off == 0) {
        return current_pos_;
    }
    return std::streampos(-1); // Indicate seek failure
}

std::streampos StreamingInputStream::StreamingStreamBuf::seekpos(
    std::streampos sp, std::ios_base::openmode which) {
    // Seeking to specific position not supported in streaming
    return std::streampos(-1);
}

StreamingInputStream::StreamingInputStream(std::shared_ptr<StreamingAudioBuffer> buffer)
    : std::istream(nullptr), buf_(buffer)
{
    // rdbuf must be set after buf_ has been constructed (base classes are
    // initialized before members), so set it here.
    rdbuf(&buf_);
}

std::streampos StreamingInputStream::tellg() {
    // For streaming, we don't know the total size
    // Return current read position in the buffer
    return buf_.getCurrentPos();
}

std::istream& StreamingInputStream::seekg(std::streampos pos) {
    // Seeking not supported in streaming mode
    setstate(std::ios_base::failbit);
    return *this;
}

std::istream& StreamingInputStream::seekg(std::streamoff off, std::ios_base::seekdir way) {
    // Only support telling current position (seekg(0, cur))
    if (way == std::ios_base::cur && off == 0) {
        // This is just a tellg() operation, don't fail
        return *this;
    }
    // Other seeks not supported in streaming
    setstate(std::ios_base::failbit);
    return *this;
}