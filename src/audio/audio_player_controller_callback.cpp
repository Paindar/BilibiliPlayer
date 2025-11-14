#include "audio_player_controller.h"
#include <manager/application_context.h>
#include <config/config_manager.h>
#include <playlist/playlist_manager.h>
#include <network/network_manager.h>
#include <log/log_manager.h>
#include <QTimer>
#include <iostream>
#include <fstream>

namespace audio {
void AudioPlayerController::onPlayEvent(const QVariantHash&)
{
    std::scoped_lock locker(m_stateMutex, m_componentMutex);
    
    switch (m_currentState) {
    case PlaybackState::Playing:
        LOG_WARN(" Play event received but playback is already in progress");
        break;
    case PlaybackState::Paused:
        if (m_decoder) {
            m_decoder->resumeDecoding();
        } 
        if (m_audioOutput) {
            m_audioOutput->start();
            m_currentState = PlaybackState::Playing;
            m_positionTimer->start();
            LOG_INFO(" Playback resumed");
            emit playbackStateChanged(m_currentState);
        } else {
            LOG_ERROR(" Audio output is not initialized, cannot resume playback");
            m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_COMPLETE);
        }
        break;
    case PlaybackState::Stopped:
        if (m_currentPlaylist.isEmpty()) {
            LOG_WARN(" Play event received but playlist is empty");
        }
        if (m_currentIndex < 0 || m_currentIndex >= m_currentPlaylist.size()) {
            LOG_WARN(" Invalid current song index");
            return;
        }
        //Robustness: Ensure components' status
        // Clean up previous component: stop/pause but KEEP instances for reuse
        do {
            bool success = true;
            if (m_frameQueue->empty() == false) {
                success = false;
                LOG_ERROR(" Frame queue is not empty before starting new playback");
            }
            if (m_playbackWatcherThread) {
                success = false;
                LOG_ERROR(" Playback watcher thread is still running before starting new playback");
            }
            if (m_audioOutput) {
                success = false;
                LOG_ERROR(" Audio output is not stopped before starting new playback");
            }
            if (m_decoder) {
                success = false;
                LOG_ERROR(" Decoder is not stopped before starting new playback");
            }
            if (m_currentStream) {
                success = false;
                LOG_ERROR(" Current stream is not released before starting new playback");
            }
            if (!success) {
                LOG_WARN(" Previous playback components not properly cleaned up");
                m_eventProcessor->postEvent(audio::AudioEventProcessor::STOP);
                return;
            }
            playCurrentSongUnsafe();
        }while (false);
        break;
    default:
        LOG_ERROR(" Unknown playback state: {}", magic_enum::enum_name(m_currentState));
    }
}

void AudioPlayerController::onPauseEvent(const QVariantHash&)
{
    std::scoped_lock locker(m_stateMutex, m_componentMutex);
    
    switch (m_currentState) {
    case PlaybackState::Playing:
        if (m_decoder) {
            m_decoder->pauseDecoding();
        }
        if (m_audioOutput) {
            m_audioOutput->pause();
        }
        if (m_positionTimer) {
            m_positionTimer->stop();
        }
        
        m_currentState = PlaybackState::Paused;
        
        LOG_INFO(" Playback paused");
        emit playbackStateChanged(m_currentState);
        break;
    case PlaybackState::Paused:
        LOG_WARN(" Pause event received but playback is already paused");
        break;
    case PlaybackState::Stopped:
        LOG_WARN(" Pause event received but playback is already stopped");
        break;
    default:
        LOG_ERROR(" Unknown playback state: {}", magic_enum::enum_name(m_currentState));
    }
}

void AudioPlayerController::onStopEvent(const QVariantHash&)
{
    std::scoped_lock locker(m_stateMutex, m_componentMutex);
    switch (m_currentState) {
    case PlaybackState::Playing:
    case PlaybackState::Paused:
        cleanPlayResourcesUnsafe();
        m_currentState = PlaybackState::Stopped;
    case PlaybackState::Stopped:
        LOG_INFO(" Playback stopped");
        break;
    default:
        LOG_ERROR(" Unknown playback state: {}", magic_enum::enum_name(m_currentState));
    }
}

void AudioPlayerController::onPlayFinishedEvent(const QVariantHash &)
{
    {
        std::scoped_lock locker(m_stateMutex, m_componentMutex);
        cleanPlayResourcesUnsafe();
        m_currentState = PlaybackState::Stopped;
        m_positionTimer->stop();
        LOG_INFO(" Playback finished");
        emit playbackStateChanged(m_currentState);
        emit playbackCompleted();
    }
    next();
}

