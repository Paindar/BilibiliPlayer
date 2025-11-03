#include "ffmpeg_decoder.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <thread>
#include <log/log_manager.h>
#include "../stream/streaming_audio_buffer.h"

FFmpegStreamDecoder::FFmpegStreamDecoder()
    : format_ctx_(nullptr)
    , codec_ctx_(nullptr)
    , codec_(nullptr)
    , swr_ctx_(nullptr)
    , audio_stream_index_(-1)
    , playing_(false)
    , paused_(false)
    , eof_(false)
    , should_stop_(false)
    , volume_(1.0f)
    , buffer_read_pos_(0)
    , input_stream_(nullptr)
    , stream_expect_size_(0)
    , stream_eof_(false)
    , stop_stream_reader_(false)
    , avio_ctx_(nullptr)
    , avio_buffer_(nullptr) {
    
    // Initialize FFmpeg (av_register_all is deprecated in FFmpeg 4.0+)
    // Modern FFmpeg auto-registers codecs, so this is not needed
    avformat_network_init();
}

FFmpegStreamDecoder::~FFmpegStreamDecoder() {
    stop();
    cleanupFFmpeg();
}

bool FFmpegStreamDecoder::openStream(std::shared_ptr<std::istream> input_stream) {
    cleanupFFmpeg();
    
    input_stream_ = input_stream;
    stream_eof_ = false;
    
    if (!setupCustomIO()) {
        std::cerr << "Failed to setup custom IO for stream" << std::endl;
        return false;
    }
    
    return initializeCodec();
}

