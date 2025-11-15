#include "ffmpeg_decoder.h"

#include <log/log_manager.h>
#include "audio_event_processor.h"
#include <QVariantHash>
#include <QString>

namespace audio {
FFmpegStreamDecoder::FFmpegStreamDecoder(QObject* parent)
    : QObject(parent)
        , format_ctx_(nullptr)
        , codec_ctx_(nullptr),
      codec_(nullptr),
      swr_ctx_(nullptr),
      audioStreamIndex(-1),
      avio_ctx_(nullptr),
      avio_buffer_(nullptr),
      hasInit(false),
      lenFullSize(0),
      audioCache(512 * 1024), // 512KB buffer
      playing_(false),
      destroying_(false),
      decodeCompleted_(false),
    streamConsumptionFinished_(false),
    m_decoded_samples(0)
{
}

void FFmpegStreamDecoder::setEventProcessor(std::shared_ptr<AudioEventProcessor> processor)
{
    m_eventProcessor = processor;
}

FFmpegStreamDecoder::~FFmpegStreamDecoder()
{
    destroying_.store(true);
    playing_.store(false);
    
    // Wake up all waiting threads before joining
    decodeCondition.notify_all();
    decodeStateCondition.notify_all();
    audioFormatCondition_.notify_all();
    outputFrameQueue.reset();
    
    if (consumeAudioStreamThread.joinable())
    {
        consumeAudioStreamThread.join();
    }
    if (decodeAudioBytesThread.joinable())
    {
        decodeAudioBytesThread.join();
    }

    // Clean up FFmpeg resources
    if (swr_ctx_) {
        swr_free(&swr_ctx_);
        swr_ctx_ = nullptr;
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
        av_freep(&avio_ctx_->buffer);   // free your buffer
        av_freep(&avio_ctx_);           // free AVIOContext
        avio_buffer_ = nullptr;
    } else if (avio_buffer_) {
        // If we created a buffer but never attached an AVIO context,
        // free it here.
        av_free(avio_buffer_);
        avio_buffer_ = nullptr;
    }
}


bool FFmpegStreamDecoder::initialize(std::shared_ptr<std::istream> inputAudioStream, 
    std::weak_ptr<AudioFrameQueue> outputFrameQueue)
{
    {
        std::scoped_lock lock(audioCacheMutex);
        if (this->hasInit)
        {
            return false;
        }
        this->hasInit = true;

        this->inputAudioStream = inputAudioStream;
        this->outputFrameQueue = outputFrameQueue;
    }
    

    avformat_network_init();
    {
        std::scoped_lock lock(ffmpegMutex_);
        if (!setupCustomIO())
        {
            LOG_ERROR("Failed to setup custom IO for FFmpegStreamDecoder.");
            return false;
        }
    }
    

    return true;
}
// Notice: This function will block if bytes in audioCache is less than min_buffer_size(60*1024)!
bool FFmpegStreamDecoder::startDecoding(uint64_t lenFullStream)
{
    LOG_INFO("FFmpegStreamDecoder starting decoding. "
            "The length of full stream is {} bytes, "
            "audioCache's capacity is {} bytes.", lenFullStream, audioCache.capacity());
    if (lenFullStream ==0) {
        LOG_ERROR("Invalid full stream length {}.", lenFullStream);
        return false;
    }
    lenFullSize = lenFullStream;
    playing_.store(true);
    decodeCompleted_.store(false);
    streamConsumptionFinished_.store(false);
    // Start stream reader thread to fill buffer (wrap entry to catch exceptions)
    consumeAudioStreamThread = std::thread([this]() {
        try {
            this->consumeAudioStreamFunc();
        } catch (const std::exception& e) {
            LOG_ERROR("FFmpegStreamDecoder::consumeAudioStreamFunc threw: {}", e.what());
            std::terminate();
        } catch (...) {
            LOG_ERROR("FFmpegStreamDecoder::consumeAudioStreamFunc threw unknown exception");
            std::terminate();
        }
    });
    // Start decode and playback threads; perform FFmpeg setup inside decoding thread
    LOG_INFO("FFmpegStreamDecoder starting consumer and decoder threads (setup performed in decoder thread).");
    decodeAudioBytesThread = std::thread([this]() {
        try {
            this->decodeAudioBytesFunc();
        } catch (const std::exception& e) {
            LOG_ERROR("FFmpegStreamDecoder::decodeAudioBytesFunc threw: {}", e.what());
            std::terminate();
        } catch (...) {
            LOG_ERROR("FFmpegStreamDecoder::decodeAudioBytesFunc threw unknown exception");
            std::terminate();
        }
    });

    // startDecoding returns quickly; heavy lifting happens in decoder thread
    return true;
}

bool FFmpegStreamDecoder::pauseDecoding()
{
    if (!playing_.load()) {
        return false;
    }
    playing_.store(false);
    return true;
}

bool FFmpegStreamDecoder::resumeDecoding()
{
    if (!playing_.load()) {
        playing_.store(true);
        // Notify decode thread to resume
        decodeStateCondition.notify_all();
        return true;
    }
    return false;
}

bool FFmpegStreamDecoder::isDecoding() const
{
    return playing_.load();
}

bool FFmpegStreamDecoder::isCompleted() const
{
    return decodeCompleted_.load();
}

std::future<AudioFormat> FFmpegStreamDecoder::getAudioFormatAsync() const
{
    return std::async(std::launch::deferred, [this]() {
        std::unique_lock<std::mutex> lock(this->audioFormatMutex_);
        // Wait until audio format is available
        this->audioFormatCondition_.wait(lock, [this]() {
            return this->audio_format_.isValid() || this->destroying_.load();
        });
        return this->audio_format_;
    });
}

std::future<double> FFmpegStreamDecoder::getDurationAsync() const
{
    return std::async(std::launch::deferred, [this]() {
        std::unique_lock<std::mutex> lock(this->ffmpegMutex_);
        // Wait until format context is available
        this->ffmpegCondition_.wait(lock, [this]() {
            return this->format_ctx_ != nullptr || this->destroying_.load();
        });
        if (!format_ctx_ || destroying_.load()) return 0.0;
        if (format_ctx_->duration != AV_NOPTS_VALUE) {
            return static_cast<double>(format_ctx_->duration) / AV_TIME_BASE;
        }
        if (audioStreamIndex >= 0) {
            AVStream* stream = format_ctx_->streams[audioStreamIndex];
            if (stream->duration != AV_NOPTS_VALUE) {
                return static_cast<double>(stream->duration) * av_q2d(stream->time_base);
            }
        }
        return 0.0;
    });
}

void FFmpegStreamDecoder::consumeAudioStreamFunc()
{
    char buffer[4096];
    while (playing_.load() && !destroying_.load()) {
        std::unique_lock lock(audioCacheMutex);
        size_t toWrite = std::min(audioCache.capacity() - audioCache.size(), sizeof(buffer));
        if (toWrite == 0) {
            // Buffer full, wait until space is available
            // LOG_DEBUG("Audio cache full ({} bytes), waiting for space to become available.", audioCache.size());
            decodeCondition.wait(lock, [this]() {
                return audioCache.size() < audioCache.capacity() || destroying_.load();
            });
            // LOG_DEBUG("Audio cache has space available ({} bytes), resuming consumption.", audioCache.size());
            continue;
        }
        // Possible situation: stream has less data than requested,
        //  which would cause blocking here with lock held.
        lock.unlock();
        inputAudioStream->read(buffer, toWrite);
        std::streamsize bytesRead = inputAudioStream->gcount();
        if (!playing_.load() || destroying_.load()) {
            break;
        }
        if (bytesRead > 0) {
            // Reheld lock to write to audioCache
            lock.lock();
            audioCache.write(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(bytesRead));
            // LOG_DEBUG("Consumed {} bytes from input stream, audioCache size: {}", bytesRead, audioCache.size());
            decodeCondition.notify_all();
        } else {
            // Could be EOF or read error. Distinguish using stream state.
            if (inputAudioStream->eof()) {
                LOG_INFO("Stream consumption finished - no more data from input stream.");
                LOG_DEBUG("audioCache size at end of stream: {}", audioCache.size());
                emit readCompleted();
                if (auto proc = m_eventProcessor.lock()) {
                    QVariantHash data{};
                    proc->postEvent(AudioEventProcessor::STREAM_READ_COMPLETED, data);
                }
                break;
            } else {
                // treat as read error
                QString reason = QStringLiteral("Failed to read from input stream");
                LOG_ERROR("Stream read error while consuming input stream.");
                emit readError(reason);
                if (auto proc = m_eventProcessor.lock()) {
                    QVariantHash data{{QStringLiteral("error"), QVariant(reason)}};
                    proc->postEvent(AudioEventProcessor::STREAM_READ_ERROR, data);
                }
                break;
            }
        }
    }
    
    // Mark stream consumption as finished and notify waiting threads
    streamConsumptionFinished_.store(true);
    inputAudioStream.reset();
    decodeCondition.notify_all();
    LOG_INFO("Stream consumption thread completed");
}

void FFmpegStreamDecoder::decodeAudioBytesFunc()
{
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        LOG_ERROR("Failed to allocate AVPacket for decoding.");
        QString reason = QStringLiteral("Failed to allocate AVPacket for decoding.");
        emit decodingError(reason);
        if (auto proc = m_eventProcessor.lock()) {
            QVariantHash data{{QStringLiteral("error"), QVariant(reason)}};
            proc->postEvent(AudioEventProcessor::DECODING_ERROR, data);
        }
        return;
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        LOG_ERROR("Failed to allocate AVFrame for decoding.");
        av_packet_free(&packet);
        QString reason = QStringLiteral("Failed to allocate AVFrame for decoding.");
        emit decodingError(reason);
        if (auto proc = m_eventProcessor.lock()) {
            QVariantHash data{{QStringLiteral("error"), QVariant(reason)}};
            proc->postEvent(AudioEventProcessor::DECODING_ERROR, data);
        }
        return;
    }
    
