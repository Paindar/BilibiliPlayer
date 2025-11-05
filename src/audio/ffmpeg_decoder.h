#pragma once

#include <memory>
#include <functional>
#include <string>
#include <iostream>
#include <future>
#include <vector>
#include <util/circular_buffer.hpp>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <QObject>
#include <queue>

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
    
    constexpr bool isValid() const { return sample_rate > 0 && channels > 0 && bits_per_sample > 0; }
};

struct AudioFrameQueue {
    std::mutex mutex;
    std::condition_variable cond_var;
    std::queue<std::shared_ptr<AudioFrame>> frame_queue;
};

/**
 * FFmpeg-based audio stream decoder, using custom AVIO context for stream reading.
 * Supports decoding from an input stream and provides (if needed) decoded audio frames.
 * Provides asynchronous methods for retrieving audio format and codec name.
 * 
 */
class FFmpegStreamDecoder : public QObject{
    Q_OBJECT
public:
    explicit FFmpegStreamDecoder();
    ~FFmpegStreamDecoder();
    
    /**
     * Initialize the decoder with an input stream, can't be initialized more than once.
     * @param inputAudioStream Input stream to read encoded audio data from
     * @param outputFrameStream Output stream to write decoded audio frames to (can be nullptr if not needed)
     * @return true on success, false on failure
     */
    bool initialize(std::shared_ptr<std::istream> inputAudioStream, 
        std::shared_ptr<AudioFrameQueue> outputFrameQueue);
    
    /**
     * Start decoding process.
     * @param lenFullStream Full length of the input stream in bytes, deciding how much data would be decoded
     * @return true on success, false on failure
     */
    bool startDecoding(uint64_t lenFullStream);
    bool pauseDecoding();
    bool resumeDecoding();
    bool isDecoding() const;
    bool isCompleted() const;

    /**
     * Try to get audio format information asynchronously.
     * If decoder is not yet initialized, future ends with invalid AudioFormat(see AudioFormat::isValid).
     * @return Future that will hold AudioFormat once available
     */
    std::future<AudioFormat> getAudioFormatAsync() const;
    // std::future<std::string> getCodecNameAsync() const;
    
    std::future<double> getDurationAsync() const;
    
    // Todo seek specified position in seconds
    // bool seek(double position_seconds);
    // double getCurrentPosition() const;

signals:
protected:
    /** 
     * Custom seek function for AVIO, only support AVSEEK_SIZE and
     *  return the value that startDecoding gets, no matter whether
     * inputAudioStream support seeking.
    */
    static int64_t seek(void* opaque, int64_t offset, int whence);
    static int readPacket(void *opaque, uint8_t *buf, int buf_size);

private:
    // Setup custom AVIO context for FFmpeg to read from inputAudioStream
    bool setupCustomIO();
    // Read from inputAudioStream and fill internal buffer
    void consumeAudioStreamFunc();
    // Decode audio bytes from internal buffer and push to outputFrameQueue
    void decodeAudioBytesFunc();
    std::shared_ptr<AudioFrame> decodeFrame(AVFrame* frame);
private:
    // Used for getDurationAsync and getCodecNameAsync
    mutable std::mutex ffmpegMutex_;
    mutable std::condition_variable ffmpegCondition_;
    AVFormatContext* format_ctx_;
    AVCodecContext* codec_ctx_;
    const AVCodec* codec_;
    SwrContext* swr_ctx_;
    int audioStreamIndex; // the name of audio_stream_index has been rename with audioStreamIndex
    
    // Used to wake up waiting getAudioFormatAsync calls
    mutable std::mutex audioFormatMutex_;
    mutable std::condition_variable audioFormatCondition_;
    AudioFormat audio_format_;
    
    // Custom AVIO
    AVIOContext* avio_ctx_;
    uint8_t* avio_buffer_;
    static const size_t AVIO_BUFFER_SIZE = 4096;

    bool hasInit;
    uint64_t lenFullSize;
    mutable std::condition_variable decodeCondition;
    mutable std::mutex audioCacheMutex;
    std::shared_ptr<std::istream> inputAudioStream;
    std::shared_ptr<AudioFrameQueue> outputFrameQueue;
    util::CircularBuffer<uint8_t> audioCache;

    std::atomic<bool> playing_;
    std::atomic<bool> destroying_;
    std::atomic<bool> decodeCompleted_;
    std::atomic<bool> streamConsumptionFinished_;

    // Used to notify the waiting decodeThread.
    mutable std::mutex decodeStateMutex;
    mutable std::condition_variable decodeStateCondition;
    std::thread consumeAudioStreamThread;
    std::thread decodeAudioBytesThread;
};