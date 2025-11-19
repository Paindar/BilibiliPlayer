# Playback Control Component Contract

**Feature**: 001-unified-stream-player  
**Component**: Audio Playback and Player Control  
**Date**: 2025-11-13 (Updated from actual implementation)

## Purpose

Defines the interface contract for audio playback operations based on the actual `audio::AudioPlayerController` implementation, including play/pause/stop, volume control, playlist navigation, and state management.

---

## Component Interface: AudioPlayerController

### Purpose
Coordinates audio decoding (FFmpeg), buffering (AudioFrameQueue), and output (WASAPI). Manages playback state with event-driven architecture and exposes controls via Qt signals/slots.

### Class Definition

```cpp
namespace audio {
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
        qint64 currentPosition = 0;  // milliseconds
        qint64 totalDuration = 0;    // milliseconds
        playlist::PlayMode playMode = playlist::PlayMode::PlaylistLoop;
    };

    class AudioPlayerController : public QObject {
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
        void setCurrentPlaylist(const QUuid& playlistId, int startIndex = 0);
        QUuid getCurrentPlaylistId() const;
        int getCurrentAudioIndex() const;
        void clearPlaylist();
        
        // Playback control
        void play();
        void pause();
        void stop();
        void next();
        void previous();
        // void seekTo(qint64 positionMs); // TODO: Future implementation
        
        // Play mode control
        void setPlayMode(playlist::PlayMode mode);
        playlist::PlayMode getPlayMode() const;
        
        // Status queries
        PlaybackStatus getStatus() const;
        bool isPlaying() const;
        bool isPaused() const;
        bool isStopped() const;
        
        // Volume control
        void setVolume(float volume);  // 0.0 to 1.0
        float getVolume() const;
        
    signals:
        // Playback state signals
        void playbackStateChanged(PlaybackState state);
        void playbackCompleted();
        void currentSongChanged(const playlist::SongInfo& song, int index);
        void positionChanged(qint64 positionMs);
        void playModeChanged(playlist::PlayMode mode);
        void volumeChanged(float volume);
        
        // Error signals
        void playbackError(const QString& message);
        void songLoadError(const playlist::SongInfo& song, const QString& message);
        
    private slots:
        void onPositionTimerTimeout();
        void onPlaylistChanged(const QUuid& playlistId);
        
    private:
        // Event handlers (serialized execution via AudioEventProcessor)
        void onPlayEvent(const QVariantHash&);
        void onPauseEvent(const QVariantHash&);
        void onStopEvent(const QVariantHash&);
        void onPlayFinishedEvent(const QVariantHash&);
        void onPlayErrorEvent(const QVariantHash&);
        void onDecodingFinishedEvent(const QVariantHash&);
        
        // Audio components
        std::shared_ptr<WASAPIAudioOutputUnsafe> m_audioOutput;
        std::unique_ptr<FFmpegStreamDecoder> m_decoder;
        std::shared_ptr<std::istream> m_currentStream;
        std::shared_ptr<AudioFrameQueue> m_frameQueue;
        
        // State
        mutable std::mutex m_stateMutex;
        QUuid m_currentPlaylistId;
        QList<playlist::SongInfo> m_currentPlaylist;
        int m_currentIndex;
        PlaybackState m_currentState;
        playlist::PlayMode m_playMode;
        float m_volume;
        
        // Background threads
        std::shared_ptr<std::thread> m_frameTransmissionThread;
        std::shared_ptr<std::thread> m_playbackWatcherThread;
        std::atomic<bool> m_frameTransmissionActive;
        
        // Random playback support
        std::mt19937 m_randomGenerator;
        QList<int> m_randomPlayOrder;
    };
}
```

---

## Method Contracts

### Playlist Control

#### `playPlaylist(playlistId, startIndex)`

**Preconditions**:
- `playlistId` must be valid UUID from PlaylistManager
- `startIndex` must be >= 0 and < playlist size (default: 0)
- Player can be in any state (will stop current playback if playing)

**Postconditions**:
- Loads playlist from PlaylistManager by UUID
- Sets current playlist and index
- Starts playback of song at `startIndex`
- Previous playback stopped (if any)
- `currentSongChanged` signal emitted
- `playbackStateChanged` signal emitted (Stopped → Playing)

