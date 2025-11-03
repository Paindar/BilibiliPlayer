#pragma once

#include "util/circular_buffer.hpp"
#include <istream>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

// Streaming buffer for real-time audio playback
class StreamingAudioBuffer {
private:
    CircularBuffer<uint8_t> buffer_;
    mutable std::mutex mutex_;
    // Two condition variables: one for writers (not full) and one for readers (not empty)
    std::condition_variable cv_not_full_;
    std::condition_variable cv_not_empty_;
    std::atomic<bool> download_complete_{false};
    std::atomic<size_t> total_written_{0};
    std::atomic<bool> destroyed_{false};
    
public:
    explicit StreamingAudioBuffer(size_t capacity);
    ~StreamingAudioBuffer();
    // Non-copyable
    StreamingAudioBuffer(const StreamingAudioBuffer&) = delete;
    StreamingAudioBuffer& operator=(const StreamingAudioBuffer&) = delete;
    
    // Producer: Write data from download thread
    // Legacy API: blocks until all bytes are written or buffer destroyed
    void write(const char* data, size_t size);
    // (No cancellable overload) write blocks until bytes written or destroyed

    // Consumer: Read data for playback thread
    // Legacy API: blocks until requested bytes are read, EOF, or buffer destroyed
    size_t read(uint8_t* data, size_t size);
    // (No cancellable overload) read blocks until bytes are available, EOF, or destroyed
    void destroy();
    
    // Check if we have enough data to start playback
    bool hasEnoughData(size_t min_bytes) const;
    // Wait until at least min_bytes are available or timeout_ms elapses
    bool waitForEnoughData(size_t min_bytes, int timeout_ms);
    
    void setDownloadComplete();
    
    size_t available() const;
    size_t totalWritten() const;
    bool isDownloadComplete() const;
    void clear();
};

// Custom input stream for FFmpeg that reads from streaming buffer
class StreamingInputStream : public std::istream {
private:
    class StreamingStreamBuf : public std::streambuf {
    private:
        std::shared_ptr<StreamingAudioBuffer> buffer_;
        std::vector<char> read_buffer_;
        std::streampos current_pos_;
        
    public:
        explicit StreamingStreamBuf(std::shared_ptr<StreamingAudioBuffer> buffer);
        
        std::streampos getCurrentPos() const { return current_pos_; }
        // Expose buffer state helpers
        bool hasEnoughData(size_t min_bytes) const { return buffer_->hasEnoughData(min_bytes); }
        size_t available() const { return buffer_->available(); }
        bool isDownloadComplete() const { return buffer_->isDownloadComplete(); }
        bool waitForEnoughData(size_t min_bytes, int timeout_ms) { return buffer_->waitForEnoughData(min_bytes, timeout_ms); }
        // Destroy underlying buffer to wake any blocked readers/writers
        void destroyBuffer();
        
    protected:
        int_type underflow() override;
        std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way, 
                              std::ios_base::openmode which = std::ios_base::in) override;
        std::streampos seekpos(std::streampos sp, 
                              std::ios_base::openmode which = std::ios_base::in) override;
    };
    
    std::shared_ptr<StreamingStreamBuf> buf_;

public:
    explicit StreamingInputStream(std::shared_ptr<StreamingAudioBuffer> buffer);
    
    // Get current position in stream (non-virtual, hides base method)
    std::streampos tellg();
    
    // Custom seekg methods for streaming (non-virtual, hides base methods)
    std::istream& seekg(std::streampos pos);
    std::istream& seekg(std::streamoff off, std::ios_base::seekdir way);

    // Convenience wrappers for buffering state
    bool hasEnoughData(size_t min_bytes) const { return buf_->hasEnoughData(min_bytes); }
    size_t available() const { return buf_->available(); }
    bool isDownloadComplete() const { return buf_->isDownloadComplete(); }
    bool waitForEnoughData(size_t min_bytes, int timeout_ms) { return buf_->waitForEnoughData(min_bytes, timeout_ms); }
    // Destroy underlying buffer to wake any blocked readers/writers.
    void destroyBuffer();
};