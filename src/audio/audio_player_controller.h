#pragma once

#include <QObject>
class QTimer;
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
#include <util/safe_queue.hpp>
#include "audio_frame.h"
#include "audio_event_processor.h"
#include "ffmpeg_decoder.h"
#include "wasapi_audio_output.h"

// Forward declarations
class WASAPIAudioOutputUnsafe;
class FFmpegStreamDecoder;
namespace network { class NetworkManager; }

namespace audio { 
class AudioEventProcessor;

enum class PlaybackState {
    Stopped = 0,
    Playing = 1,
    Paused = 2
};

struct PlaybackStatus {
    PlaybackState state = PlaybackState::Stopped;
    playlist::SongInfo currentSong;
    int currentIndex = -1;
    int totalSongs = 0;
    qint64 currentPosition = 0;  // in milliseconds
    qint64 totalDuration = 0;    // in milliseconds
    playlist::PlayMode playMode = playlist::PlayMode::PlaylistLoop;
};

// Result returned by operations that previously emitted signals while holding locks.
struct PlayOperationResult {
    enum Kind {
        Success = 0,
        SongLoadError = 1,
        PlaybackError = 2
    } kind = Success;
    QString message;
};

class AudioPlayerController : public QObject
{
    Q_OBJECT
public:
    explicit AudioPlayerController(QObject *parent = nullptr);
    ~AudioPlayerController();

    // Configuration
    void saveConfig();
    void loadConfig();

    // Playlist control
    void playPlaylist(const QUuid& playlistId, int startIndex = 0);
    void playPlaylistFromSong(const QUuid& playlistId, const playlist::SongInfo& startSong);
    void setCurrentPlaylist(const QUuid& playlistId, const int startIndex = 0);
    QUuid getCurrentPlaylistId() const;
    int getCurrentAudioIndex() const;
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
    void playbackCompleted();
    void currentSongChanged(const playlist::SongInfo& song, int index);
    void positionChanged(qint64 positionMs);
    // void durationChanged(qint64 durationMs);
    void playModeChanged(playlist::PlayMode mode);
    void volumeChanged(float volume);
    
    // Error signals
    void playbackError(const QString& message);
    void songLoadError(const playlist::SongInfo& song, const QString& message);

private slots:
    void onPositionTimerTimeout();
    void onPlaylistChanged(const QUuid& playlistId);

private:
    void registerEventHandlers();

    // File and streaming management
    QString generateStreamingFilepath(const playlist::SongInfo& song) const;
    bool isLocalFileAvailable(const playlist::SongInfo& song) const;
    void setupRandomGenerator();
    PlayOperationResult playCurrentSongUnsafe(const playlist::SongInfo& song, int index);
    PlayOperationResult cleanPlayResources();
    

private: // Event handlers (serialized execution)
    void onPlayEvent(const QVariantHash&);
    void onPauseEvent(const QVariantHash&);
    void onStopEvent(const QVariantHash&);
    void onPlayFinishedEvent(const QVariantHash&);
    void onPlayErrorEvent(const QVariantHash&);
    void onInputStreamReadCompletedEvent(const QVariantHash&);
    void onInputStreamReadErrorEvent(const QVariantHash&);
    void onDecodingFinishedEvent(const QVariantHash&);
    void onDecodingErrorEvent(const QVariantHash&);
    void onFrameTransmissionFinishedEvent(const QVariantHash&);
    void onFrameTransmissionErrorEvent(const QVariantHash&);
private:
    // Event system
    std::shared_ptr<audio::AudioEventProcessor> m_eventProcessor;

    // Audio components
    mutable std::mutex m_componentMutex;
    std::shared_ptr<WASAPIAudioOutputUnsafe> m_audioOutput;
    std::unique_ptr<FFmpegStreamDecoder> m_decoder;
    std::shared_ptr<std::istream> m_currentStream;

    // Notice: This mutex is just used for lock the m_frameQueue pointer,
    //  if u wanna access the frame queue content, pls use the mutex INSIDE AudioFrameQueue struct.
    std::shared_ptr<AudioFrameQueue> m_frameQueue;
    
    // Frame transmission thread (consumes frames from decoder and sends to audio output)
    std::shared_ptr<std::thread> m_frameTransmissionThread;
    std::atomic<bool> m_frameTransmissionActive;
    void frameTransmissionLoop();

    // Playlist data
    // Thread safety: protect controller state with a standard mutex
    mutable std::mutex m_stateMutex;
    QUuid m_currentPlaylistId;
    QList<playlist::SongInfo> m_currentPlaylist;
    int m_currentIndex;
    // Playback state
    PlaybackState m_currentState;
    playlist::PlayMode m_playMode;
    qint64 m_currentPosition;
    qint64 m_totalDuration;
    float m_volume;

    // Position Timer
    QTimer* m_positionTimer;
    std::shared_ptr<std::thread> m_playbackWatcherThread;
    void playbackWatcherFunc();
    std::atomic<bool> m_playbackWatcherExitFlag;

    // Random playback
    std::random_device m_randomDevice;
    std::mt19937 m_randomGenerator;
    QList<int> m_randomPlayOrder;
    int m_randomIndex;
};
}