**Error Handling**:
- Emits `playbackError` if playlist not found
- Emits `songLoadError` if song at startIndex fails to load
- State remains Stopped on error

**Threading**:
- Thread-safe (mutex-protected)
- Can be called from any thread (UI or background)

**Example**:
```cpp
QUuid playlistId = playlistManager->getPlaylist("Favorites")->uuid;
audioPlayer->playPlaylist(playlistId, 0);  // Play from first song
```

---

#### `playPlaylistFromSong(playlistId, startSong)`

**Preconditions**:
- `playlistId` must be valid
- `startSong` must exist in the playlist

**Postconditions**:
- Loads playlist and finds index of `startSong`
- Starts playback from that song
- Equivalent to `playPlaylist(playlistId, foundIndex)`

**Error Handling**:
- Emits `playbackError` if song not found in playlist
- Falls back to `startIndex = 0` if song not found

---

#### `setCurrentPlaylist(playlistId, startIndex)`

**Preconditions**:
- `playlistId` must be valid

**Postconditions**:
- Loads playlist without starting playback
- Sets current index to `startIndex`
- Does NOT change playback state
- Useful for queuing a playlist without auto-play

**Error Handling**:
- Logs warning if playlist not found
- No error signals emitted

---

#### `clearPlaylist()`

**Preconditions**:
- None

**Postconditions**:
- Stops current playback
- Clears `m_currentPlaylist` and resets `m_currentIndex` to -1
- State changes to Stopped
- `playbackStateChanged` emitted

---

### Playback Control

#### `play()`

**Preconditions**:
- Must have a playlist loaded (`m_currentPlaylist` not empty)
- Can be called in any state

**Postconditions**:
- **If Stopped**: Starts playback from current index
  - Creates network stream via NetworkManager (if platform song)
  - Creates file stream (if local song)
  - Initializes FFmpegStreamDecoder with stream
  - Starts decoder thread
  - Starts frame transmission thread
  - Starts WASAPI playback
  - State: Stopped → Playing
- **If Paused**: Resumes playback from paused position
  - WASAPI resumes output
  - State: Paused → Playing
- **If Playing**: No-op (already playing)

**Error Handling**:
- Emits `playbackError` if no playlist loaded
- Emits `songLoadError` if stream creation fails
- State remains unchanged on error

**Threading**:
- Non-blocking (spawns background threads)
- Decoder runs in separate thread
- Frame transmission runs in separate thread
- WASAPI output runs in separate thread

**Event Flow**:
```
play() → PlayEvent → onPlayEvent() → 
  NetworkManager::getAudioStreamAsync() → 
  FFmpegStreamDecoder::open() → 
  FFmpegStreamDecoder::start() → 
  frameTransmissionLoop() starts → 
  WASAPIAudioOutput::start()
```

---

#### `pause()`

**Preconditions**:
- Must be in Playing state

**Postconditions**:
- Pauses WASAPI output (audio stops)
- Decoder and frame transmission continue (keeps buffer filled)
- Current position preserved
- State: Playing → Paused
- `playbackStateChanged` emitted

**Error Handling**:
- No-op if not playing
- Safe to call multiple times

**Threading**:
- Thread-safe

---

#### `stop()`

**Preconditions**:
- None (can be called in any state)

**Postconditions**:
- Stops WASAPI output
- Stops decoder thread
- Stops frame transmission thread
- Closes current stream
- Clears frame queue
- Resets position to 0
- State: * → Stopped
- `playbackStateChanged` emitted

**Error Handling**:
- Always succeeds (idempotent)
- Safe to call multiple times

**Threading**:
- Blocks until background threads terminate (may take up to 500ms)

---

#### `next()`

**Preconditions**:
- Must have playlist loaded

**Postconditions**:
- Determines next song based on `m_playMode`:
  - **PlaylistLoop**: `(currentIndex + 1) % playlist.size()`
  - **SingleLoop**: Same index (repeat current)
  - **Random**: Random index from `m_randomPlayOrder`
- Stops current playback
- Starts playback of next song
- `currentSongChanged` emitted
- `playbackStateChanged` emitted (if state changed)

