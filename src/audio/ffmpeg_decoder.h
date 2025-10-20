#pragma once

#include <memory>
#include <functional>
#include <string>
#include <fstream>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

struct AudioFrame {
    std::vector<uint8_t> data;
    int sample_rate;
    int channels;
    int64_t pts;
    
    AudioFrame(size_t size) : data(size) {}
};

// Audio format information extracted from decoded stream
struct AudioFormat {
    int sample_rate;
    int channels;
    int bits_per_sample;
    
    AudioFormat() : sample_rate(0), channels(0), bits_per_sample(0) {}
    AudioFormat(int sr, int ch, int bps) : sample_rate(sr), channels(ch), bits_per_sample(bps) {}
    
    bool isValid() const { return sample_rate > 0 && channels > 0 && bits_per_sample > 0; }
};

class FFmpegStreamDecoder {
public:
    // Callback for audio output
    using AudioCallback = std::function<void(const uint8_t* data, int size, int sample_rate, int channels)>;
    
    // Constructor/Destructor
    FFmpegStreamDecoder();
    ~FFmpegStreamDecoder();
    
    // Stream input methods
    // Unified interface for all stream types
    bool openStream(std::shared_ptr<std::istream> input_stream);
    
    // Playback control
    bool play();
    bool pause();
    bool stop();
    
    // Audio format information (available after openStream succeeds)
    AudioFormat getAudioFormat() const;
    
    // Stream information
    double getDuration() const;
    std::string getCodecName() const;
    int getSampleRate() const;    // Convenience method
    int getChannels() const;      // Convenience method
    int getBitsPerSample() const; // Convenience method
    
    // Position control
    bool seek(double position_seconds);
    double getCurrentPosition() const;
    
    // Audio settings
    void setVolume(float volume); // 0.0 to 1.0
    float getVolume() const;
    
    // Set callback for audio frames
    void setAudioCallback(AudioCallback callback);
    
    // Set stream's expected size (for progress tracking)
    void setStreamExpectedSize(uint64_t size);

    // Status
    bool isPlaying() const;
    bool isPaused() const;
    bool isEOF() const;

private:
    // FFmpeg components
    AVFormatContext* format_ctx_;
    AVCodecContext* codec_ctx_;
    const AVCodec* codec_;
    SwrContext* swr_ctx_;
    int audio_stream_index_;
    
    // Playback state
    std::atomic<bool> playing_;
    std::atomic<bool> paused_;
    std::atomic<bool> eof_;
    std::atomic<bool> should_stop_;
    std::atomic<float> volume_;
    
    // Audio format
    AudioFormat audio_format_;
    
    // Threading
    std::thread decode_thread_;
    std::thread playback_thread_;
    std::thread stream_reader_thread_;
    
    // Frame queue
    std::queue<std::shared_ptr<AudioFrame>> frame_queue_;
    std::mutex frame_mutex_;
    std::condition_variable frame_cv_;
    static const size_t MAX_QUEUE_SIZE = 30;
    
    // Audio callback
    AudioCallback audio_callback_;
    
    // Stream reading
    std::shared_ptr<std::istream> input_stream_;
    uint64_t stream_expect_size_;
    std::vector<uint8_t> stream_buffer_;
    std::atomic<size_t> buffer_read_pos_;
    std::atomic<bool> stream_eof_;
    std::atomic<bool> stop_stream_reader_;
    
    // Custom AVIO
    AVIOContext* avio_ctx_;
    uint8_t* avio_buffer_;
    static const size_t AVIO_BUFFER_SIZE = 4096;
    
    // Private methods
    bool setupCustomIO();
    bool initializeCodec();
    void cleanupFFmpeg();
    void decodeLoop();
    void playbackLoop();
    void streamReaderLoop();
    std::shared_ptr<AudioFrame> decodeFrame(AVFrame* frame);
    void applyVolume(uint8_t* data, int size, int channels, float volume);
    
    // Static callback for AVIO
    static int readPacket(void* opaque, uint8_t* buf, int buf_size);
    static int64_t seek(void* opaque, int64_t offset, int whence);
};