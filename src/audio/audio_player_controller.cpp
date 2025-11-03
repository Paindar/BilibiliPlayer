#include "audio_player_controller.h"
#include "wasapi_audio_player.h"
#include "ffmpeg_decoder.h"
#include "../playlist/playlist_manager.h"
#include "../network/network_manager.h"
#include "../log/log_manager.h"
#include "../manager/application_context.h"
#include "../config/config_manager.h"
#include "../stream/streaming_audio_buffer.h"
#include <QDir>
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
    // Initialize decoder; audio player will be created after we know the format
    m_decoder = std::make_unique<FFmpegStreamDecoder>();
    
    // Setup position timer for progress updates
    m_positionTimer->setInterval(100); // Update every 100ms
    connect(m_positionTimer, &QTimer::timeout, this, &AudioPlayerController::onPositionTimerTimeout);
    
    // Setup decoder callbacks
    m_decoder->setAudioCallback([this](const uint8_t* data, int size, int sample_rate, int channels) {
        if (m_audioPlayer && m_audioPlayer->isInitialized()) {
            m_audioPlayer->playAudioData(data, size);
        }
    });
    
    LOG_INFO("AudioPlayerController initialized");
}

AudioPlayerController::~AudioPlayerController()
{
    stop();
    LOG_INFO("AudioPlayerController destroyed");
}


/**
 * @note This method need to hold m_stateMutex.
 */
void AudioPlayerController::playPlaylist(const QUuid& playlistId, int startIndex)
{
    
    auto* playlistManager = PLAYLIST_MANAGER;
    if (!playlistManager) {
        emit playbackError("Playlist manager not available");
        return;
    }
    
    // Get playlist songs
    std::unique_lock<std::mutex> locker(m_stateMutex);
    auto songs = playlistManager->iterateSongsInPlaylist(playlistId, [](const playlist::SongInfo&) { return true; });
    if (songs.isEmpty()) {
        emit playbackError("Playlist is empty");
        return;
    }
    
    // Validate start index
    if (startIndex < 0 || startIndex >= songs.size()) {
        startIndex = 0;
    }
    
    // Stop current playback
    stopCurrentSong();
    
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
    playCurrentSong();
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
            m_decoder->play();
        }
        // TODO store enough buffer before starting audio player
        // Otherwise ffmpeg would starve and throw eof
        if (m_audioPlayer) {
            m_audioPlayer->start();
        }
        m_currentState = PlaybackState::Playing;
        m_positionTimer->start();
        
        LOG_INFO("Playback resumed");
        emit playbackStateChanged(m_currentState);
    } else if (m_currentState == PlaybackState::Stopped && !m_currentPlaylist.isEmpty()) {
        // Start playing current song
        playCurrentSong();
    }
}

void AudioPlayerController::pause()
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    
    if (m_currentState == PlaybackState::Playing) {
        if (m_decoder) {
            m_decoder->pause();
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
    playCurrentSong();
}

void AudioPlayerController::previous()
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    
    if (m_currentPlaylist.isEmpty()) {
        return;
    }
    
    loadPreviousSong();
    playCurrentSong();
}

void AudioPlayerController::seekTo(qint64 positionMs)
{
    std::unique_lock<std::mutex> locker(m_stateMutex);
    
    if (m_decoder && m_currentState != PlaybackState::Stopped) {
        // TODO: Implement seeking functionality
        // Note: FFmpegDecoder would need seek functionality
        // This is a placeholder for future implementation
        m_currentPosition = positionMs;
        emit positionChanged(m_currentPosition);
        LOG_INFO("Seek to position: {}ms", positionMs);
    }
}

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
    if (m_currentState == PlaybackState::Playing) {
        m_currentPosition += 100; // Add 100ms
        
        // Clamp to total duration
        if (m_totalDuration > 0 && m_currentPosition > m_totalDuration) {
            m_currentPosition = m_totalDuration;
            
            // Song finished, move to next
            onAudioDecodingFinished();
            return;
        }
        
        emit positionChanged(m_currentPosition);
    }
}