    // Perform FFmpeg setup here in decoder thread so startDecoding() can return quickly
    {
        std::scoped_lock lock(ffmpegMutex_);
        // Open stream with custom IO
        if (avformat_open_input(&format_ctx_, nullptr, nullptr, nullptr) < 0) {
            LOG_ERROR("Failed to open custom stream for decoding.");
            QString reason = QStringLiteral("Failed to open custom stream for decoding.");
            emit decodingError(reason);
            return;
        }
        // Find stream info (may block inside readPacket, but decoder thread handles it)
        if (avformat_find_stream_info(format_ctx_, nullptr) < 0) {
            LOG_ERROR("Failed to find stream info for decoding.");
            QString reason = QStringLiteral("Failed to find stream info for decoding.");
            emit decodingError(reason);
            return;
        }

        // Find audio stream
        const AVCodec* codec = nullptr;
        audioStreamIndex = av_find_best_stream(format_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
        if (audioStreamIndex < 0) {
            LOG_ERROR("Failed to find audio stream for decoding.");
            QString reason = QStringLiteral("Failed to find audio stream for decoding.");
            emit decodingError(reason);
            return;
        }

        // Create codec context
        codec_ctx_ = avcodec_alloc_context3(codec);
        if (!codec_ctx_) {
            LOG_ERROR("Failed to allocate codec context.");
            QString reason = QStringLiteral("Failed to allocate codec context.");
            emit decodingError(reason);
            return;
        }

        //Copy codec parameters
        AVStream* audioStream = format_ctx_->streams[audioStreamIndex];
        if (avcodec_parameters_to_context(codec_ctx_, audioStream->codecpar) < 0) {
            LOG_ERROR("Failed to copy codec parameters to context.");
            QString reason = QStringLiteral("Failed to copy codec parameters to context.");
            emit decodingError(reason);
            return;
        }

        // Open codec
        if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
            LOG_ERROR("Failed to open codec for decoding.");
            QString reason = QStringLiteral("Failed to open codec for decoding.");
            emit decodingError(reason);
            return;
        }

        // Extract audio format information
        audio_format_.sample_rate = codec_ctx_->sample_rate;
        audio_format_.channels = codec_ctx_->ch_layout.nb_channels;
        audio_format_.bits_per_sample = 16;

        // notify waiters that audio format is available
        audioFormatCondition_.notify_all();

        // Init resampler
        swr_ctx_ = swr_alloc();
        if (!swr_ctx_) {
            LOG_ERROR("Failed to allocate resampler context.");
            QString reason = QStringLiteral("Failed to allocate resampler context.");
            emit decodingError(reason);
            return;
        }

        AVChannelLayout in_ch_layout = codec_ctx_->ch_layout;
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
            LOG_ERROR("Failed to initialize resampler.");
            QString reason = QStringLiteral("Failed to initialize resampler.");
            emit decodingError(reason);
            return;
        }

