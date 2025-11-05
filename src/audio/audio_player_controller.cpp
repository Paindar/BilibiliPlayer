#include "audio_player_controller.h"
#include "wasapi_audio_output.h"
#include "ffmpeg_decoder.h"
#include <playlist/playlist_manager.h>
#include <network/network_manager.h>
#include <log/log_manager.h>
#include <manager/application_context.h>
#include <config/config_manager.h>
#include <QDir>
#include <chrono>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <algorithm>
#include <fstream>
#include <memory>

AudioPlayerController::AudioPlayerController(QObject *parent)
    : QObject(parent)
    , m_audioPlayer(nullptr)
    , m_decoder(nullptr)
    , m_frameQueue(nullptr)
    , m_frameTransmissionActive(false)
    , m_currentIndex(-1)
    , m_currentState(PlaybackState::Stopped)
    , m_playMode(playlist::PlayMode::PlaylistLoop)
    , m_currentPosition(0)
    , m_totalDuration(0)
    , m_volume(0.8f)
    , m_positionTimer(new QTimer(this))
    , m_randomGenerator(m_randomDevice())
    , m_randomIndex(0)
{
    // Initialize decoder and frame queue
    m_decoder = std::make_unique<FFmpegStreamDecoder>();
    m_frameQueue = std::make_shared<AudioFrameQueue>();
    
    // Setup position timer for progress updates
    m_positionTimer->setInterval(100); // Update every 100ms
    connect(m_positionTimer, &QTimer::timeout, this, &AudioPlayerController::onPositionTimerTimeout);
    
    LOG_INFO("AudioPlayerController initialized");
}

AudioPlayerController::~AudioPlayerController()
{
    // First, stop frame transmission thread to avoid races
    m_frameTransmissionActive.store(false);
    if (m_frameQueue) {
        m_frameQueue->cond_var.notify_all(); // wake up waiting thread
    }
    if (m_frameTransmissionThread.joinable()) {
        m_frameTransmissionThread.join();
    }
    
    // Then stop playback and clean up components
    stop();
    
    LOG_INFO("AudioPlayerController destroyed");
}

void AudioPlayerController::frameTransmissionLoop()
{
    LOG_INFO("Frame transmission thread started");
    
    while (m_frameTransmissionActive.load()) {
        if (!m_frameQueue || !m_audioPlayer || !m_audioPlayer->isInitialized()) {
            // TODO : consider using a more efficient wait mechanism
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        std::shared_ptr<AudioFrame> frame = nullptr;
        {
            std::unique_lock<std::mutex> lock(m_frameQueue->mutex);
            // Wait for frame or stop signal
            m_frameQueue->cond_var.wait(lock, [this]() {
                return !m_frameQueue->frame_queue.empty() || !m_frameTransmissionActive.load();
            });
            
            if (!m_frameTransmissionActive.load()) {
                break;
            }
            
            if (!m_frameQueue->frame_queue.empty()) {
                frame = m_frameQueue->frame_queue.front();
                m_frameQueue->frame_queue.pop();
            }
        }
        
        if (frame && !frame->data.empty()) {
            // Calculate frame size in samples
            int bytesPerSample = 2; // 16-bit = 2 bytes
            int samplesInFrame = frame->data.size() / (frame->channels * bytesPerSample);
            
            // Wait for sufficient buffer space before sending
            bool success = false;
            int retryCount = 0;
            const int maxRetries = 20;
            
            while (!success && retryCount < maxRetries && m_frameTransmissionActive.load()) {
                if (!m_audioPlayer->isInitialized() || !m_audioPlayer->isPlaying()) {
                    // Audio player not ready, wait a bit
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    retryCount++;
                    continue;
                }
                
                // Check if buffer has enough space for this frame
                int availableFrames = m_audioPlayer->getAvailableFrames();
                if (availableFrames >= samplesInFrame) {
                    // Buffer has space, send the frame
                    m_audioPlayer->playAudioData(frame->data.data(), static_cast<int>(frame->data.size()));
                    success = true;
                    LOG_DEBUG("Transmitted audio frame: PTS {}, size {} bytes, samples {}, buffer space {}", 
                             frame->pts, frame->data.size(), samplesInFrame, availableFrames);
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
                LOG_WARN("Failed to transmit audio frame after {} retries - buffer may be stuck", maxRetries);
            }
        }
    }
    
    LOG_INFO("Frame transmission thread stopped");
}

void AudioPlayerController::playPlaylist(const QUuid& playlistId, int startIndex)
{
    
    auto* playlistManager = PLAYLIST_MANAGER;
    if (!playlistManager) {
        emit playbackError("Playlist manager not available");
        return;
    }
    
    // Get playlist songs (can be called outside mutex)
    auto songs = playlistManager->iterateSongsInPlaylist(playlistId, [](const playlist::SongInfo&) { return true; });
    if (songs.isEmpty()) {
        emit playbackError("Playlist is empty");
        return;
    }
    
    // Validate start index
    if (startIndex < 0 || startIndex >= songs.size()) {
        startIndex = 0;
    }
    
    // Now acquire mutex and update state
    std::unique_lock<std::mutex> locker(m_stateMutex);
    
    // Stop current playback
    stopCurrentSongUnsafe();
    
    // Set new playlist
    m_currentPlaylistId = playlistId;
    m_currentPlaylist = songs;
    m_currentIndex = startIndex;
    
    // Setup random order if needed
    if (m_playMode == playlist::PlayMode::Random) {
        setupRandomGenerator();
    }

    LOG_INFO("Starting playlist playback with {} songs, starting at index {}", songs.size(), startIndex);

    // Start playing
    playCurrentSongUnsafe();
}

void AudioPlayerController::playPlaylistFromSong(const QUuid& playlistId, const playlist::SongInfo& startSong)
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    
    auto* playlistManager = PLAYLIST_MANAGER;
    if (!playlistManager) {
        emit playbackError("Playlist manager not available");
        return;
    }
    
    // Get playlist songs
    auto songs = playlistManager->iterateSongsInPlaylist(playlistId, [](const playlist::SongInfo&) { return true; });
    if (songs.isEmpty()) {
        emit playbackError("Playlist is empty");
        return;
    }
    
    // Find the starting song index
    int startIndex = -1;
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
    
    // Avoid recursive locking: release the state mutex before calling
    // playPlaylist(), which acquires the same mutex again.
    locker.unlock();
    playPlaylist(playlistId, startIndex);
}

void AudioPlayerController::setCurrentPlaylist(const QUuid& playlistId)
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    m_currentPlaylistId = playlistId;
}

