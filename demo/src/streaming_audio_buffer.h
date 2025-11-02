#pragma once

#include "circular_buffer.hpp"
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
    std::condition_variable cv_;
    std::atomic<bool> download_complete_{false};
    std::atomic<size_t> total_written_{0};
    
public:
    explicit StreamingAudioBuffer(size_t capacity);
    
    // Producer: Write data from download thread
    void write(const char* data, size_t size);
    
    // Consumer: Read data for playback thread
    size_t read(uint8_t* data, size_t size);
    
    // Check if we have enough data to start playback
    bool hasEnoughData(size_t min_bytes) const;
    
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
        StreamingAudioBuffer* buffer_;
        std::vector<char> read_buffer_;
        std::streampos current_pos_;
        
    public:
        explicit StreamingStreamBuf(StreamingAudioBuffer* buffer);
        
        std::streampos getCurrentPos() const { return current_pos_; }
        
    protected:
        int_type underflow() override;
        std::streampos seekoff(std::streamoff off, std::ios_base::seekdir way, 
                              std::ios_base::openmode which = std::ios_base::in) override;
        std::streampos seekpos(std::streampos sp, 
                              std::ios_base::openmode which = std::ios_base::in) override;
    };
    
    // Keep an owning shared_ptr option so callers can pass ownership and
    // the streambuf will keep the buffer alive while the stream exists.
    std::shared_ptr<StreamingAudioBuffer> owner_;
    StreamingStreamBuf buf_;

public:
    explicit StreamingInputStream(StreamingAudioBuffer* buffer);
    // Shared-ownership constructor: takes ownership of the buffer to ensure
    // it remains alive while the stream/decoder uses it.
    explicit StreamingInputStream(std::shared_ptr<StreamingAudioBuffer> buffer);
    
    // Get current position in stream (non-virtual, hides base method)
    std::streampos tellg();
    
    // Custom seekg methods for streaming (non-virtual, hides base methods)
    std::istream& seekg(std::streampos pos);
    std::istream& seekg(std::streamoff off, std::ios_base::seekdir way);
};