bool FFmpegStreamDecoder::setupCustomIO() {
    if (!input_stream_) return false;
    
    // Allocate AVIO buffer
    avio_buffer_ = static_cast<uint8_t*>(av_malloc(AVIO_BUFFER_SIZE));
    if (!avio_buffer_) {
        std::cerr << "Failed to allocate AVIO buffer" << std::endl;
        return false;
    }
    
    // Create custom AVIO context
    avio_ctx_ = avio_alloc_context(avio_buffer_, AVIO_BUFFER_SIZE, 0, this, readPacket, nullptr, seek);
    if (!avio_ctx_) {
        std::cerr << "Failed to allocate AVIO context" << std::endl;
        av_free(avio_buffer_);
        avio_buffer_ = nullptr;
        return false;
    }
    
    // Create format context with custom IO
    format_ctx_ = avformat_alloc_context();
    if (!format_ctx_) {
        std::cerr << "Failed to allocate format context" << std::endl;
        return false;
    }
    
    format_ctx_->pb = avio_ctx_;
    
    // Start stream reader thread to fill buffer
    stream_reader_thread_ = std::thread(&FFmpegStreamDecoder::streamReaderLoop, this);
    
    // Wait for initial buffering (at least 64KB for proper format detection)
    const size_t min_buffer_size = 64 * 1024;
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(5);
    
    while (true) {
        if (stream_buffer_.size() >= min_buffer_size || stream_eof_) {
            break;
        }
        
        if (std::chrono::steady_clock::now() - start_time > timeout) {
            std::cerr << "Timeout waiting for initial buffering" << std::endl;
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Open stream with custom IO
    if (avformat_open_input(&format_ctx_, nullptr, nullptr, nullptr) < 0) {
        std::cerr << "Failed to open custom stream" << std::endl;
        return false;
    }
    
    return true;
}

void FFmpegStreamDecoder::streamReaderLoop() {
    
    if (input_stream_) {
        // Try to determine if this is a streaming input or file input
        input_stream_->seekg(0, std::ios::end);
        
        // Check if seek was successful (fail for streaming inputs)
        if (input_stream_->fail()) {
            // This is a streaming input - read chunks as they become available
            input_stream_->clear(); // Clear fail state

            stream_eof_ = false; // Mark as not complete

            char buffer[8192];
            while (!stop_stream_reader_.load()) {
                input_stream_->read(buffer, sizeof(buffer));
                if (input_stream_ == nullptr) break; // Check if stream was closed
                auto bytes_read = input_stream_->gcount();
                
                if (bytes_read > 0) {
                    // Append to stream buffer
                    size_t old_size = stream_buffer_.size();
                    stream_buffer_.resize(old_size + bytes_read);
                    std::memcpy(stream_buffer_.data() + old_size, buffer, bytes_read);
                    
                } else if (input_stream_->eof()) {
                    stream_eof_ = true;
                    break;
                }
            }
        } else {
            // This is a regular file - load entire file into memory
            auto file_size = input_stream_->tellg();
            input_stream_->seekg(0, std::ios::beg);
            
            // Reserve space and read entire file
            stream_buffer_.reserve(static_cast<size_t>(file_size));
            stream_buffer_.resize(static_cast<size_t>(file_size));
            
            input_stream_->read(reinterpret_cast<char*>(stream_buffer_.data()), file_size);
            auto bytes_read = input_stream_->gcount();
            
            
            if (bytes_read < file_size) {
                stream_buffer_.resize(static_cast<size_t>(bytes_read));
            }
            
            stream_eof_ = true;
        }
    }
}

bool FFmpegStreamDecoder::initializeCodec() {
    
    // Find stream info
    if (avformat_find_stream_info(format_ctx_, nullptr) < 0) {
        std::cerr << "Failed to find stream info" << std::endl;
        return false;
    }
    
    // Find audio stream
    const AVCodec* codec = nullptr;
    audio_stream_index_ = av_find_best_stream(format_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (audio_stream_index_ < 0) {
        std::cerr << "No audio stream found" << std::endl;
        return false;
    }
    
    // Create codec context
    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) {
        std::cerr << "Failed to allocate codec context" << std::endl;
        return false;
    }
    
    // Copy codec parameters
    AVStream* audio_stream = format_ctx_->streams[audio_stream_index_];
    if (avcodec_parameters_to_context(codec_ctx_, audio_stream->codecpar) < 0) {
        std::cerr << "Failed to copy codec parameters" << std::endl;
        return false;
    }
    
    // Open codec
    if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
        std::cerr << "Failed to open codec" << std::endl;
        return false;
    }
    
    // Extract audio format information from the decoded stream
    audio_format_.sample_rate = codec_ctx_->sample_rate;
    audio_format_.channels = codec_ctx_->ch_layout.nb_channels;
    audio_format_.bits_per_sample = 16; // We'll always convert to 16-bit for compatibility
    
    // Initialize resampler
    swr_ctx_ = swr_alloc();
    if (!swr_ctx_) {
        std::cerr << "Failed to allocate resampler" << std::endl;
        return false;
    }
    
    // Set input parameters (use new ch_layout API for FFmpeg 5.1+)
    AVChannelLayout in_ch_layout;
    av_channel_layout_copy(&in_ch_layout, &codec_ctx_->ch_layout);
    av_opt_set_chlayout(swr_ctx_, "in_chlayout", &in_ch_layout, 0);
    av_opt_set_int(swr_ctx_, "in_sample_rate", codec_ctx_->sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx_, "in_sample_fmt", codec_ctx_->sample_fmt, 0);
    
    // Set output parameters (preserve sample rate and channels, but convert to 16-bit)
    AVChannelLayout out_ch_layout;
    av_channel_layout_copy(&out_ch_layout, &codec_ctx_->ch_layout);
    av_opt_set_chlayout(swr_ctx_, "out_chlayout", &out_ch_layout, 0);
    av_opt_set_int(swr_ctx_, "out_sample_rate", codec_ctx_->sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx_, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    
    // Initialize resampler
    if (swr_init(swr_ctx_) < 0) {
        std::cerr << "Failed to initialize resampler" << std::endl;
        return false;
    }
    
    return true;
}

bool FFmpegStreamDecoder::play() {
    if (!format_ctx_ || !codec_ctx_) {
        std::cerr << "Stream not initialized" << std::endl;
        return false;
    }
    
    if (playing_) {
        paused_ = false;
        return true;
    }
    
    playing_ = true;
    paused_ = false;
    should_stop_ = false;
    eof_ = false;
    
    // Start decode and playback threads
    decode_thread_ = std::thread(&FFmpegStreamDecoder::decodeLoop, this);
    playback_thread_ = std::thread(&FFmpegStreamDecoder::playbackLoop, this);
    
    return true;
}

bool FFmpegStreamDecoder::pause() {
    paused_ = !paused_;
    return true;
}

bool FFmpegStreamDecoder::stop() {
    if (!playing_) return true;
    // Signal all threads to stop. Make sure the stream reader is asked to stop
    // early so any blocking read can be avoided (reader loop checks this flag).
    should_stop_ = true;
    stop_stream_reader_.store(true);
    playing_ = false;
    paused_ = false;

    // If the input stream is a StreamingInputStream, destroy its buffer to
    // wake any blocked readers (this will make underflow() return EOF).
    if (input_stream_) {
        auto sis = dynamic_cast<StreamingInputStream*>(input_stream_.get());
        if (sis) {
            std::cerr << "FFmpegStreamDecoder::stop(): destroying streaming input buffer" << std::endl;
            sis->destroyBuffer();
        }
    }

    // Wake up threads waiting on condition variables.
    frame_cv_.notify_all();

    // Wait for decode/playback threads to finish.
    if (decode_thread_.joinable()) {
        decode_thread_.join();
    }
    if (playback_thread_.joinable()) {
        playback_thread_.join();
    }

    // Clear frame queue
    std::lock_guard<std::mutex> lock(frame_mutex_);
    while (!frame_queue_.empty()) {
        frame_queue_.pop();
    }

    return true;
}

void FFmpegStreamDecoder::decodeLoop() {
    AVPacket packet;
    AVFrame* frame = av_frame_alloc();
    
    if (!frame) {
        std::cerr << "Failed to allocate frame" << std::endl;
        return;
    }

    while (playing_ && !should_stop_) {
        // Read packet
        int ret = av_read_frame(format_ctx_, &packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                
                if (stream_eof_) {
                    eof_ = true;
                    frame_cv_.notify_all();
                    break;
                } else {
                    
                    // Reset FFmpeg's internal EOF state by seeking to current position
                    if (format_ctx_->pb) {
                        int64_t current_pos = avio_tell(format_ctx_->pb);
                        
                        // Seek to current position to clear EOF flag
                        int seek_result = avio_seek(format_ctx_->pb, current_pos, SEEK_SET);
                        
                        // Also try to flush any internal buffers
                        if (format_ctx_->pb->buf_ptr) {
                            format_ctx_->pb->buf_ptr = format_ctx_->pb->buffer;
                            format_ctx_->pb->buf_end = format_ctx_->pb->buffer;
                        }
                    }
                    
                    // Wait for more data to accumulate and try again
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }
            } else if (ret == AVERROR(EAGAIN)) {
                // Temporarily no data available, wait briefly and retry
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            std::cerr << "Error reading frame: " << ret << std::endl;
            break;
        }
        
        // Skip non-audio packets
        if (packet.stream_index != audio_stream_index_) {
            av_packet_unref(&packet);
            continue;
        }
        
        // Send packet to decoder
        ret = avcodec_send_packet(codec_ctx_, &packet);
        if (ret < 0) {
            std::cerr << "Error sending packet to decoder" << std::endl;
            av_packet_unref(&packet);
            continue;
        }
        
        // Receive decoded frames
        while (ret >= 0 && playing_ && !should_stop_) {
            ret = avcodec_receive_frame(codec_ctx_, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                std::cerr << "Error receiving frame from decoder" << std::endl;
                break;
            }
            
            // Convert frame to output format
            auto audio_frame = decodeFrame(frame);
            if (audio_frame) {
                // Add to queue
                std::unique_lock<std::mutex> lock(frame_mutex_);
                frame_cv_.wait(lock, [this] { return frame_queue_.size() < MAX_QUEUE_SIZE || should_stop_; });
                
                if (!should_stop_) {
                    frame_queue_.push(audio_frame);
                    frame_cv_.notify_one();
                }
            }
        }
        
        av_packet_unref(&packet);
    }
    
    av_frame_free(&frame);
}

void FFmpegStreamDecoder::playbackLoop() {
    auto start_time = std::chrono::steady_clock::now();
    double total_samples_played = 0.0;
    
    while (playing_ && !should_stop_) {
        std::unique_lock<std::mutex> lock(frame_mutex_);
        
        // Wait for frames
        frame_cv_.wait(lock, [this] { 
            return !frame_queue_.empty() || should_stop_ || eof_; 
        });
        
        if (should_stop_) break;
        
        if (frame_queue_.empty()) {
            if (eof_) {
                playing_ = false;
                break;
            }
            continue;
        }
        
        auto frame = frame_queue_.front();
        frame_queue_.pop();
        frame_cv_.notify_one();
        
        lock.unlock();
        
        // Skip if paused
        if (paused_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Calculate frame duration based on samples and sample rate
        int bytes_per_sample = 2; // 16-bit = 2 bytes
        int samples_per_frame = frame->data.size() / (frame->channels * bytes_per_sample);
        double frame_duration_ms = (samples_per_frame * 1000.0) / frame->sample_rate;
        
        // Apply volume and call audio callback
        if (audio_callback_) {
            applyVolume(frame->data.data(), frame->data.size(), frame->channels, volume_);
            audio_callback_(frame->data.data(), frame->data.size(), frame->sample_rate, frame->channels);
        }
        
        // Update total samples played
        total_samples_played += samples_per_frame;
        
        // Calculate how long playback should have taken up to now
        double expected_time_ms = (total_samples_played * 1000.0) / frame->sample_rate;
        
        // Calculate actual elapsed time
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        
        // Sleep if we're ahead of schedule (playing too fast)
        if (expected_time_ms > elapsed_ms) {
            double sleep_time = expected_time_ms - elapsed_ms;
            if (sleep_time > 0 && sleep_time < 1000) { // Sanity check
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(sleep_time)));
            }
        }
    }
}

std::shared_ptr<AudioFrame> FFmpegStreamDecoder::decodeFrame(AVFrame* frame) {
    if (!swr_ctx_) return nullptr;
    
    // Calculate output buffer size
    int out_samples = swr_get_out_samples(swr_ctx_, frame->nb_samples);
    int out_channels = audio_format_.channels;
    int bytes_per_sample = 2; // 16-bit
    int buffer_size = out_samples * out_channels * bytes_per_sample;
    
    auto audio_frame = std::make_shared<AudioFrame>(buffer_size);
    audio_frame->sample_rate = audio_format_.sample_rate;
    audio_frame->channels = out_channels;
    audio_frame->pts = frame->pts;
    
    // Convert audio
    uint8_t* output_data = audio_frame->data.data();
    int converted_samples = swr_convert(swr_ctx_,
                                       &output_data, out_samples,
                                       const_cast<const uint8_t**>(frame->data), frame->nb_samples);
    
    if (converted_samples < 0) {
        std::cerr << "Error converting audio" << std::endl;
        return nullptr;
    }
    
    // Resize buffer to actual size
    int actual_size = converted_samples * out_channels * bytes_per_sample;
    audio_frame->data.resize(actual_size);
    
    return audio_frame;
}

void FFmpegStreamDecoder::applyVolume(uint8_t* data, int size, int channels, float volume) {
    if (volume == 1.0f) return; // No change needed
    
    int16_t* samples = reinterpret_cast<int16_t*>(data);
    int num_samples = size / sizeof(int16_t);
    
    for (int i = 0; i < num_samples; ++i) {
        int32_t sample = static_cast<int32_t>(samples[i] * volume);
        // Clamp to 16-bit range
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        samples[i] = static_cast<int16_t>(sample);
    }
}

// Audio format methods
AudioFormat FFmpegStreamDecoder::getAudioFormat() const {
    return audio_format_;
}

int FFmpegStreamDecoder::getSampleRate() const {
    return audio_format_.sample_rate;
}

int FFmpegStreamDecoder::getChannels() const {
    return audio_format_.channels;
}

int FFmpegStreamDecoder::getBitsPerSample() const {
    return audio_format_.bits_per_sample;
}

// Stream information methods
double FFmpegStreamDecoder::getDuration() const {
    if (!format_ctx_) return 0.0;
    
    if (format_ctx_->duration != AV_NOPTS_VALUE) {
        return static_cast<double>(format_ctx_->duration) / AV_TIME_BASE;
    }
    
    // Try to get duration from audio stream
    if (audio_stream_index_ >= 0) {
        AVStream* stream = format_ctx_->streams[audio_stream_index_];
        if (stream->duration != AV_NOPTS_VALUE) {
            return static_cast<double>(stream->duration) * av_q2d(stream->time_base);
        }
    }
    
    return 0.0;
}

std::string FFmpegStreamDecoder::getCodecName() const {
    if (!codec_ctx_ || !codec_ctx_->codec) return "Unknown";
    return codec_ctx_->codec->name;
}

bool FFmpegStreamDecoder::seek(double position_seconds) {
    if (!format_ctx_) return false;
    
    int64_t seek_target = static_cast<int64_t>(position_seconds * AV_TIME_BASE);
    
    if (av_seek_frame(format_ctx_, -1, seek_target, AVSEEK_FLAG_BACKWARD) < 0) {
        return false;
    }
    
    // Clear frame queue
    std::lock_guard<std::mutex> lock(frame_mutex_);
    while (!frame_queue_.empty()) {
        frame_queue_.pop();
    }
    
    // Flush codec buffers
    if (codec_ctx_) {
        avcodec_flush_buffers(codec_ctx_);
    }
    
    return true;
}

double FFmpegStreamDecoder::getCurrentPosition() const {
    // This is a simplified implementation
    // In a real implementation, you'd track the current playback position
    return 0.0;
}

void FFmpegStreamDecoder::setVolume(float volume) {
    volume_ = std::max(0.0f, std::min(1.0f, volume));
}

float FFmpegStreamDecoder::getVolume() const {
    return volume_;
}

void FFmpegStreamDecoder::setAudioCallback(AudioCallback callback) {
    audio_callback_ = callback;
}

void FFmpegStreamDecoder::setStreamExpectedSize(uint64_t size)
{
    stream_expect_size_ = size;
}

bool FFmpegStreamDecoder::isPlaying() const {
    return playing_;
}

bool FFmpegStreamDecoder::isPaused() const {
    return paused_;
}

bool FFmpegStreamDecoder::isEOF() const {
    return eof_;
}

void FFmpegStreamDecoder::cleanupFFmpeg() {
    // Stop stream reader thread
    if (stream_reader_thread_.joinable()) {
        stop_stream_reader_.store(true);
        stream_reader_thread_.join();
        stop_stream_reader_.store(false);
    }
    
    // Clean up resampler
    if (swr_ctx_) {
        swr_free(&swr_ctx_);
    }
    
    // Clean up codec
    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
    }
    
    // Clean up format
    if (format_ctx_) {
        avformat_close_input(&format_ctx_);
    }
    
    // Clean up AVIO
    if (avio_ctx_) {
        // avio_context_free will free the internal buffer if one was
        // provided to avio_alloc_context. To avoid double-freeing the
        // same buffer, clear our pointer after freeing the context.
        avio_context_free(&avio_ctx_);
        avio_buffer_ = nullptr;
    } else if (avio_buffer_) {
        // If we created a buffer but never attached an AVIO context,
        // free it here.
        av_free(avio_buffer_);
        avio_buffer_ = nullptr;
    }
    
    // Reset audio format
    audio_format_ = AudioFormat();
    
    // Clear stream buffer
    stream_buffer_.clear();
    buffer_read_pos_ = 0;
}