void AudioPlayerController::clearPlaylist()
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    clearPlaylistUnsafe();
}

// Unsafe variant: caller MUST hold m_stateMutex when calling this.
void AudioPlayerController::clearPlaylistUnsafe()
{
    // Caller must hold m_stateMutex
    stopCurrentSongUnsafe();
    m_currentPlaylistId = QUuid();
    m_currentPlaylist.clear();
    m_currentIndex = -1;
    m_randomPlayOrder.clear();

    emit currentSongChanged(playlist::SongInfo(), -1);
}

void AudioPlayerController::play()
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    
    if (m_currentState == PlaybackState::Paused) {
        // Resume playback
        if (m_decoder) {
            m_decoder->resumeDecoding();
        }
        if (m_audioPlayer) {
            m_audioPlayer->start();
        }
        m_currentState = PlaybackState::Playing;
        m_positionTimer->start();
        
        LOG_INFO("Playback resumed");
        emit playbackStateChanged(m_currentState);
    } else if (m_currentState == PlaybackState::Stopped && !m_currentPlaylist.isEmpty()) {
        // Start playing current song
        playCurrentSongUnsafe();
    }
}

void AudioPlayerController::pause()
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    
    if (m_currentState == PlaybackState::Playing) {
        if (m_decoder) {
            m_decoder->pauseDecoding();
        }
        if (m_audioPlayer) {
            m_audioPlayer->pause();
        }
        
        m_currentState = PlaybackState::Paused;
        m_positionTimer->stop();
        
        LOG_INFO("Playback paused");
        emit playbackStateChanged(m_currentState);
    }
}

void AudioPlayerController::stop()
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    stopCurrentSongUnsafe();
}

void AudioPlayerController::next()
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    
    if (m_currentPlaylist.isEmpty()) {
        return;
    }
    
    loadNextSong();
    playCurrentSongUnsafe();
}

void AudioPlayerController::previous()
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    
    if (m_currentPlaylist.isEmpty()) {
        return;
    }
    
    loadPreviousSong();
    playCurrentSongUnsafe();
}

// void AudioPlayerController::seekTo(qint64 positionMs)
// {
//     // TODO: Implement seeking functionality in new decoder API
//     // The new FFmpegStreamDecoder doesn't yet support seeking
// }

