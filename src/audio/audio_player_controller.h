#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QUuid>
#include <QList>
#include <mutex>
#include <memory>
#include <atomic>
#include <random>
#include <thread>
#include <condition_variable>
#include <playlist/playlist.h>

// Forward declarations
class WASAPIAudioOutputUnsafe;
class FFmpegStreamDecoder;
struct AudioFrameQueue;
namespace network { class NetworkManager; }

enum class PlaybackState {
    Stopped = 0,
    Playing = 1,
    Paused = 2,
    Loading = 3,
    Error = 4
};

struct PlaybackStatus {
    PlaybackState state = PlaybackState::Stopped;
    playlist::SongInfo currentSong;
    int currentIndex = -1;
    int totalSongs = 0;
    qint64 currentPosition = 0;  // in milliseconds
    qint64 totalDuration = 0;    // in milliseconds
    playlist::PlayMode playMode = playlist::PlayMode::PlaylistLoop;
    QString errorMessage;
};

class AudioPlayerController : public QObject
{
    Q_OBJECT

public:
    explicit AudioPlayerController(QObject *parent = nullptr);
    ~AudioPlayerController();

    // Playlist control
    void playPlaylist(const QUuid& playlistId, int startIndex = 0);
    void playPlaylistFromSong(const QUuid& playlistId, const playlist::SongInfo& startSong);
    void setCurrentPlaylist(const QUuid& playlistId);
    void clearPlaylist();

    // Playback control
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    // void seekTo(qint64 positionMs); // TODO: Implement in new decoder API

    // Play mode control
    void setPlayMode(playlist::PlayMode mode);
    playlist::PlayMode getPlayMode() const;

    // Status queries
    PlaybackStatus getStatus() const;
    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;

    // Volume control
    void setVolume(float volume); // 0.0 to 1.0
    float getVolume() const;

signals:
    // Playback state signals
    void playbackStateChanged(PlaybackState state);
    void currentSongChanged(const playlist::SongInfo& song, int index);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void playModeChanged(playlist::PlayMode mode);
    void volumeChanged(float volume);
    
    // Error signals
    void playbackError(const QString& message);
    void songLoadError(const playlist::SongInfo& song, const QString& message);

private slots:
    void onPositionTimerTimeout();
    void onAudioDecodingFinished();
    void onAudioDecodingError(const QString& error);

private:
    // Core playback functions
    void playCurrentSong();
    void stopCurrentSong();
    void loadNextSong();
    void loadPreviousSong();
    int getNextSongIndex();
    int getPreviousSongIndex();
    
    // Private helpers that MUST be called with m_stateMutex held (unsafe)
    void playCurrentSongUnsafe();
    void stopCurrentSongUnsafe();
    void clearPlaylistUnsafe();

    // File and streaming management
    QString generateStreamingFilepath(const playlist::SongInfo& song) const;
    bool isLocalFileAvailable(const playlist::SongInfo& song) const;
    // Streaming is acquired via NetworkManager in playCurrentSong()

    // Utility functions
    // void updatePlaybackStatus();
    // void emitStatusSignals();
    void setupRandomGenerator();

private:
    // Audio components
    std::unique_ptr<WASAPIAudioOutputUnsafe> m_audioPlayer;
    std::unique_ptr<FFmpegStreamDecoder> m_decoder;
    std::shared_ptr<AudioFrameQueue> m_frameQueue;
    
    // Frame transmission thread (consumes frames from decoder and sends to audio output)
    std::thread m_frameTransmissionThread;
    std::atomic<bool> m_frameTransmissionActive;
    void frameTransmissionLoop();
    
    // Streaming components
    std::shared_ptr<std::istream> m_currentStream;

    // Playlist data
    QUuid m_currentPlaylistId;
    QList<playlist::SongInfo> m_currentPlaylist;
    int m_currentIndex;

    // Playback state
    PlaybackState m_currentState;
    playlist::PlayMode m_playMode;
    qint64 m_currentPosition;
    qint64 m_totalDuration;
    float m_volume;

    // Timers and threading
    QTimer* m_positionTimer;
    
    // Random playback
    std::random_device m_randomDevice;
    std::mt19937 m_randomGenerator;
    QList<int> m_randomPlayOrder;
    int m_randomIndex;

    // Thread safety: protect controller state with a standard mutex
    mutable std::mutex m_stateMutex;

    // Error handling
    QString m_lastError;
};