void AudioPlayerController::onPlayErrorEvent(const QVariantHash & args)
{
    // Handle play error event
    emit playbackError("Play error");
    QString errMsg = args.value("error").toString();
    LOG_ERROR("Playback error, details: {}", errMsg.toStdString());
    m_eventProcessor->postEvent(audio::AudioEventProcessor::STOP);
}

void AudioPlayerController::onInputStreamReadCompletedEvent(const QVariantHash&)
{
    // Clean up current stream
    std::scoped_lock locker(m_componentMutex);
    m_currentStream.reset();
}

void AudioPlayerController::onInputStreamReadErrorEvent(const QVariantHash& args)
{
    // Handle input stream read error
    QString errMsg = args.value("error").toString();
    LOG_ERROR("Input stream read error, details: {}", errMsg.toStdString());
    emit playbackError("Input stream read error");
    m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_ERROR,
                                QVariantHash{{QString("error"), QVariant(QString("Input stream read error"))}});
}

void AudioPlayerController::onDecodingFinishedEvent(const QVariantHash&)
{
    // Handle decoding finished event
    LOG_INFO(" Decoding finished");
    std::scoped_lock locker(m_componentMutex);
    // Decoder finished — pause but keep instance for potential reuse
    if (m_decoder) {
        m_decoder.reset();
        LOG_DEBUG(" Decoder instance released after decoding finished");
    }
}

void AudioPlayerController::onDecodingErrorEvent(const QVariantHash& args)
{
    // Handle decoding error
    QString errMsg = args.value("error").toString();
    LOG_ERROR(" Decoding error, details: {}", errMsg.toStdString());
    emit playbackError("Decoding error");
    m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_ERROR,
                                QVariantHash{{QString("error"), QVariant(QString("Decoding error"))}});
}

void AudioPlayerController::onFrameTransmissionFinishedEvent(const QVariantHash&)
{
    LOG_INFO(" Frame transmission finished");
}

void AudioPlayerController::onFrameTransmissionErrorEvent(const QVariantHash& args)
{
    // Handle frame transmission error
    QString errMsg = args.value("error").toString();
    LOG_ERROR("Frame transmission error, details: {}", errMsg.toStdString());
    emit playbackError("Frame transmission error");
    m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_ERROR,
                                QVariantHash{{QString("error"), QVariant(QString("Frame transmission error"))}});
}