void AudioPlayerController::setPlayMode(playlist::PlayMode mode)
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    
    if (m_playMode != mode) {
        m_playMode = mode;
        
        if (mode == playlist::PlayMode::Random) {
            setupRandomGenerator();
        }

        LOG_INFO("Play mode changed to: {}", static_cast<int>(mode));
        emit playModeChanged(mode);
    }
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
    status.errorMessage = m_lastError;
    
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
    
    volume = qBound(0.0f, volume, 1.0f);
    if (qAbs(m_volume - volume) > 0.01f) {
        m_volume = volume;
        
        // Apply volume to audio player if available
        // Note: WASAPIAudioPlayer would need volume control
        
        emit volumeChanged(m_volume);
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
    // This is a simplified implementation
    std::unique_lock<std::mutex> locker(m_stateMutex);
    
    if (m_currentState == PlaybackState::Playing) {
        m_currentPosition += 100; // Add 100ms
        
        // Check if decoder has completed
        if (m_decoder && m_decoder->isCompleted()) {
            // Check if there are still frames to play
            size_t queueSize = 0;
            if (m_frameQueue) {
                std::lock_guard<std::mutex> queueLock(m_frameQueue->mutex);
                queueSize = m_frameQueue->frame_queue.size();
            }
            
            // LOG_DEBUG("Decoder completed but frame queue has {} frames remaining, position: {}ms", queueSize, m_currentPosition);
            
            // Only finish if queue is also empty
            if (queueSize == 0) {
                // Song finished, move to next
                locker.unlock(); // Release before calling onAudioDecodingFinished (it acquires mutex)
                onAudioDecodingFinished();
                return;
            }
        }
        
        // Clamp to total duration if available
        if (m_totalDuration > 0 && m_currentPosition > m_totalDuration) {
            m_currentPosition = m_totalDuration;
        }
        
        emit positionChanged(m_currentPosition);
    }
}

void AudioPlayerController::onAudioDecodingFinished()
{
    LOG_INFO("Audio decoding finished for current song");
    
    std::unique_lock<std::mutex> locker(m_stateMutex);
    
    // Handle different play modes
    switch (m_playMode) {
    case playlist::PlayMode::SingleLoop:
        // Restart current song
        playCurrentSongUnsafe();
        break;
        
    case playlist::PlayMode::PlaylistLoop:
    case playlist::PlayMode::Random:
        // Move to next song
        loadNextSong();
        if (m_currentIndex >= 0) {
            playCurrentSongUnsafe();
        } else {
            // End of playlist
            locker.unlock(); // Release mutex before calling stop() which acquires it
            stop();
        }
        break;
    }
}

void AudioPlayerController::onAudioDecodingError(const QString& error)
{
    LOG_ERROR("Audio decoding error: {}", error.toStdString());
    
    std::unique_lock<std::mutex> locker(m_stateMutex);
    
    m_lastError = error;
    m_currentState = PlaybackState::Error;
    
    emit playbackError(error);
    emit playbackStateChanged(m_currentState);
    
    // Try to skip to next song
    loadNextSong();
    if (m_currentIndex >= 0) {
        playCurrentSongUnsafe();
    }
}

void AudioPlayerController::playCurrentSong()
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    playCurrentSongUnsafe();
}

void AudioPlayerController::stopCurrentSong()
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    stopCurrentSongUnsafe();
}

// Unsafe variant: caller MUST hold m_stateMutex when calling this.
void AudioPlayerController::stopCurrentSongUnsafe()
{
    // Stop frame transmission thread
    m_frameTransmissionActive.store(false);
    if (m_frameQueue) {
        m_frameQueue->cond_var.notify_all(); // wake up waiting thread
    }
    if (m_frameTransmissionThread.joinable()) {
        m_frameTransmissionThread.join();
    }
    
    if (m_decoder) {
        m_decoder->pauseDecoding(); // Stop decoding
    }

    if (m_currentStream) {
        m_currentStream.reset();
    }
    
    if (m_audioPlayer) {
        m_audioPlayer->stop();
    }
    
    m_positionTimer->stop();
    m_currentState = PlaybackState::Stopped;
    m_currentPosition = 0;
    
    LOG_INFO("Playback stopped");
    emit playbackStateChanged(m_currentState);
    emit positionChanged(m_currentPosition);
}

void AudioPlayerController::loadNextSong()
{
    if (m_currentPlaylist.isEmpty()) {
        m_currentIndex = -1;
        return;
    }
    
    int nextIndex = getNextSongIndex();
    m_currentIndex = nextIndex;
    
    LOG_INFO("Moving to next song, index: {}", nextIndex);
}

void AudioPlayerController::loadPreviousSong()
{
    if (m_currentPlaylist.isEmpty()) {
        m_currentIndex = -1;
        return;
    }
    
    int prevIndex = getPreviousSongIndex();
    m_currentIndex = prevIndex;
    
    LOG_INFO("Moving to previous song, index: {}", prevIndex);
}

