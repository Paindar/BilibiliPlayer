#include "audio_player_controller.h"

#include <manager/application_context.h>
#include <config/config_manager.h>
#include <log/log_manager.h>
#include <network/network_manager.h>
#include <playlist/playlist_manager.h>
#include <util/md5.h>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QTimer>
#include <QPointer>
#include <chrono>
#include <fmt/format.h>
#include <fstream>
#include <memory>
#include <magic_enum/magic_enum.hpp>
#include "ffmpeg_decoder.h"
#include "wasapi_audio_output.h"

namespace audio {

AudioPlayerController::AudioPlayerController(QObject *parent)
    : QObject(parent)
    , m_eventProcessor(std::make_unique<audio::AudioEventProcessor>(this))
    // Audio components
    , m_audioOutput(nullptr)
    , m_decoder(nullptr)
    , m_currentStream(nullptr)

    // Frame transmission thread (consumes frames from decoder and sends to audio output)
    , m_frameQueue(std::make_shared<AudioFrameQueue>())
    , m_frameTransmissionActive(false)

    // Playlist data
    , m_currentPlaylistId(QUuid())
    , m_currentIndex(-1)
    // Playback state
    , m_currentState(PlaybackState::Stopped)
    , m_playMode(playlist::PlayMode::PlaylistLoop)
    , m_currentPosition(0)
    , m_totalDuration(0)
    , m_volume(0.8f)
    // Position timer
    , m_positionTimer(new QTimer(this))
    , m_randomGenerator(m_randomDevice())
    , m_randomIndex(0)
{
    // Setup event processor
    registerEventHandlers();
    m_eventProcessor->start();
    m_positionTimer->setInterval(500); // Update position every 500 ms
    
    LOG_INFO(" AudioPlayerController initialized with event-driven architecture");
    connect(m_positionTimer, &QTimer::timeout, this, &AudioPlayerController::onPositionTimerTimeout);
    connect(PLAYLIST_MANAGER, &PlaylistManager::playlistSongsChanged, this, &AudioPlayerController::onPlaylistChanged);
}

AudioPlayerController::~AudioPlayerController()
{
    // Stop sending frame into audio player.
    m_frameTransmissionActive.store(false);
    // Clean frame queue.
    m_frameQueue->clean();
    if (m_frameTransmissionThread && m_frameTransmissionThread->joinable()) {
        m_frameTransmissionThread->join();
    }

    std::unique_lock<std::mutex> locker(m_componentMutex);
    // Free audio player
    if (m_audioOutput) {
        m_audioOutput->stop();
        m_audioOutput.reset();
    }

    // Free decoder
    if (m_decoder) {
        m_decoder->pauseDecoding();
        m_decoder.reset();
    }
    // Free input stream
    if (m_currentStream) {
        m_currentStream->clear();
        m_currentStream.reset();
    }

    if (m_playbackWatcherThread && m_playbackWatcherThread->joinable()) {
        m_playbackWatcherExitFlag.store(true);
        m_playbackWatcherThread->join();
    }
    
    LOG_INFO(" AudioPlayerController destroyed");
}

void AudioPlayerController::saveConfig()
{
    if (!CONFIG_MANAGER) {
        return;
    }
    CONFIG_MANAGER->setVolumeLevel(static_cast<int>(m_volume * 100));
    CONFIG_MANAGER->setPlayMode(static_cast<int>(m_playMode));
    CONFIG_MANAGER->setCurrentPlaylistId(m_currentPlaylistId.toString());
    CONFIG_MANAGER->setCurrentAudioIndex(m_currentIndex);
}

void AudioPlayerController::loadConfig()
{
    if (!CONFIG_MANAGER) {
        return;
    }
    int volumeLevel = CONFIG_MANAGER->getVolumeLevel();
    setVolume(static_cast<float>(volumeLevel) / 100.0f);
    
    int playModeInt = CONFIG_MANAGER->getPlayMode();
    if (auto mode = magic_enum::enum_cast<playlist::PlayMode>(playModeInt)) {
        setPlayMode(*mode);
    }

    QString playlistIdStr = CONFIG_MANAGER->getCurrentPlaylistId();
    QUuid playlistId(playlistIdStr);
    int audioIndex = CONFIG_MANAGER->getCurrentAudioIndex();
    setCurrentPlaylist(playlistId, audioIndex);
}

void AudioPlayerController::playPlaylist(const QUuid& playlistId, int startIndex)
{
    if (!PLAYLIST_MANAGER) {
        emit playbackError("Playlist manager not available");
        return;
    }
    
    // Get playlist songs
    auto songs = PLAYLIST_MANAGER->iterateSongsInPlaylist(playlistId, [](const playlist::SongInfo&) { return true; });
    if (songs.isEmpty()) {
        emit playbackError("Playlist is empty");
        return;
    }
    
    // Validate start index
    if (startIndex < 0 || startIndex >= songs.size()) {
        startIndex = 0;
    }

    // Set new playlist
    std::scoped_lock locker(m_stateMutex);
    m_currentPlaylistId = playlistId;
    m_currentPlaylist = songs;
    m_currentIndex = startIndex;
    
    // Setup random order if needed
    if (m_playMode == playlist::PlayMode::Random) {
        setupRandomGenerator();
    }

    LOG_INFO(" Starting playlist playback with {} songs, starting at index {}", songs.size(), startIndex);
    emit currentSongChanged(m_currentPlaylist[m_currentIndex], m_currentIndex);
    // Send Start event to event processor
    m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAY);
}