void AudioPlayerController::onAudioDecodingFinished()
{
    LOG_INFO("Audio decoding finished for current song");
    
    // Handle different play modes
    switch (m_playMode) {
    case playlist::PlayMode::SingleLoop:
        // Restart current song
        playCurrentSong();
        break;
        
    case playlist::PlayMode::PlaylistLoop:
    case playlist::PlayMode::Random:
        // Move to next song
        loadNextSong();
        if (m_currentIndex >= 0) {
            playCurrentSong();
        } else {
            // End of playlist
            stop();
        }
        break;
    }
}

void AudioPlayerController::onAudioDecodingError(const QString& error)
{
    LOG_ERROR("Audio decoding error: {}", error.toStdString());
    
    m_lastError = error;
    m_currentState = PlaybackState::Error;
    
    emit playbackError(error);
    emit playbackStateChanged(m_currentState);
    
    // Try to skip to next song
    loadNextSong();
    if (m_currentIndex >= 0) {
        playCurrentSong();
    }
}

void AudioPlayerController::playCurrentSong()
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
            auto& nm = network::NetworkManager::instance();

            // Start both requests: expected size first (critical), and stream acquisition
            auto sizeFuture = nm.getStreamSizeByParamsAsync(platform, song.args);
            auto streamFuture = nm.getAudioStreamAsync(platform, song.args, filepath);

            // Get expected size and set it on decoder BEFORE opening the stream
            uint64_t expectedSize = 0;
            try {
                expectedSize = sizeFuture.get();
            } catch (...) {
                expectedSize = 0;
            }
            m_decoder->setStreamExpectedSize(expectedSize);

            m_currentStream = streamFuture.get();
            if (!m_currentStream) {
                throw std::runtime_error("Failed to acquire streaming input");
            }
            // Wait for minimal bytes to mitigate header parse failures
            {
                constexpr size_t kMinHeaderBuffer = 256 * 1024; // 256KB
                // Emit simple buffering progress while waiting, up to 3 seconds
                const int timeoutMs = 3000;
                const int stepMs = 100;
                int waited = 0;
                while (waited < timeoutMs) {
                    size_t buffered = m_currentStream->available();
                    emit downloadProgress(filepath, static_cast<qint64>(buffered), static_cast<qint64>(expectedSize));
                    if (buffered >= kMinHeaderBuffer) break;
                    QThread::msleep(stepMs);
                    waited += stepMs;
                }
                if (m_currentStream->available() < kMinHeaderBuffer) {
                    // Final wait without busy updates, to not oversleep beyond total timeout
                    m_currentStream->waitForEnoughData(kMinHeaderBuffer, timeoutMs - std::min(waited, timeoutMs));
                }
            }
            opened = m_decoder->openStream(m_currentStream);
        } else {
            // Use local file stream
            auto fileStream = std::make_shared<std::ifstream>(filepath.toStdString(), std::ios::binary);
            if (!fileStream->is_open()) {
                throw std::runtime_error("Failed to open local file");
            }
            // Determine file size for decoder progress/length
            fileStream->seekg(0, std::ios::end);
            std::streampos endPos = fileStream->tellg();
            fileStream->seekg(0, std::ios::beg);
            uint64_t fileSize = (endPos >= 0) ? static_cast<uint64_t>(endPos) : 0ULL;

            m_decoder->setStreamExpectedSize(fileSize);
            opened = m_decoder->openStream(fileStream);
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

    // After opening, query audio format and initialize audio output
    const auto fmt = m_decoder->getAudioFormat();
    if (!fmt.isValid()) {
        emit playbackError("Decoder did not provide a valid audio format");
        return;
    }

    // (Re)create audio player with the correct format
    m_audioPlayer.reset(new WASAPIAudioPlayer(fmt.sample_rate, fmt.channels, fmt.bits_per_sample));
    if (!m_audioPlayer->isInitialized()) {
        emit playbackError("Failed to initialize WASAPI audio player");
        return;
    }
    m_audioPlayer->start();

    // Start decoding/playback
    m_currentState = PlaybackState::Playing;
    m_currentPosition = 0;
    m_positionTimer->start();
    if (!m_decoder->play()) {
        emit playbackError("Failed to start audio decoding");
        return;
    }
    emit playbackStateChanged(m_currentState);
    emit positionChanged(m_currentPosition);
}