**Error Handling**:
- No-op if playlist empty
- Emits `songLoadError` if next song fails to load

**Example**:
```cpp
// PlaylistLoop mode: song1 → song2 → song3 → song1 → ...
// SingleLoop mode: song1 → song1 → song1 → ...
// Random mode: song1 → song5 → song2 → song1 → song4 → ...
```

---

#### `previous()`

**Preconditions**:
- Must have playlist loaded

**Postconditions**:
- **If position < 3 seconds**: Restart current song (position = 0)
- **If position >= 3 seconds**: Go to previous song
  - PlaylistLoop: `(currentIndex - 1 + size) % size`
  - Random: Previous in random order
- Stops and restarts playback
- `currentSongChanged` emitted (if changed)

---

### Play Mode Control

#### `setPlayMode(mode)`

**Preconditions**:
- `mode` must be valid `playlist::PlayMode` enum value

**Postconditions**:
- Updates `m_playMode`
- If mode is Random: generates new `m_randomPlayOrder`
- `playModeChanged` signal emitted
- Does NOT affect current playback

**Thread Safety**:
- Thread-safe (mutex-protected)

**Example**:
```cpp
audioPlayer->setPlayMode(playlist::PlayMode::Random);
// Future next() calls will use random order
```

---

### Volume Control

#### `setVolume(volume)`

**Preconditions**:
- `volume` must be in range [0.0, 1.0]

**Postconditions**:
- Updates `m_volume`
- Applies volume to WASAPIAudioOutput (via `ISimpleAudioVolume::SetMasterVolume`)
- `volumeChanged` signal emitted
- Volume persisted in config (on next `saveConfig()`)

**Error Handling**:
- Clamps volume to [0.0, 1.0] if out of range
- Logs warning if WASAPI volume set fails

**Threading**:
- Thread-safe

**Example**:
```cpp
audioPlayer->setVolume(0.5f);  // 50% volume
```

---

#### `getVolume()`

**Preconditions**:
- None

**Postconditions**:
- Returns current volume (0.0 to 1.0)

**Thread Safety**:
- Thread-safe (mutex-protected)

---

### Status Queries

#### `getStatus()`

**Preconditions**:
- None

**Postconditions**:
- Returns `PlaybackStatus` struct with:
  - `state`: Current playback state
  - `currentSong`: SongInfo of current song
  - `currentIndex`: Index in playlist
  - `totalSongs`: Playlist size
  - `currentPosition`: Position in milliseconds
  - `totalDuration`: Song duration in milliseconds
  - `playMode`: Current play mode

**Thread Safety**:
- Thread-safe (creates snapshot of current state)

**Example**:
```cpp
PlaybackStatus status = audioPlayer->getStatus();
qDebug() << "Playing:" << status.currentSong.title;
qDebug() << "Position:" << status.currentPosition / 1000 << "s";
```

---

#### `isPlaying() / isPaused() / isStopped()`

**Preconditions**:
- None

**Postconditions**:
- Returns boolean indicating current state

**Thread Safety**:
- Thread-safe (atomic or mutex-protected)

---

## Signal Contracts

### `playbackStateChanged(PlaybackState state)`

**Emitted When**:
- State transitions: Stopped ↔ Playing ↔ Paused
- Called from event handlers (onPlayEvent, onPauseEvent, onStopEvent)

**Threading**:
- Emitted on main thread (Qt event loop)

**Example**:
```cpp
connect(audioPlayer, &AudioPlayerController::playbackStateChanged, 
    [](PlaybackState state) {
        if (state == PlaybackState::Playing) {
            playButton->setText("Pause");
        }
    });
```

---

### `currentSongChanged(const playlist::SongInfo& song, int index)`

**Emitted When**:
- Playback starts on new song
- `next()` or `previous()` called
- Playlist changed via `playPlaylist()`

**Parameters**:
- `song`: SongInfo of new current song
- `index`: Index in current playlist

**Example**:
```cpp
connect(audioPlayer, &AudioPlayerController::currentSongChanged,
    [](const playlist::SongInfo& song, int index) {
        titleLabel->setText(song.title);
        artistLabel->setText(song.uploader);
    });
```

---

### `positionChanged(qint64 positionMs)`