void AudioPlayerController::playPlaylistFromSong(const QUuid& playlistId, const playlist::SongInfo& startSong)
{
    if (!PLAYLIST_MANAGER) {
        emit playbackError("Playlist manager not available");
        return;
    }
    
    // Get playlist songs
    auto songs = PLAYLIST_MANAGER->iterateSongsInPlaylist(playlistId, [](const playlist::SongInfo&) { return true; });
    if (songs.isEmpty()) {
        emit playbackError("Playlist is empty");
        return;
    }

    int startIndex = -1;
    {
        std::scoped_lock locker(m_stateMutex);
        
        // Find the starting song index
        for (int i = 0; i < songs.size(); ++i) {
            if (songs[i] == startSong) {
                startIndex = i;
                break;
            }
        }
        
        if (startIndex == -1) {
            emit playbackError("Starting song not found in playlist");
            return;
        }
    }
    // Set new playlist
    std::scoped_lock locker(m_stateMutex);
    m_currentPlaylistId = playlistId;
    m_currentPlaylist = songs;
    m_currentIndex = startIndex;
    // Setup random order if needed
    if (m_playMode == playlist::PlayMode::Random) {
        setupRandomGenerator();
    }

    LOG_INFO(" Starting playlist playback with {} songs, starting at index {}", songs.size(), startIndex);
    emit currentSongChanged(m_currentPlaylist[m_currentIndex], m_currentIndex);
    // Send Start event to event processor
    m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAY);
}

void AudioPlayerController::setCurrentPlaylist(const QUuid &playlistId, const int startIndex)
{
    if (!PLAYLIST_MANAGER) {
        emit playbackError("Playlist manager not available");
        return;
    }
    
    // Get playlist songs
    auto songs = PLAYLIST_MANAGER->iterateSongsInPlaylist(playlistId, [](const playlist::SongInfo&) { return true; });
    
    std::scoped_lock locker(m_stateMutex);
    if (songs.isEmpty()) {
        m_currentIndex = -1;
        return;
    } else {
        m_currentPlaylistId = playlistId;
        m_currentPlaylist = songs;
        if (startIndex < 0 || startIndex >= songs.size()) {
            m_currentIndex = 0;
        } else {
            m_currentIndex = startIndex;
        }
    }
    emit currentSongChanged(m_currentPlaylist[m_currentIndex], m_currentIndex);
}

QUuid AudioPlayerController::getCurrentPlaylistId() const
{
    std::scoped_lock locker(m_stateMutex);
    return m_currentPlaylistId;
}

int AudioPlayerController::getCurrentAudioIndex() const
{
    std::scoped_lock locker(m_stateMutex);
    return m_currentIndex;
}