        // Signal that FFmpeg setup is complete
        ffmpegCondition_.notify_all();
    }

    while (playing_.load() && !destroying_.load()) {
        {
            std::unique_lock<std::mutex> lock(ffmpegMutex_);
            // Wait if paused
            decodeStateCondition.wait(lock, [this]() {
                return playing_.load() || destroying_.load();
            });
            if (destroying_.load()) {
                break;
            }
        }

        // Call av_read_frame - blocking for data is handled in readPacket callback
        int ret = av_read_frame(format_ctx_, packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                // At this point, we should have sufficient data or stream is truly finished
                decodeCompleted_.store(true);
                LOG_INFO("Decoding completed - av_read_frame returned EOF");
                emit decodingFinished();
                if (auto proc = m_eventProcessor.lock()) {
                    QVariantHash data{};
                    proc->postEvent(AudioEventProcessor::DECODING_FINISHED, data);
                }
                // Log cumulative decoded duration for diagnostics
                {
                    int64_t samples = m_decoded_samples.load(std::memory_order_relaxed);
                    double approx_seconds = 0.0;
                    if (audio_format_.sample_rate > 0) {
                        approx_seconds = static_cast<double>(samples) / static_cast<double>(audio_format_.sample_rate);
                    }
                    LOG_INFO("Decoding summary: decoded_samples={}, approx_seconds={} (sample_rate={})", samples, approx_seconds, audio_format_.sample_rate);
                }
                break;
            } else {
                LOG_ERROR("Error reading frame: {}", ret);
                QString reason = QStringLiteral("Error reading frame");
                emit decodingError(reason);
                if (auto proc = m_eventProcessor.lock()) {
                    QVariantHash data{{QStringLiteral("code"), QVariant(ret)}, {QStringLiteral("error"), QVariant(reason)}};
                    proc->postEvent(AudioEventProcessor::DECODING_ERROR, data);
                }
                continue;
            }
        }

        if (packet->stream_index != audioStreamIndex) {
            av_packet_unref(packet);
            continue;
        }

        // Send packet to decoder
            ret = avcodec_send_packet(codec_ctx_, packet);
            if (ret < 0) {
            LOG_ERROR("Error sending packet to decoder: {}", ret);
            av_packet_unref(packet);
            {
                QString reason = QStringLiteral("Error sending packet to decoder");
                emit decodingError(reason);
                if (auto proc = m_eventProcessor.lock()) {
                    QVariantHash data{{QStringLiteral("code"), QVariant(ret)}, {QStringLiteral("error"), QVariant(reason)}};
                    proc->postEvent(AudioEventProcessor::DECODING_ERROR, data);
                }
            }
            continue;
        }

        // Receive frames
        int lastRet = 0;
        while (ret >= 0 && playing_.load() && !destroying_.load()) {
            ret = avcodec_receive_frame(codec_ctx_, frame);
            if (ret == AVERROR(EAGAIN)){
                // if (lastRet != ret) {
                //     LOG_DEBUG("avcodec_receive_frame returned EAGAIN, continuing...");
                // }
                continue;
            } else if (ret == AVERROR_EOF) {
                if (outputFrameQueue.expired() == false) {
                    auto sharedQueue = outputFrameQueue.lock();
                    sharedQueue->clean();
                    LOG_DEBUG("avcodec_receive_frame returned EOF, ending frame reception loop, frameQueue size: {}", sharedQueue->size());
                }
                
                break;
            } else if (ret < 0) {
                LOG_ERROR("Error receiving frame from decoder: {}", ret);
                {
                    QString reason = QStringLiteral("Error receiving frame from decoder");
                    emit decodingError(reason);
                    if (auto proc = m_eventProcessor.lock()) {
                        QVariantHash data{{QStringLiteral("code"), QVariant(ret)}, {QStringLiteral("error"), QVariant(reason)}};
                        proc->postEvent(AudioEventProcessor::DECODING_ERROR, data);
                    }
                }
                break;
            }
            lastRet = ret;

            // Convert frame to output format
            auto audio_frame = decodeFrame(frame);
            if (audio_frame) {
                // Add to queue
                if (outputFrameQueue.expired() == false) {
                    auto sharedQueue = outputFrameQueue.lock();
                    sharedQueue->enqueue(audio_frame);
                    // LOG_DEBUG("Decoded and queued an audio frame with PTS {}", frame->pts);
                } else {
                    LOG_ERROR("Failed to push decoded frame to output queue - queue expired, PTS {}", frame->pts);
                    if (auto proc = m_eventProcessor.lock()) {
                        QVariantHash data{{QStringLiteral("error"), QVariant(QStringLiteral("Output frame queue expired"))}};
                        proc->postEvent(AudioEventProcessor::DECODING_ERROR, data);
                    }
                    break;
                }
            }
        }
        av_packet_unref(packet);
    }
    LOG_DEBUG("Decoding thread exiting, reason: playing_={}, destroying_={}", playing_.load(), destroying_.load());

    // free frame and packet resources
    if (frame) {
        av_frame_free(&frame);
    }
    if (packet) {
        av_packet_free(&packet);
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
    audio_frame->bits_per_sample = bytes_per_sample * 8;
    audio_frame->pts = frame->pts;
    
    // Convert audio
    uint8_t* output_data = audio_frame->data.data();
    int converted_samples = swr_convert(swr_ctx_,
                                       &output_data, out_samples,
                                       const_cast<const uint8_t**>(frame->data), frame->nb_samples);
    
    if (converted_samples < 0) {
        LOG_ERROR("Error converting audio");
        return nullptr;
    }
    
    // Resize buffer to actual size
    int actual_size = converted_samples * out_channels * bytes_per_sample;
    audio_frame->data.resize(actual_size);
    
    // Update diagnostic counter: converted_samples is samples per channel
    m_decoded_samples.fetch_add(static_cast<int64_t>(converted_samples), std::memory_order_relaxed);

    return audio_frame;
}