**Emitted When**:
- Position timer timeout (every 100ms by default)
- Updated by playback watcher thread

**Parameters**:
- `positionMs`: Current position in milliseconds

**Threading**:
- Emitted on main thread via QTimer

**Example**:
```cpp
connect(audioPlayer, &AudioPlayerController::positionChanged,
    [](qint64 pos) {
        progressSlider->setValue(pos);
    });
```

---

### `playbackError(const QString& message)` / `songLoadError(const playlist::SongInfo& song, const QString& message)`

**Emitted When**:
- Network stream retrieval fails
- FFmpeg decoder initialization fails
- WASAPI output initialization fails
- Song file not found (local playback)

**Error Handling**:
- UI should display error to user
- Player state changes to Stopped
- `next()` can be called to skip failed song

---

## Architecture: Event-Driven Design

### Event Flow

AudioPlayerController uses **AudioEventProcessor** for serialized event execution:

```
UI Thread                Event Thread              Background Threads
   |                          |                            |
play() -----> PlayEvent ----> onPlayEvent() ----> NetworkManager::getAudioStreamAsync()
   |                          |                            |
   |                          |                      (stream ready)
   |                          |                            |
   |                    DecodingStarted <-------- FFmpegStreamDecoder::start()
   |                          |                            |
   |                    FrameDecoded <--------- (frame decoded loop)
   |                          |                            |
   |                          | -----> frameTransmissionLoop() sends to WASAPI
   |                          |                            |
pause() ----> PauseEvent ---> onPauseEvent() ---> WASAPIAudioOutput::pause()
   |                          |                            |
stop() -----> StopEvent ----> onStopEvent() -----> Cleanup threads
```

**Benefits**:
- **Thread safety**: All state mutations happen on event thread
- **Serialization**: Events processed sequentially (no race conditions)
- **Decoupling**: Background threads don't directly modify controller state

---

## Threading Architecture

### Thread Overview

1. **Main Thread (Qt Event Loop)**:
   - Handles UI interactions
   - Emits Qt signals
   - Processes QTimer events (`positionChanged`)

2. **Event Thread (AudioEventProcessor)**:
   - Processes playback events (Play, Pause, Stop)
   - Mutates controller state

3. **Decoder Thread (FFmpegStreamDecoder)**:
   - Reads from input stream (`std::istream`)
   - Decodes audio frames via FFmpeg
   - Pushes frames to `AudioFrameQueue`

4. **Frame Transmission Thread**:
   - Consumes frames from `AudioFrameQueue`
   - Sends PCM data to WASAPI circular buffer
   - Runs `frameTransmissionLoop()`

5. **Playback Watcher Thread**:
   - Monitors playback progress
   - Updates position
   - Detects end-of-track

6. **Network Threads (NetworkManager)**:
   - Downloads audio streams from Bilibili/YouTube
   - Writes to `RealtimePipe` (producer-consumer)

### Synchronization

| Resource | Protection Mechanism |
|----------|---------------------|
| `m_currentState` | `m_stateMutex` (std::mutex) |
| `m_currentPlaylist` | `m_stateMutex` |
| `m_frameQueue` | Internal mutex (AudioFrameQueue) |
| `m_audioOutput` | `m_componentMutex` |
| `m_decoder` | `m_componentMutex` |
| Event queue | AudioEventProcessor internal mutex |

---

## Integration with Other Components

### PlaylistManager Integration

```cpp
// AudioPlayerController subscribes to playlist changes
connect(playlistManager, &PlaylistManager::playlistSongsChanged,
    audioPlayer, &AudioPlayerController::onPlaylistChanged);

// When playlist changes, reload if currently playing that playlist
void AudioPlayerController::onPlaylistChanged(const QUuid& playlistId) {
    if (playlistId == m_currentPlaylistId) {
        // Reload playlist from PlaylistManager
        // Preserve current index if possible
    }
}
```

### NetworkManager Integration