void AudioPlayerController::clearPlaylist()
{
    // Caller must hold m_stateMutex
    std::unique_lock<std::mutex> locker(m_stateMutex);
    m_currentPlaylistId = QUuid();
    m_currentPlaylist.clear();
    m_currentIndex = -1;
    m_randomPlayOrder.clear();

    m_eventProcessor->postEvent(audio::AudioEventProcessor::STOP);
    emit currentSongChanged(playlist::SongInfo(), -1);
}

void AudioPlayerController::play()
{
    m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAY);
}

void AudioPlayerController::pause()
{
    m_eventProcessor->postEvent(audio::AudioEventProcessor::PAUSE);
}

void AudioPlayerController::stop()
{
    m_eventProcessor->postEvent(audio::AudioEventProcessor::STOP);
}

void AudioPlayerController::next()
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    if (m_currentState == PlaybackState::Playing) {
        m_eventProcessor->postEvent(audio::AudioEventProcessor::STOP);
    }
    if (m_currentPlaylist.isEmpty()) {
        m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_ERROR, QVariantHash{
            {"error", "Failed to play next song: Playlist is empty"}
        });
        return;
    }
    
    switch (m_playMode) {
    case playlist::PlayMode::SingleLoop:
        break;
    case playlist::PlayMode::PlaylistLoop:
        m_currentIndex = (m_currentIndex + 1) % m_currentPlaylist.size();
        break;
    case playlist::PlayMode::Random:
        if (m_randomPlayOrder.isEmpty()) {
            m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_ERROR, QVariantHash{
                {"error", "Failed to play next song: Random play order is empty"}
            });
            return;
        }
        break;
        m_randomIndex = (m_randomIndex + 1) % m_randomPlayOrder.size();
    }
    emit currentSongChanged(m_currentPlaylist[m_currentIndex], m_currentIndex);
    m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAY);
}

void AudioPlayerController::previous()
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    if (m_currentState == PlaybackState::Playing) {
        m_eventProcessor->postEvent(audio::AudioEventProcessor::STOP);
    }
    if (m_currentPlaylist.isEmpty()) {
        m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_ERROR, QVariantHash{
            {"error", "Failed to play previous song: Playlist is empty"}
        });
        return;
    }
    
    switch (m_playMode) {
    case playlist::PlayMode::SingleLoop:
        break;
    case playlist::PlayMode::PlaylistLoop:
        m_currentIndex = (m_currentIndex - 1 + m_currentPlaylist.size()) % m_currentPlaylist.size();
        
    case playlist::PlayMode::Random:
        if (m_randomPlayOrder.isEmpty()) {
            m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_ERROR, QVariantHash{
                {"error", "Failed to play previous song: Random play order is empty"}
            });
            return;
        }
        m_randomIndex = (m_randomIndex - 1 + m_randomPlayOrder.size()) % m_randomPlayOrder.size();
        break;
    }
    
    emit currentSongChanged(m_currentPlaylist[m_currentIndex], m_currentIndex);
    m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAY);
}

// void AudioPlayerController::seekTo(qint64 positionMs)
// {
//     // TODO: Implement seeking functionality in new decoder API
//     // The new FFmpegStreamDecoder doesn't yet support seeking
// }

void AudioPlayerController::setPlayMode(playlist::PlayMode mode)
{
    std::scoped_lock locker(m_stateMutex);
    m_playMode = mode;
    emit playModeChanged(mode);
}

playlist::PlayMode AudioPlayerController::getPlayMode() const
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    return m_playMode;
}

PlaybackStatus AudioPlayerController::getStatus() const
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    
    PlaybackStatus status;
    status.state = m_currentState;
    status.playMode = m_playMode;
    status.currentPosition = m_currentPosition;
    status.totalDuration = m_totalDuration;
    status.currentIndex = m_currentIndex;
    status.totalSongs = m_currentPlaylist.size();
    
    if (m_currentIndex >= 0 && m_currentIndex < m_currentPlaylist.size()) {
        status.currentSong = m_currentPlaylist[m_currentIndex];
    }
    
    return status;
}

bool AudioPlayerController::isPlaying() const
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    return m_currentState == PlaybackState::Playing;
}

bool AudioPlayerController::isPaused() const
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    return m_currentState == PlaybackState::Paused;
}

bool AudioPlayerController::isStopped() const
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    return m_currentState == PlaybackState::Stopped;
}