void AudioPlayerController::stopCurrentSong()
{
    if (m_decoder) {
        m_decoder->stop();
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

// Unsafe variant: caller MUST hold m_stateMutex when calling this.
void AudioPlayerController::stopCurrentSongUnsafe()
{
    // Same implementation as stopCurrentSong; kept separate so callers that
    // already hold the mutex can call this directly without re-locking.
    if (m_decoder) {
        m_decoder->stop();
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
            auto& nm = network::NetworkManager::instance();

            // Start both requests: expected size first (critical), and stream acquisition
            auto sizeFuture = nm.getStreamSizeByParamsAsync(platform, song.args);
            auto streamFuture = nm.getAudioStreamAsync(platform, song.args, filepath);

            // Get expected size and set it on decoder BEFORE opening the stream
            uint64_t expectedSize = 0;
            try { expectedSize = sizeFuture.get(); } catch (...) { expectedSize = 0; }
            m_decoder->setStreamExpectedSize(expectedSize);

            m_currentStream = streamFuture.get();
            if (!m_currentStream) {
                throw std::runtime_error("Failed to acquire streaming input");
            }
            // Wait for minimal bytes to mitigate header parse failures
            {
                constexpr size_t kMinHeaderBuffer = 256 * 1024; // 256KB
                // Emit simple buffering progress while waiting, up to 3 seconds
                const int timeoutMs = 3000;
                const int stepMs = 100;
                int waited = 0;
                while (waited < timeoutMs) {
                    size_t buffered = m_currentStream->available();
                    emit downloadProgress(filepath, static_cast<qint64>(buffered), static_cast<qint64>(expectedSize));
                    if (buffered >= kMinHeaderBuffer) break;
                    QThread::msleep(stepMs);
                    waited += stepMs;
                }
                if (m_currentStream->available() < kMinHeaderBuffer) {
                    // Final wait without busy updates, to not oversleep beyond total timeout
                    m_currentStream->waitForEnoughData(kMinHeaderBuffer, timeoutMs - std::min(waited, timeoutMs));
                }
            }
            opened = m_decoder->openStream(m_currentStream);
        } else {
            // Use local file stream
            auto fileStream = std::make_shared<std::ifstream>(filepath.toStdString(), std::ios::binary);
            if (!fileStream->is_open()) {
                throw std::runtime_error("Failed to open local file");
            }
            // Determine file size for decoder progress/length
            fileStream->seekg(0, std::ios::end);
            std::streampos endPos = fileStream->tellg();
            fileStream->seekg(0, std::ios::beg);
            uint64_t fileSize = (endPos >= 0) ? static_cast<uint64_t>(endPos) : 0ULL;

            m_decoder->setStreamExpectedSize(fileSize);
            opened = m_decoder->openStream(fileStream);
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

    // After opening, query audio format and initialize audio output
    const auto fmt = m_decoder->getAudioFormat();
    if (!fmt.isValid()) {
        emit playbackError("Decoder did not provide a valid audio format");
        return;
    }

    // (Re)create audio player with the correct format
    m_audioPlayer.reset(new WASAPIAudioPlayer(fmt.sample_rate, fmt.channels, fmt.bits_per_sample));
    if (!m_audioPlayer->isInitialized()) {
        emit playbackError("Failed to initialize WASAPI audio player");
        return;
    }
    m_audioPlayer->start();

    // Start decoding/playback
    m_currentState = PlaybackState::Playing;
    m_currentPosition = 0;
    m_positionTimer->start();
    if (!m_decoder->play()) {
        emit playbackError("Failed to start audio decoding");
        return;
    }
    emit playbackStateChanged(m_currentState);
    emit positionChanged(m_currentPosition);
}