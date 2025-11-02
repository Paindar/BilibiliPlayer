#include "streaming_audio_buffer.h"
#include <thread>
#include <chrono>

// StreamingAudioBuffer implementation
StreamingAudioBuffer::StreamingAudioBuffer(size_t capacity) 
    : buffer_(capacity) {}

void StreamingAudioBuffer::write(const char* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < size; ++i) {
        if (!buffer_.write(static_cast<uint8_t>(data[i]))) {
            // Buffer full, wait or implement overflow strategy
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            --i; // Retry this byte
        }
    }
    total_written_ += size;
    cv_.notify_all();
}

size_t StreamingAudioBuffer::read(uint8_t* data, size_t size) {
    std::unique_lock<std::mutex> lock(mutex_);
    size_t bytes_read = 0;
    
    while (bytes_read < size && (!download_complete_ || buffer_.available() > 0)) {
        if (buffer_.available() == 0) {
            if (download_complete_) break;
            cv_.wait_for(lock, std::chrono::milliseconds(10));
            continue;
        }
        
        uint8_t byte;
        if (buffer_.read(byte)) {
            data[bytes_read++] = byte;
        }
    }
    
    return bytes_read;
}

bool StreamingAudioBuffer::hasEnoughData(size_t min_bytes) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.available() >= min_bytes;
}

void StreamingAudioBuffer::setDownloadComplete() {
    std::lock_guard<std::mutex> lock(mutex_);
    download_complete_ = true;
    cv_.notify_all();
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
StreamingInputStream::StreamingStreamBuf::StreamingStreamBuf(StreamingAudioBuffer* buffer) 
    : buffer_(buffer), read_buffer_(8192), current_pos_(0) {}

StreamingInputStream::StreamingStreamBuf::int_type 
StreamingInputStream::StreamingStreamBuf::underflow() {
    if (gptr() == egptr()) {
        size_t bytes_read = buffer_->read(
            reinterpret_cast<uint8_t*>(read_buffer_.data()), 
            read_buffer_.size()
        );
        
        if (bytes_read == 0) {
            // Only return EOF if download is complete AND no data available
            if (buffer_->isDownloadComplete()) {
                return traits_type::eof();
            } else {
                // For streaming, wait a bit and try again
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                bytes_read = buffer_->read(
                    reinterpret_cast<uint8_t*>(read_buffer_.data()), 
                    read_buffer_.size()
                );
                if (bytes_read == 0) {
                    // Still no data, but download not complete - indicate would block
                    return traits_type::eof();
                }
            }
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

StreamingInputStream::StreamingInputStream(StreamingAudioBuffer* buffer) 
    : std::istream(&buf_), owner_(nullptr), buf_(buffer) {}

StreamingInputStream::StreamingInputStream(std::shared_ptr<StreamingAudioBuffer> buffer)
    : std::istream(&buf_), owner_(std::move(buffer)), buf_(owner_.get()) {}

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