void AudioPlayerController::setVolume(float volume)
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    m_volume = std::clamp(volume, 0.0f, 1.0f);
    
    if (m_audioOutput) {
        m_audioOutput->setVolume(m_volume);
    }
}

float AudioPlayerController::getVolume() const
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    return m_volume;
}

void AudioPlayerController::onPositionTimerTimeout()
{
    // Update playback position using audio output's device clock (IAudioClock)
    std::scoped_lock locker(m_stateMutex, m_componentMutex);
    if (m_audioOutput && m_audioOutput->isInitialized() && m_audioOutput->isPlaying()) {
        int64_t posMs = m_audioOutput->getCurrentPositionMs();
        // Only update and emit if changed to avoid excessive signals
        if (posMs != m_currentPosition) {
            m_currentPosition = posMs;
            emit positionChanged(m_currentPosition);
        }
    } else {
        // Fallback: emit current cached position
        emit positionChanged(m_currentPosition);
    }
}

void AudioPlayerController::onPlaylistChanged(const QUuid &playlistId)
{
    std::scoped_lock locker(m_stateMutex);
    if (playlistId != m_currentPlaylistId) {
        return;
    }
    if (m_currentPlaylist.isEmpty()) {
        m_currentPlaylistId = playlistId;
        m_currentPlaylist = PLAYLIST_MANAGER->iterateSongsInPlaylist(playlistId, [](const playlist::SongInfo&) { return true; });
        m_currentIndex = m_currentPlaylist.isEmpty() ? -1 : 0;
        return;
    }

    auto song = m_currentPlaylist[m_currentIndex];
    m_currentPlaylist = PLAYLIST_MANAGER->iterateSongsInPlaylist(playlistId, [](const playlist::SongInfo&) { return true; });
    // Try to find the previous song in the updated playlist
    m_currentIndex = -1;
    for (int i = 0; i < m_currentPlaylist.size(); ++i) {
        if (m_currentPlaylist[i] == song) {
            m_currentIndex = i;
            break;
        }
    }
}

void AudioPlayerController::registerEventHandlers()
{

    // Helper to wrap handlers with a thread-check (reduces repeated lambda boilerplate)
    auto makeThreadCheckedHandler = [this](std::function<void(const QVariantHash&)> handler) {
        return [this, handler](const QVariantHash& data) {
            QThread* objThread = this->thread();
            QThread* currentThread = QThread::currentThread();
            if (objThread != currentThread) {
                // This is expected: AudioEventProcessor runs in a worker thread and posts events to handlers.
                // Marshal the handler to the controller thread instead of warning at runtime.
                // LOG_DEBUG("AudioPlayerController event handler invoked from different thread (obj: {}, current: {}) - marshaling to controller thread",
                //             static_cast<void*>(objThread->thread()),
                //             static_cast<void*>(currentThread));
                // Marshal the handler to this object's thread to avoid QObject/QTimer usage from other threads.
                QPointer<AudioPlayerController> self(this);
                if (self) {
                    // Use QMetaObject::invokeMethod overload that accepts a functor (Qt 5.10+/Qt6) to post to the object's thread.
                    QMetaObject::invokeMethod(self.data(), [handler, data]() mutable {
                        handler(data);
                    }, Qt::QueuedConnection);
                }
                return;
            }
            handler(data);
        };
    };

    m_eventProcessor->setEventHandler(audio::AudioEventProcessor::PLAY,
        makeThreadCheckedHandler([this](const QVariantHash& data){ onPlayEvent(data); }));
    m_eventProcessor->setEventHandler(audio::AudioEventProcessor::PAUSE,
        makeThreadCheckedHandler([this](const QVariantHash& data){ onPauseEvent(data); }));
    m_eventProcessor->setEventHandler(audio::AudioEventProcessor::STOP,
        makeThreadCheckedHandler([this](const QVariantHash& data){ onStopEvent(data); }));
    m_eventProcessor->setEventHandler(audio::AudioEventProcessor::PLAYBACK_COMPLETE,
        makeThreadCheckedHandler([this](const QVariantHash& data){ onPlayFinishedEvent(data); }));
    m_eventProcessor->setEventHandler(audio::AudioEventProcessor::PLAYBACK_ERROR,
        makeThreadCheckedHandler([this](const QVariantHash& data){ onPlayErrorEvent(data); }));
    // Additional event handlers (also marshaled where needed)
    m_eventProcessor->setEventHandler(audio::AudioEventProcessor::STREAM_READ_COMPLETED,
        makeThreadCheckedHandler([this](const QVariantHash& data){ onInputStreamReadCompletedEvent(data); }));
    m_eventProcessor->setEventHandler(audio::AudioEventProcessor::STREAM_READ_ERROR,
        makeThreadCheckedHandler([this](const QVariantHash& data){ onInputStreamReadErrorEvent(data); }));
    m_eventProcessor->setEventHandler(audio::AudioEventProcessor::DECODING_FINISHED,
        makeThreadCheckedHandler([this](const QVariantHash& data){ onDecodingFinishedEvent(data); }));
    m_eventProcessor->setEventHandler(audio::AudioEventProcessor::DECODING_ERROR,
        makeThreadCheckedHandler([this](const QVariantHash& data){ onDecodingErrorEvent(data); }));
    // Frame transmission events are marshaled to ensure they run on the desired thread;
    // wrap the marshaled handler in the same thread-checked helper to keep diagnostics consistent.
    m_eventProcessor->setEventHandler(audio::AudioEventProcessor::FRAME_TRANSMISSION_COMPLETED,
        makeThreadCheckedHandler([this](const QVariantHash& data){ onFrameTransmissionFinishedEvent(data); }));
    m_eventProcessor->setEventHandler(audio::AudioEventProcessor::FRAME_TRANSMISSION_ERROR,
        makeThreadCheckedHandler([this](const QVariantHash& data){ onFrameTransmissionErrorEvent(data); }));
}