int AudioPlayerController::getNextSongIndex()
{
    if (m_currentPlaylist.isEmpty()) {
        return -1;
    }
    
    switch (m_playMode) {
    case playlist::PlayMode::SingleLoop:
        return m_currentIndex; // Stay on same song
        
    case playlist::PlayMode::PlaylistLoop:
        return (m_currentIndex + 1) % m_currentPlaylist.size();
        
    case playlist::PlayMode::Random:
        if (m_randomPlayOrder.isEmpty()) {
            return -1;
        }
        m_randomIndex = (m_randomIndex + 1) % m_randomPlayOrder.size();
        return m_randomPlayOrder[m_randomIndex];
    }
    
    return -1;
}

int AudioPlayerController::getPreviousSongIndex()
{
    if (m_currentPlaylist.isEmpty()) {
        return -1;
    }
    
    switch (m_playMode) {
    case playlist::PlayMode::SingleLoop:
        return m_currentIndex; // Stay on same song
        
    case playlist::PlayMode::PlaylistLoop:
        return (m_currentIndex - 1 + m_currentPlaylist.size()) % m_currentPlaylist.size();
        
    case playlist::PlayMode::Random:
        if (m_randomPlayOrder.isEmpty()) {
            return -1;
        }
        m_randomIndex = (m_randomIndex - 1 + m_randomPlayOrder.size()) % m_randomPlayOrder.size();
        return m_randomPlayOrder[m_randomIndex];
    }
    
    return -1;
}

QString AudioPlayerController::generateStreamingFilepath(const playlist::SongInfo& song) const
{
    // Generate filepath using only platform and args hash for safety and uniqueness
    QString platformStr = QString::number(song.platform);
    
    // Create a comprehensive hash from multiple song properties for uniqueness
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(song.args.toUtf8());
    hash.addData(song.title.toUtf8());
    hash.addData(song.uploader.toUtf8());
    QString fileHash = hash.result().toHex(); // Full MD5 hash for uniqueness
    
    // Safe filename format: "platform_hash.audio"
    QString filename = QString("%1_%2.audio").arg(platformStr).arg(fileHash);
    
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
    
    LOG_INFO("Random play order generated with {} songs", m_randomPlayOrder.size());
}

// Unsafe variant: caller MUST hold m_stateMutex when calling this.
void AudioPlayerController::playCurrentSongUnsafe()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_currentPlaylist.size()) {
        LOG_WARN("Invalid current song index");
        return;
    }
    
    const auto& song = m_currentPlaylist[m_currentIndex];
    LOG_INFO("Playing song: {} by {}", song.title.toStdString(), song.uploader.toStdString());
    
    m_currentState = PlaybackState::Loading;
    emit playbackStateChanged(m_currentState);
    emit currentSongChanged(song, m_currentIndex);
    
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
        auto* playlistManager = PLAYLIST_MANAGER;
        if (playlistManager) {
            playlistManager->updateSongInPlaylist(updatedSong, m_currentPlaylistId);
        }

        LOG_INFO("Using streaming for song: {}", filepath.toStdString());
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
                LOG_WARN("Failed to get expected stream size: {}", e.what());
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
            auto fileStream = std::make_shared<std::ifstream>(filepath.toStdString(), std::ios::binary);
            if (!fileStream->is_open()) {
                throw std::runtime_error("Failed to open local file");
            }
            expectedSize = fileStream->seekg(0, std::ios::end).tellg();
            fileStream->seekg(0, std::ios::beg);
            opened = m_decoder->initialize(fileStream, m_frameQueue);
        }
    } catch (const std::exception& e) {
        QString error = QString("Stream open error: %1").arg(e.what());
        emit songLoadError(song, error);
        onAudioDecodingError(error);
        return;
    }

    if (!opened) {
        QString error = QString("Failed to open audio stream for: %1").arg(filepath);
        emit songLoadError(song, error);
        onAudioDecodingError(error);
        return;
    }

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

    // (Re)create audio player with the correct format
    m_audioPlayer = std::make_unique<WASAPIAudioOutputUnsafe>();
    if (!m_audioPlayer->initialize(fmt.sample_rate, fmt.channels, fmt.bits_per_sample)) {
        emit playbackError("Failed to initialize WASAPI audio player");
        return;
    }
    m_audioPlayer->start();

    // Start frame transmission thread
    m_frameTransmissionActive.store(true);
    // if (m_frameTransmissionThread.joinable()) {
    //     m_frameTransmissionThread.join(); // join previous thread if any
    // }
    m_frameTransmissionThread = std::thread(&AudioPlayerController::frameTransmissionLoop, this);

    // Update playback state
    m_currentState = PlaybackState::Playing;
    m_currentPosition = 0;
    m_positionTimer->start();
    
    emit playbackStateChanged(m_currentState);
    emit positionChanged(m_currentPosition);
}