// Static AVIO callbacks
int FFmpegStreamDecoder::readPacket(void* opaque, uint8_t* buf, int buf_size) {
    auto* decoder = static_cast<FFmpegStreamDecoder*>(opaque);
    
    if (!decoder || buf_size <= 0) {
        std::cerr << "readPacket: Invalid parameters" << std::endl;
        return AVERROR(EINVAL);
    }

    // If we've been asked to stop, return an I/O error so FFmpeg can abort
    if (decoder->should_stop_ || decoder->stop_stream_reader_.load()) {
        std::cerr << "readPacket: stop requested, returning EIO" << std::endl;
        return AVERROR(EIO);
    }
    
    size_t current_pos = decoder->buffer_read_pos_.load();
    size_t available = decoder->stream_buffer_.size() - current_pos;
    
    if (available == 0) {
        if (decoder->stream_eof_) {
            // Stream is truly finished - return EOF
            return AVERROR_EOF;
        } else {
            // For streaming, wait a bit for more data instead of returning EAGAIN immediately
            
            // Wait briefly for new data to arrive
            size_t initial_size = decoder->stream_buffer_.size();
            int wait_attempts = 0;
            while (decoder->stream_buffer_.size() <= initial_size && 
                   !decoder->stream_eof_ && wait_attempts < 50) { // Wait max 500ms
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                wait_attempts++;
            }
            
            // Check if new data arrived
            size_t new_available = decoder->stream_buffer_.size() - current_pos;
            if (new_available > 0) {
                // Don't return here, let it fall through to read the new data
            } else if (decoder->stream_eof_) {
                return AVERROR_EOF;
            } else {
                // Still no data and stream not finished, return EAGAIN
                return AVERROR(EAGAIN);
            }
            
            // Recalculate available data
            available = decoder->stream_buffer_.size() - current_pos;
            if (available == 0) {
                return AVERROR(EAGAIN);
            }
        }
    }
    
    size_t to_read = std::min(available, static_cast<size_t>(buf_size));
    
    std::memcpy(buf, decoder->stream_buffer_.data() + current_pos, to_read);
    decoder->buffer_read_pos_ += to_read;
    
    static int call_count = 0;
    call_count++;
    if (call_count % 10 == 0) { // Log every 10th call to reduce verbosity
        
        // Calculate percentage of buffer consumed
        double percent_read = (double)current_pos / decoder->stream_buffer_.size() * 100.0;
    }
    
    return static_cast<int>(to_read);
}

int64_t FFmpegStreamDecoder::seek(void* opaque, int64_t offset, int whence) {
    auto* decoder = static_cast<FFmpegStreamDecoder*>(opaque);
    
    if (!decoder) return -1;
    
    size_t new_pos = 0;
    size_t buffer_size = decoder->stream_buffer_.size();
    
    switch (whence) {
        case SEEK_SET:
            new_pos = static_cast<size_t>(offset);
            break;
        case SEEK_CUR:
            new_pos = decoder->buffer_read_pos_.load() + static_cast<size_t>(offset);
            break;
        case SEEK_END:
            new_pos = buffer_size + static_cast<size_t>(offset);
            break;
        case AVSEEK_SIZE:
            return static_cast<int64_t>(decoder->stream_expect_size_);
        default:
            return -1;
    }
    
    if (new_pos > buffer_size) {
        new_pos = buffer_size;
    }
    
    decoder->buffer_read_pos_ = new_pos;
    return static_cast<int64_t>(new_pos);
}