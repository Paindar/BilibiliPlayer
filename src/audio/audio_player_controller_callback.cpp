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
    // Handle quick transitions under lock, then perform the heavy work and emission outside.
    PlayOperationResult opResult{PlayOperationResult::Success, QString{}};
    PlaybackState stateCopy = m_currentState;
    // Data needed for potential error signal (song)
    playlist::SongInfo songCopy;
    int songIndex = -1;
    bool shouldStartPlayback = false;

    // Scoped checks: perform only quick checks while holding locks, capture song/index to operate on.
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
                stateCopy = m_currentState;
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
            {
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
            }

            // Capture song/index for starting playback outside the lock
            songCopy = m_currentPlaylist[m_currentIndex];
            songIndex = m_currentIndex;
            shouldStartPlayback = true;
            break;
        default:
            LOG_ERROR(" Unknown playback state: {}", magic_enum::enum_name(m_currentState));
        }
    }

    // If we need to start playback, do it now outside the controller locks
    if (shouldStartPlayback) {
        opResult = playCurrentSongUnsafe(songCopy, songIndex);
        std::scoped_lock locker(m_stateMutex);
        stateCopy = m_currentState;
    }

    // Emit signals outside of locks
    if (opResult.kind == PlayOperationResult::SongLoadError) {
        emit songLoadError(songCopy, opResult.message);
        m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_ERROR,
                                    QVariantHash{{QStringLiteral("error"), QVariant(opResult.message)}});
    } else if (opResult.kind == PlayOperationResult::PlaybackError) {
        emit playbackError(opResult.message);
        m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_ERROR,
                                    QVariantHash{{QStringLiteral("error"), QVariant(opResult.message)}});
    }

    // Emit state change (if any)
    emit playbackStateChanged(stateCopy);
}

void AudioPlayerController::onPauseEvent(const QVariantHash&)
{
    PlaybackState stateCopy;
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
        stateCopy = m_currentState;
    }

    // Emit outside lock
    emit playbackStateChanged(stateCopy);
}

void AudioPlayerController::onStopEvent(const QVariantHash&)
{
    // Call cleanup outside locks. cleanPlayResourcesUnsafe will acquire locks internally as needed.
    PlayOperationResult res = cleanPlayResources();

    // Read state for emission
    PlaybackState stateCopy;
    qint64 posCopy = 0;
    {
        std::scoped_lock locker(m_stateMutex);
        stateCopy = m_currentState;
        posCopy = m_currentPosition;
    }

    // Emit after cleanup
    emit playbackStateChanged(stateCopy);
    emit positionChanged(posCopy);

    if (res.kind == PlayOperationResult::PlaybackError) {
        emit playbackError(res.message);
    }
}

void AudioPlayerController::onPlayFinishedEvent(const QVariantHash &)
{
    // Perform cleanup outside locks
    PlayOperationResult res = cleanPlayResources();

    // Read state for emission
    PlaybackState stateCopy;
    {
        std::scoped_lock locker(m_stateMutex);
        stateCopy = m_currentState;
    }

    // Emit outside lock
    emit playbackStateChanged(stateCopy);
    emit playbackCompleted();

    if (res.kind == PlayOperationResult::PlaybackError) {
        emit playbackError(res.message);
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

// Only can be called from onPlayEvent; expects caller to have captured song/index and calls this outside locks.
PlayOperationResult AudioPlayerController::playCurrentSongUnsafe(const playlist::SongInfo& song, int index)
{
    LOG_INFO(" Playing song: {} by {}", song.title.toStdString(), song.uploader.toStdString());

    QString filepath = song.filepath;
    bool useStreaming = false;
    uint64_t expectedSize = 0;
    if (filepath.isEmpty() || !isLocalFileAvailable(song)) {
        useStreaming = true;
        LOG_INFO(" Using streaming for song: {}", song.title.toStdString());
    }

    // Create local instances for heavy resources so we don't hold controller locks while initializing them.
    std::unique_ptr<FFmpegStreamDecoder> newDecoder = std::make_unique<FFmpegStreamDecoder>();
    if (newDecoder && m_eventProcessor) {
        newDecoder->setEventProcessor(m_eventProcessor);
    }
    std::shared_ptr<std::istream> newStream;
    std::shared_ptr<WASAPIAudioOutputUnsafe> newAudioOutput;

    bool opened = false;
    try {
        if (useStreaming) {
            auto platform = static_cast<network::PlatformType>(song.platform);
            auto sizeFuture = NETWORK_MANAGER->getStreamSizeByParamsAsync(platform, song.args);
            auto streamFuture = NETWORK_MANAGER->getAudioStreamAsync(platform, song.args, filepath);

            try {
                expectedSize = sizeFuture.get();
            } catch (std::exception& e) {
                LOG_WARN(" Failed to get expected stream size: {}", e.what());
                expectedSize = 0;
            }

            newStream = streamFuture.get();
            if (!newStream) {
                throw std::runtime_error("Failed to acquire streaming input");
            }
            opened = newDecoder->initialize(newStream, m_frameQueue);
        } else {
            auto fs = std::make_shared<std::ifstream>(filepath.toStdString(), std::ios::binary);
            if (!fs->is_open()) {
                throw std::runtime_error("Failed to open local file");
            }
            expectedSize = fs->seekg(0, std::ios::end).tellg();
            fs->seekg(0, std::ios::beg);
            opened = newDecoder->initialize(fs, m_frameQueue);
            newStream = fs;
        }
    } catch (const std::exception& e) {
        QString error = QString("Stream open error: %1").arg(e.what());
        m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_ERROR,
                                    QVariantHash{{QStringLiteral("error"), QVariant(error)}});
        return PlayOperationResult{PlayOperationResult::SongLoadError, error};
    }

    if (!opened) {
        QString error = QString("Failed to open audio stream for: %1").arg(filepath);
        m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_ERROR,
                                    QVariantHash{{QStringLiteral("error"), QVariant(error)}});
        return PlayOperationResult{PlayOperationResult::SongLoadError, error};
    }

    LOG_DEBUG(" Created new decoder instance for new song");
    if (!newDecoder->startDecoding(expectedSize)) {
        QString error = QStringLiteral("Failed to start decoder");
        return PlayOperationResult{PlayOperationResult::PlaybackError, error};
    }

    auto formatFuture = newDecoder->getAudioFormatAsync();
    auto fmt = formatFuture.get();
    if (!fmt.isValid()) {
        QString error = QStringLiteral("Decoder did not provide a valid audio format");
        return PlayOperationResult{PlayOperationResult::PlaybackError, error};
    }
    LOG_INFO(" Audio format: {} Hz, {} channels, {} bits per sample",
             fmt.sample_rate, fmt.channels, fmt.bits_per_sample);

    newAudioOutput = std::make_shared<WASAPIAudioOutputUnsafe>();
    if (!newAudioOutput->initialize(fmt.sample_rate, fmt.channels, fmt.bits_per_sample)) {
        QString error = QStringLiteral("Failed to initialize WASAPI audio player");
        return PlayOperationResult{PlayOperationResult::PlaybackError, error};
    }

    // Commit initialized resources into controller state under lock, then start runtime threads.
    {
        std::scoped_lock locker(m_stateMutex, m_componentMutex);
        m_decoder = std::move(newDecoder);
        m_currentStream = newStream;
        m_audioOutput = std::move(newAudioOutput);
        m_currentState = PlaybackState::Playing;
        m_currentPosition = 0;
        m_frameTransmissionActive.store(true);
        m_audioOutput->start();
        m_positionTimer->start();
        // Start frame transmission thread now that members are in place
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
    }

    // Notify success to any monitoring systems via event processor if needed
    return PlayOperationResult{PlayOperationResult::Success, QString{}};
}