bool AudioPlayerController::isLocalFileAvailable(const playlist::SongInfo& song) const
{
    if (song.filepath.isEmpty()) {
        return false;
    }
    
    QFileInfo fileInfo(song.filepath);
    return fileInfo.exists() && fileInfo.isFile() && fileInfo.isReadable();
}

void AudioPlayerController::setupRandomGenerator()
{
    if (m_currentPlaylist.isEmpty()) {
        return;
    }
    
    // Create shuffled order of indices
    m_randomPlayOrder.clear();
    for (int i = 0; i < m_currentPlaylist.size(); ++i) {
        m_randomPlayOrder.append(i);
    }
    
    // Shuffle using modern C++ random
    std::shuffle(m_randomPlayOrder.begin(), m_randomPlayOrder.end(), m_randomGenerator);
    
    // Set current position in random order
    m_randomIndex = 0;
    if (m_currentIndex >= 0) {
        // Find current song in random order
        for (int i = 0; i < m_randomPlayOrder.size(); ++i) {
            if (m_randomPlayOrder[i] == m_currentIndex) {
                m_randomIndex = i;
                break;
            }
        }
    }
    
    LOG_INFO(" Random play order generated with {} songs", m_randomPlayOrder.size());
}

void AudioPlayerController::frameTransmissionLoop()
{
    LOG_INFO(" Frame transmission thread started");
    
    while (m_frameTransmissionActive.load()) {
        {
            std::unique_lock<std::mutex> comMutex(m_componentMutex);
            if (!m_audioOutput || !m_audioOutput->isInitialized()) {
                LOG_ERROR(" Audio output not initialized in frame transmission loop, exiting");
                m_frameTransmissionActive.store(false);
                break;
            }
        }
        
        
        std::shared_ptr<AudioFrame> frame = nullptr;
        {
            if (!m_frameTransmissionActive.load()) {
                break;
            }
            if (m_frameQueue->empty()) {
                if (m_decoder == nullptr || m_decoder->isCompleted()) // Decoder has been destroyed, exit loop
                {
                    LOG_DEBUG(" Decoder is null, exiting transmission loop");
                    std::unique_lock<std::mutex> comMutex(m_componentMutex);
                    
                    if (m_playbackWatcherThread) {
                        LOG_ERROR(" try start new playbackWatcherThread while existing one present, exiting");
                        m_eventProcessor->postEvent(AudioEventProcessor::FRAME_TRANSMISSION_ERROR,  QVariantHash{
                                {"error", "Playback watcher thread already exists"}
                            });
                        break;
                    }
                    m_playbackWatcherExitFlag.store(false);
                    m_playbackWatcherThread = std::make_unique<std::thread>([this]() { 
                        try { this->playbackWatcherFunc(); } 
                        catch (...) { 
                            LOG_ERROR(" Exception in playbackWatcherFunc"); 
                        }
                    });
                    m_eventProcessor->postEvent(AudioEventProcessor::FRAME_TRANSMISSION_COMPLETED);
                    break;
                }
                // Wait for new frames to be available
            }
            m_frameQueue->waitAndDequeue(frame);
        }
        
        // If no frame available, and no frames in audioPlayer buffer, consider playback complete
        if (frame && !frame->data.empty()) {
            // Calculate frame size in samples
            int bytesPerSample = frame->bits_per_sample / 8; 
            int samplesInFrame = frame->data.size() / (frame->channels * bytesPerSample);
            // LOG_DEBUG(" Preparing to transmit audio frame: PTS {}, size {} bytes, samples {}, duration {:.2f} ms", 
            //           frame->pts, frame->data.size(), samplesInFrame, frame->durationSeconds());
            
            // Wait for sufficient buffer space before sending
            bool success = false;
            int retryCount = 0;
            const int maxRetries = 20;
            
            while (!success && retryCount < maxRetries && m_frameTransmissionActive.load()) {
                if (!m_audioOutput->isInitialized() || !m_audioOutput->isPlaying()) {
                    // Audio worker not ready, wait a bit
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    retryCount++;
                    continue;
                }

                // Check if buffer has enough space for this frame
                int availableFrames = m_audioOutput->getAvailableFrames();
                if (availableFrames >= samplesInFrame) {
                    // Buffer has space, send the frame
                    std::unique_lock<std::mutex> comMutex(m_componentMutex);
                    m_audioOutput->playAudioData(frame->data.data(), static_cast<int>(frame->data.size()));
                    success = true;
                    // LOG_DEBUG("Transmitted audio frame: PTS {}, size {} bytes, samples {}, buffer space {}", 
                    //           frame->pts, frame->data.size(), samplesInFrame, availableFrames);
                } else {
                    // Buffer is full, wait for consumption
                    // Wait time based on how long it takes to consume remaining buffer
                    double bufferDurationMs = (static_cast<double>(availableFrames) / frame->sample_rate) * 1000.0 * 0.5; // Wait for half buffer to clear
                    int waitMs = std::max(1, std::min(10, static_cast<int>(bufferDurationMs))); // Clamp between 1-10ms
                    std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
                    retryCount++;
                }
            }
            
            if (!success) {
                auto currentState = PlaybackState::Stopped;
                {
                    std::scoped_lock locker(m_stateMutex);
                    currentState = m_currentState;
                }
                if (currentState != PlaybackState::Playing) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } else {
                    LOG_WARN(" Failed to transmit audio frame after {} retries - buffer may be stuck", retryCount);
                }
                
                
            }
        } 
    }
    
    LOG_INFO(" Frame transmission thread stopped");
    
}

void AudioPlayerController::playbackWatcherFunc()
{
    LOG_DEBUG(" Playback watcher thread started");
    while(m_playbackWatcherExitFlag.load() == false) {
        LOG_DEBUG(" Playback watcher checking audio player state");
        int currentFrames = 0;
        {
            std::scoped_lock locker(m_componentMutex);
            if (!m_audioOutput) {
                LOG_INFO(" Playback watcher detected no audio worker, exiting");
                m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_COMPLETE);
                break;
            }
            currentFrames = m_audioOutput->getCurrentFrames();
        }
        LOG_DEBUG(" Playback watcher found current frames in audio player buffer: {}", currentFrames);   
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (currentFrames == 0) {
            LOG_INFO(" Playback watcher detected audio player buffer empty, signaling playback complete");
            m_eventProcessor->postEvent(audio::AudioEventProcessor::PLAYBACK_COMPLETE);
            break;
        }
    }
    LOG_DEBUG(" Playback watcher thread exiting");
}
} // namespace audio