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
    
    LOG_INFO(" AudioPlayerController initialized with event-driven architecture");
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
    // Avoid recursive locking: release the state mutex before calling
    // playPlaylist(), which acquires the same mutex again.
    playPlaylist(playlistId, startIndex);
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
    // Update position based on decoder progress
    // emit positionChanged(m_currentPosition);
}

void AudioPlayerController::registerEventHandlers()
{

    // Helper to wrap handlers with a thread-check (reduces repeated lambda boilerplate)
    auto makeThreadCheckedHandler = [this](std::function<void(const QVariantHash&)> handler) {
        return [this, handler](const QVariantHash& data) {
            QThread* objThread = this->thread();
            QThread* currentThread = QThread::currentThread();
            if (objThread != currentThread) {
                LOG_WARN("AudioPlayerController event handler called from wrong thread (obj: {}, current: {})",
                            static_cast<void*>(objThread->thread()),
                            static_cast<void*>(currentThread));
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

QString AudioPlayerController::generateStreamingFilepath(const playlist::SongInfo& song) const
{
    // Generate filepath using only platform and args hash for safety and uniqueness
    // song.platform is not an enum type directly; cast it to the actual enum used by NetworkManager
    QString platformStr = QString::fromStdString(std::string(
        magic_enum::enum_name(static_cast<network::SupportInterface>(song.platform))
    ));
    
    // Create a comprehensive hash from multiple song properties for uniqueness
    std::string titleHash = util::md5Hash(song.title.toStdString()),
                uploaderHash = util::md5Hash(song.uploader.toStdString()),
                argsHash = util::md5Hash(song.args.toStdString());
    std::string mixedStr = fmt::format("{}{}{}", titleHash, uploaderHash, argsHash);
    std::string result = util::md5Hash(mixedStr);
    
    // Safe filename format: "platform_hash.audio"
    QString filename = QString("%1_%2.audio").arg(platformStr).arg(QString::fromStdString(result));
    
    // Get configured audio cache directory from ConfigManager if available,
    // otherwise use a local ./tmp directory relative to the executable/current working dir.
    QString cacheDir;
    if (CONFIG_MANAGER) {
        cacheDir = CONFIG_MANAGER->getAudioCacheDirectory();
    } else {
        cacheDir = QDir::cleanPath(QDir::currentPath() + "/tmp");
    }

    // Ensure directory exists
    QDir().mkpath(cacheDir);

    return QDir(cacheDir).filePath(filename);
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
                LOG_WARN(" Failed to transmit audio frame after {} retries - buffer may be stuck", retryCount);
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
        filepath = generateStreamingFilepath(song);
        useStreaming = true;
        
        // Update the song's filepath in the playlist for future use
        playlist::SongInfo updatedSong = song;
        updatedSong.filepath = filepath;
        m_currentPlaylist[m_currentIndex] = updatedSong;
        
        // Also update the song in the persistent playlist manager
        if (PLAYLIST_MANAGER) {
            PLAYLIST_MANAGER->updateSongInPlaylist(updatedSong, m_currentPlaylistId);
        }

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
            auto platform = static_cast<network::SupportInterface>(song.platform);

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