// Only can be called from onPlayEvent when holding all mutexes.
void AudioPlayerController::playCurrentSongUnsafe()
{
    const auto& song = m_currentPlaylist[m_currentIndex];
    LOG_INFO(" Playing song: {} by {}", song.title.toStdString(), song.uploader.toStdString());
    
    // Check if local file exists
    QString filepath = song.filepath;
    bool useStreaming = false;
    uint64_t expectedSize = 0;
    
    if (filepath.isEmpty() || !isLocalFileAvailable(song)) {
        // Generate streaming filepath
        useStreaming = true;

        LOG_INFO(" Using streaming for song: {}", song.title.toStdString());
    }
    // Create decoder as shared_ptr so we can share it and pass the event processor
    m_decoder = std::make_unique<FFmpegStreamDecoder>();
    // Pass the controller's event processor to the decoder so it can post events
    if (m_decoder && m_eventProcessor) {
        m_decoder->setEventProcessor(m_eventProcessor);
    }
    
    // Prepare input and decoder
    bool opened = false;
    try {
        if (useStreaming) {
            // Acquire real-time stream via NetworkManager
            auto platform = static_cast<network::PlatformType>(song.platform);

            // Start both requests: expected size first (critical), and stream acquisition
            auto sizeFuture = NETWORK_MANAGER->getStreamSizeByParamsAsync(platform, song.args);
            auto streamFuture = NETWORK_MANAGER->getAudioStreamAsync(platform, song.args, filepath);

            // Get expected size
            try {
                expectedSize = sizeFuture.get();
            } catch (std::exception& e) {
                LOG_WARN(" Failed to get expected stream size: {}", e.what());
                expectedSize = 0;
            }

            m_currentStream = streamFuture.get();
            if (!m_currentStream) {
                throw std::runtime_error("Failed to acquire streaming input");
            }
            // Todo test if it is still good after removing waiting for at least some data
            opened = m_decoder->initialize(m_currentStream, m_frameQueue);
        } else {
            // Use local file stream
            auto fs = std::make_shared<std::ifstream>(filepath.toStdString(), std::ios::binary);
            if (!fs->is_open()) {
                throw std::runtime_error("Failed to open local file");
            }
            expectedSize = fs->seekg(0, std::ios::end).tellg();
            fs->seekg(0, std::ios::beg);
            opened = m_decoder->initialize(fs, m_frameQueue);
            m_currentStream = fs;
        }
    } catch (const std::exception& e) {
        QString error = QString("Stream open error: %1").arg(e.what());
        emit songLoadError(song, error);
        m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_ERROR,
                                    QVariantHash{{QString("error"), QVariant(error)}});
        return;
    }

    if (!opened) {
        QString error = QString("Failed to open audio stream for: %1").arg(filepath);
        emit songLoadError(song, error);
        m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_ERROR,
                                    QVariantHash{{QString("error"), QVariant(error)}});
        return;
    }

    LOG_DEBUG(" Created new decoder instance for new song");
    // Start decoding to get audio format
    if (!m_decoder->startDecoding(expectedSize)) {
        emit playbackError("Failed to start decoder");
        return;
    }

    // Wait for audio format to be available
    auto formatFuture = m_decoder->getAudioFormatAsync();
    auto fmt = formatFuture.get();
    if (!fmt.isValid()) {
        emit playbackError("Decoder did not provide a valid audio format");
        return;
    }
    LOG_INFO(" Audio format: {} Hz, {} channels, {} bits per sample",
             fmt.sample_rate, fmt.channels, fmt.bits_per_sample);

    // Ensure the audio worker exists and initialize WASAPI on its thread
    m_audioOutput = std::make_unique<WASAPIAudioOutputUnsafe>();
    if (!m_audioOutput->initialize(fmt.sample_rate, fmt.channels, fmt.bits_per_sample)) {
        emit playbackError("Failed to initialize WASAPI audio player");
        return;
    }
    m_audioOutput->start();
    // Start frame transmission thread
    m_frameTransmissionActive.store(true);
    m_frameTransmissionThread = std::make_shared<std::thread>([this]() {
        try {
            this->frameTransmissionLoop();
        } catch (const std::exception& e) {
            LOG_ERROR(" frameTransmissionLoop threw exception: {}", e.what());
            std::terminate();
        } catch (...) {
            LOG_ERROR(" frameTransmissionLoop threw unknown exception");
            std::terminate();
        }
    });

    // Update playback state
    m_currentState = PlaybackState::Playing;
    m_currentPosition = 0;
    if (m_positionTimer->isActive()) {
        LOG_WARN(" Position timer is already active when starting playback");
    }
    // If the timer is already running, it will be stopped and restarted. 
    // This will also change its id().
    m_positionTimer->start();
    
    emit playbackStateChanged(m_currentState);
    emit positionChanged(m_currentPosition);
}

// Unsafe variant: caller MUST hold m_stateMutex and m_componentMutex when calling this.
void AudioPlayerController::cleanPlayResourcesUnsafe()
{
    LOG_DEBUG(" Stopping current song playback");
    // Stop frame transmission thread
    m_frameTransmissionActive.store(false);
    m_playbackWatcherExitFlag.store(true);
    // Stop playback but keep components allocated for reuse
    if (m_playbackWatcherThread) {
        m_playbackWatcherThread->join();
        m_playbackWatcherThread.reset();
        LOG_DEBUG(" Joined playback watcher thread during stop");
    }
    LOG_DEBUG(" Clearing frame queue during stop");
    m_frameQueue->clean();
    if (m_frameTransmissionThread) {
        if (m_frameTransmissionThread->joinable()) {
            m_frameTransmissionThread->join();
        }
        m_frameTransmissionThread.reset();
        LOG_DEBUG(" Joined frame transmission thread during stop");
    }
    if (m_currentStream) {
        LOG_DEBUG(" Releasing current stream reference during stop");
        m_currentStream.reset();
    }
    if (m_audioOutput) {
        m_audioOutput->stop();
        m_audioOutput.reset();
        LOG_DEBUG(" Released audio worker instance during stop");
    }
    if (m_decoder) {
        m_decoder->pauseDecoding(); // Stop decoding
        m_decoder.reset();
        LOG_DEBUG(" Released decoder instance during stop");
    }
    m_positionTimer->stop();    
    m_currentState = PlaybackState::Stopped;
    m_currentPosition = 0;
    
    LOG_INFO(" Playback stopped");
    emit playbackStateChanged(m_currentState);
    emit positionChanged(m_currentPosition);
}
// void AudioPlayerController::onSetVolumeEvent(const QVariantHash&){
// }

// void AudioPlayerController::onSetPlayModeEvent(const QVariantHash&){
// }

// void AudioPlayerController::onUpdatePositionEvent(const QVariantHash&){
// }
} // namespace audio