// Unsafe variant: caller MUST hold m_stateMutex and m_componentMutex when calling this.
PlayOperationResult AudioPlayerController::cleanPlayResources()
{
    LOG_DEBUG(" Stopping current song playback");

    // Snapshot and clear controller-owned resources under lock, then perform blocking cleanup outside lock.
    std::shared_ptr<std::thread> playbackWatcherThreadLocal;
    std::shared_ptr<std::thread> frameTransmissionThreadLocal;
    std::shared_ptr<std::istream> currentStreamLocal;
    std::shared_ptr<AudioFrameQueue> frameQueueLocal;
    std::shared_ptr<WASAPIAudioOutputUnsafe> audioOutputLocal;
    std::unique_ptr<FFmpegStreamDecoder> decoderLocal;

    {
        std::scoped_lock locker(m_stateMutex, m_componentMutex);
        m_frameTransmissionActive.store(false);
        m_playbackWatcherExitFlag.store(true);

        playbackWatcherThreadLocal = std::move(m_playbackWatcherThread);
        frameTransmissionThreadLocal = std::move(m_frameTransmissionThread);
        currentStreamLocal = std::move(m_currentStream);
        audioOutputLocal = std::move(m_audioOutput);
        decoderLocal = std::move(m_decoder);
        frameQueueLocal = m_frameQueue;

        m_frameQueue = std::make_shared<AudioFrameQueue>();
        m_currentPosition = 0;
        m_currentState = PlaybackState::Stopped;
    }

    // Perform blocking operations outside the lock
    if (playbackWatcherThreadLocal && playbackWatcherThreadLocal->joinable()) {
        playbackWatcherThreadLocal->join();
        LOG_DEBUG(" Joined playback watcher thread during stop");
    }

    if (frameQueueLocal) {
        frameQueueLocal->clean();
        LOG_DEBUG(" Cleared frame queue during stop");
    }

    if (frameTransmissionThreadLocal && frameTransmissionThreadLocal->joinable()) {
        frameTransmissionThreadLocal->join();
        LOG_DEBUG(" Joined frame transmission thread during stop");
    }

    if (currentStreamLocal) {
        currentStreamLocal.reset();
        LOG_DEBUG(" Released current stream reference during stop");
    }

    if (audioOutputLocal) {
        audioOutputLocal->stop();
        audioOutputLocal.reset();
        LOG_DEBUG(" Released audio worker instance during stop");
    }

    if (decoderLocal) {
        decoderLocal->pauseDecoding();
        decoderLocal.reset();
        LOG_DEBUG(" Released decoder instance during stop");
    }

    if (m_positionTimer) {
        m_positionTimer->stop();
    }

    LOG_INFO(" Playback stopped");
    return PlayOperationResult{PlayOperationResult::Success, QString{}};
}
// void AudioPlayerController::onSetVolumeEvent(const QVariantHash&){
// }

// void AudioPlayerController::onSetPlayModeEvent(const QVariantHash&){
// }

// void AudioPlayerController::onUpdatePositionEvent(const QVariantHash&){
// }
} // namespace audio