```cpp
// AudioPlayerController uses NetworkManager to get streams
void AudioPlayerController::playCurrentSongUnsafe() {
    const auto& song = m_currentPlaylist[m_currentIndex];
    if (song.platform != -1) {  // Network source
        auto streamFuture = networkManager->getAudioStreamAsync(
            static_cast<network::SupportInterface>(song.platform),
            song.args,
            generateStreamingFilepath(song)
        );
        m_currentStream = streamFuture.get();  // Blocks until ready
        m_decoder->open(m_currentStream);
    } else {  // Local file
        auto fileStream = std::make_shared<std::ifstream>(song.filepath.toStdString());
        m_decoder->open(fileStream);
    }
    m_decoder->start();  // Start decoding thread
}
```

### FFmpegStreamDecoder Integration

```cpp
// Decoder pushes frames to queue
m_decoder->open(stream);
m_decoder->setFrameQueue(m_frameQueue);
m_decoder->start();  // Spawns decoder thread

// Frame transmission thread consumes from queue
void AudioPlayerController::frameTransmissionLoop() {
    while (m_frameTransmissionActive) {
        AudioFrame frame;
        if (m_frameQueue->pop(frame, 100ms)) {
            m_audioOutput->write(frame.data, frame.size);
        }
    }
}
```

---

## Configuration Persistence

### Saved Settings (via `saveConfig()`)

- Last played playlist UUID
- Last played song index
- Volume level (0.0 to 1.0)
- Play mode (SingleLoop/PlaylistLoop/Random)

### Config Format (ConfigManager)

```ini
[AudioPlayer]
CurrentPlaylistId=3fa85f64-5717-4562-b3fc-2c963f66afa6
CurrentIndex=5
Volume=0.75
PlayMode=1  # PlaylistLoop
```

---

## Error Handling Patterns

### Network Stream Failure

```cpp
connect(audioPlayer, &AudioPlayerController::songLoadError,
    [this](const playlist::SongInfo& song, const QString& error) {
        QMessageBox::warning(this, "Playback Error",
            QString("Failed to load '%1': %2").arg(song.title, error));
        audioPlayer->next();  // Skip to next song
    });
```

### Decoder Failure

```cpp
// Internal handling in onDecodingErrorEvent
void AudioPlayerController::onDecodingErrorEvent(const QVariantHash& data) {
    QString error = data["error"].toString();
    emit playbackError(error);
    stop();  // Stop playback on decode error
}
```

---

## Performance Considerations

1. **Frame Queue Size**: Default 100 frames (~2 seconds of audio at 48kHz)
   - Prevents decoder starvation
   - Limits memory usage

2. **Position Update Frequency**: 100ms timer
   - Balance between UI responsiveness and CPU usage

3. **WASAPI Buffer**: 10ms latency (configurable)
   - Low latency for responsive playback
   - Requires fast frame transmission thread

4. **Stream Buffering**: RealtimePipe provides 1MB circular buffer
   - Network → Decoder decoupling
   - Handles network jitter

---

## Testing Considerations

See test files:
- `test/streaming_integration_test.cpp`: End-to-end playback with mock network
- `test/streaming_cancellation_test.cpp`: Stop/next during playback
- `test/realtime_pipe_test.cpp`: Producer-consumer patterns

---

## Future Enhancements

### Seek Support

```cpp
void AudioPlayerController::seekTo(qint64 positionMs) {
    // 1. Pause playback
    // 2. Tell decoder to seek (FFmpeg av_seek_frame)
    // 3. Clear frame queue
    // 4. Resume playback
    emit positionChanged(positionMs);
}
```

**Challenges**:
- Network streams may not support seeking (need range request support)
- FFmpeg seek requires seekable stream (not all RealtimePipe streams are seekable)

### Gapless Playback

```cpp
// Pre-load next song while current song playing
void AudioPlayerController::preloadNext() {
    int nextIndex = getNextIndex();
    auto nextStream = networkManager->getAudioStreamAsync(/* next song params */);
    m_preloadedStream = nextStream.get();
    // When current song finishes, instantly start nextStream
}
```

---

## Thread Safety Summary

| Method | Thread Safety | Notes |
|--------|---------------|-------|
| `play/pause/stop()` | Thread-safe | Posts event to event thread |
| `setVolume()` | Thread-safe | Mutex-protected |
| `getStatus()` | Thread-safe | Returns snapshot |
| `next/previous()` | Thread-safe | Posts event |
| Signal emissions | Main thread | Via Qt event loop |

---