int64_t FFmpegStreamDecoder::seek(void* opaque, int64_t offset, int whence) {
    auto* decoder = static_cast<FFmpegStreamDecoder*>(opaque);
    if (whence == AVSEEK_SIZE) {
        return decoder ? decoder->lenFullSize : -1;
    }
    return -1;
}

int FFmpegStreamDecoder::readPacket(void *opaque, uint8_t *buf, int buf_size)
{
    auto* decoder = static_cast<FFmpegStreamDecoder*>(opaque);
    
    if (!decoder || buf_size <= 0) {
        LOG_ERROR("readPacket: Invalid parameters.");
        return AVERROR(EINVAL);
    }
    std::unique_lock<std::mutex> lock(decoder->audioCacheMutex);
    
    if (decoder->audioCache.capacity() < static_cast<size_t>(buf_size)) {
        LOG_ERROR("readPacket: requested buf_size {} is larger than audio cache capacity {}.", buf_size, decoder->audioCache.capacity());
        return AVERROR(EINVAL);
    }
    
    // Wait until we have sufficient data OR stream consumption is finished
    const size_t min_available = std::min(static_cast<size_t>(buf_size), static_cast<size_t>(8192));
    
    // Block until enough data is available, or input stream is finished
    while (decoder->audioCache.size() < min_available && 
           !decoder->streamConsumptionFinished_.load() && 
           !decoder->destroying_.load() && 
           decoder->playing_.load()) {
        
        LOG_DEBUG("readPacket: Cache has {} bytes, need at least {}, waiting for more data", 
                 decoder->audioCache.size(), min_available);
        
        decoder->decodeCondition.wait(lock, [decoder, min_available]() {
            return decoder->audioCache.size() >= min_available || 
                   decoder->streamConsumptionFinished_.load() || 
                   decoder->destroying_.load() || 
                   !decoder->playing_.load();
        });
        LOG_DEBUG("readPacket: Woke up, cache has {} bytes", decoder->audioCache.size());
    }

    // if (decoder->streamConsumptionFinished_.load()) {
    //     LOG_DEBUG("readPacket: Stream consumption finished with {} bytes in cache", decoder->audioCache.size());
    // }
    
    // Check exit conditions
    if (decoder->destroying_.load() || !decoder->playing_.load()) {
        LOG_INFO("readPacket: Decoder stopping, returning EOF");
        return AVERROR_EOF;
    }
    
    size_t available = decoder->audioCache.size();
    if (available == 0) {
        // Only return EOF if stream consumption is truly finished
        if (decoder->streamConsumptionFinished_.load()) {
            LOG_INFO("readPacket: Stream consumption finished and no data in cache, returning EOF.");

            return AVERROR_EOF;
        } else {
            // This shouldn't happen after the wait above, but just in case
            LOG_DEBUG("readPacket: Unexpected - no data available but stream ongoing");
            return AVERROR(EAGAIN);
        }
    }
    
    size_t to_read = std::min(available, static_cast<size_t>(buf_size));
    decoder->audioCache.read(buf, to_read);
    // Notify producer that space is available
    // LOG_DEBUG("readPacket: Provided {} bytes from audio cache({}) to FFmpeg", to_read, available - to_read);
    decoder->decodeCondition.notify_all();
    return static_cast<int>(to_read);
}

/***************Private Methods ***************/
bool FFmpegStreamDecoder::setupCustomIO() {
    if (!inputAudioStream) return false;
    
    // Allocate AVIO buffer
    avio_buffer_ = static_cast<uint8_t*>(av_malloc(AVIO_BUFFER_SIZE));
    if (!avio_buffer_) {
        LOG_ERROR("Failed to allocate AVIO buffer.");
        return false;
    }
    
    // Create custom AVIO context
    avio_ctx_ = avio_alloc_context(avio_buffer_, AVIO_BUFFER_SIZE, 0, this, readPacket, nullptr, seek);
    if (!avio_ctx_) {
        LOG_ERROR("Failed to allocate AVIO context.");
        av_free(avio_buffer_);
        avio_buffer_ = nullptr;
        return false;
    }
    
    // Create format context with custom IO
    format_ctx_ = avformat_alloc_context();
    if (!format_ctx_) {
        LOG_ERROR("Failed to allocate format context.");
        return false;
    }
    format_ctx_->pb = avio_ctx_;
    format_ctx_->flags |= AVFMT_FLAG_CUSTOM_IO;
    return true;
}
} // namespace audio