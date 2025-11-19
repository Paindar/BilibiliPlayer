# Data Model: Unified Multi-Source Audio Stream Player

**Feature**: 001-unified-stream-player  
**Date**: 2025-11-13  
**Prerequisites**: research.md

## Purpose

This document defines the core entities, their attributes, relationships, validation rules, and state transitions for the BilibiliPlayer application.

---

## 1. AudioSource

Represents an audio item from any platform or local file system.

### Attributes

| Attribute | Type | Required | Description | Validation |
|-----------|------|----------|-------------|------------|
| `id` | `string` | Yes | Unique identifier (UUID or hash) | Non-empty, unique within playlist |
| `sourceType` | `enum SourceType` | Yes | Origin of audio (Bilibili, YouTube, Local, etc.) | Must be valid enum value |
| `url` | `string` | Conditional | URL for network sources | Required if sourceType != Local; Must be valid HTTP/HTTPS URL |
| `filePath` | `string` | Conditional | Absolute path for local files | Required if sourceType == Local; Must be valid file path, file must exist |
| `title` | `string` | Yes | Display name of audio | Non-empty, max 256 chars |
| `duration` | `int` | Yes | Duration in seconds | >0 |
| `artist` | `string` | No | Artist/creator/uploader name | Max 128 chars |
| `album` | `string` | No | Album name (if applicable) | Max 128 chars |
| `thumbnailUrl` | `string` | No | URL to thumbnail image | Valid URL or empty |
| `qualityOptions` | `array<QualityOption>` | No | Available quality levels | Empty or valid quality options |
| `metadata` | `map<string, string>` | No | Additional platform-specific metadata | Key-value pairs |
| `dateAdded` | `timestamp` | Yes | When item was added to playlist | ISO 8601 format |
| `lastPlayed` | `timestamp` | No | Last playback time | ISO 8601 format or null |

### Enums

```cpp
enum class SourceType {
    Bilibili,
    YouTube,
    Local,
    SoundCloud,  // Future expansion
    Other
};

struct QualityOption {
    string id;           // Platform-specific quality ID (e.g., "30216" for Bilibili)
    int bitrate;         // kbps
    string codec;        // "mp3", "aac", "opus", etc.
    string description;  // Human-readable (e.g., "High Quality 320kbps")
};
```

### Relationships

- **One-to-Many**: One AudioSource can appear in multiple Playlists
- **Many-to-One**: Many AudioSources belong to one SourceType

### State Transitions

```
[Created] → [Validated] → [Ready]
              ↓
          [Invalid] (URL/file no longer accessible)
          
[Ready] → [Playing] → [Paused] → [Playing]
                   ↘ [Stopped] → [Ready]
                   
[Ready/Invalid] → [Removed]
```

### Business Rules

1. At least one of `url` or `filePath` MUST be present
2. If `sourceType == Local`, `filePath` MUST be absolute path
3. If `sourceType != Local`, `url` MUST be valid HTTP/HTTPS URL
4. Title MUST NOT be empty (auto-generated from filename if not provided)
5. Duration MUST be >0 (detected during validation)
6. Validation MUST occur before adding to playlist
7. Invalid sources (deleted files, removed URLs) SHOULD be marked but not auto-removed

---

## 2. Playlist

Represents a collection of AudioSources in a specific order.

### Attributes

| Attribute | Type | Required | Description | Validation |
|-----------|------|----------|-------------|------------|
| `id` | `string` | Yes | Unique identifier (UUID) | Non-empty, unique globally |
| `name` | `string` | Yes | Playlist display name | Non-empty, max 128 chars |
| `description` | `string` | No | User-provided description | Max 512 chars |
| `items` | `array<AudioSource>` | Yes | Ordered list of audio sources | Can be empty |
| `categoryId` | `string` | No | Parent category ID | Must reference existing category or null |
| `dateCreated` | `timestamp` | Yes | Creation timestamp | ISO 8601 format |
| `dateModified` | `timestamp` | Yes | Last modification timestamp | ISO 8601 format, >= dateCreated |
| `totalDuration` | `int` | Computed | Sum of all item durations | Read-only, computed on demand |
| `itemCount` | `int` | Computed | Number of items | Read-only, items.size() |

### Relationships

- **Many-to-One**: Many Playlists belong to one Category (optional)
- **One-to-Many**: One Playlist contains many AudioSources

### State Transitions

```
[Created] → [Modified] → [Saved]
              ↓            ↓
          [Unsaved] → [Saved]
          
[Saved] → [Deleted]
```

### Business Rules

1. Playlist name MUST be unique within a category
2. Items can be reordered without changing `dateModified` (separate action)
3. Adding/removing items updates `dateModified`
4. Empty playlists are valid
5. Duplicate AudioSources (same ID) within playlist are allowed (user may want same song twice)
6. Deleting a category does NOT delete its playlists (they become uncategorized)

---

## 3. Category

Represents a grouping mechanism for organizing playlists.

### Attributes

| Attribute | Type | Required | Description | Validation |
|-----------|------|----------|-------------|------------|
| `id` | `string` | Yes | Unique identifier (UUID) | Non-empty, unique globally |
| `name` | `string` | Yes | Category display name | Non-empty, max 64 chars |
| `color` | `string` | No | UI color (hex code) | Valid hex color (#RRGGBB) or null |
| `icon` | `string` | No | Icon name or path | Max 64 chars |
| `sortOrder` | `int` | Yes | Display order in UI | >=0, unique within parent |
| `dateCreated` | `timestamp` | Yes | Creation timestamp | ISO 8601 format |

### Relationships

- **One-to-Many**: One Category contains many Playlists

### State Transitions

```
[Created] → [Active]
              ↓
          [Deleted] (playlists become uncategorized)
```

### Business Rules

1. Category name MUST be unique globally
2. Default category "Uncategorized" always exists (cannot be deleted)
3. Deleting category moves playlists to "Uncategorized"
4. sortOrder MUST be unique; renumbering allowed on reorder
5. Maximum 50 categories (UI constraint)

---

## 4. PlaybackState

Represents the current state of the audio player.

### Attributes

| Attribute | Type | Required | Description | Validation |
|-----------|------|----------|-------------|------------|
| `currentSource` | `AudioSource` | No | Currently loaded audio | Null if nothing loaded |
| `playlistId` | `string` | No | Active playlist ID | Null if playing standalone item |
| `currentIndex` | `int` | No | Index in playlist | >=0 if playlist active, else null |
| `state` | `enum PlayerState` | Yes | Playback state | Must be valid enum value |
| `position` | `int` | Yes | Current position in seconds | >=0, <= duration |
| `duration` | `int` | Yes | Total duration in seconds | >=0 |
| `volume` | `int` | Yes | Volume level 0-100 | 0-100 inclusive |
| `isMuted` | `bool` | Yes | Mute state | true/false |
| `isShuffled` | `bool` | Yes | Shuffle mode | true/false |
| `repeatMode` | `enum RepeatMode` | Yes | Repeat mode | Must be valid enum value |
| `bufferPercent` | `int` | Yes | Buffer fill percentage | 0-100 inclusive |
| `isBuffering` | `bool` | Yes | Currently buffering | true/false |
| `errorMessage` | `string` | No | Last error (if any) | Max 256 chars |

### Enums

```cpp
enum class PlayerState {
    Stopped,
    Playing,
    Paused,
    Buffering,
    Error
};

enum class RepeatMode {
    Off,        // Play playlist once, stop at end
    All,        // Repeat entire playlist
    One         // Repeat current song
};
```

### State Transitions

```
[Stopped] → [Buffering] → [Playing] → [Paused] → [Playing]
                             ↓           ↓
                          [Error]    [Stopped]
                          
[Playing] → (end of track) → [Stopped/Playing next]
```

### Business Rules

1. State transition validation enforced (can't go directly from Stopped to Paused)
2. Position MUST be reset to 0 when loading new source
3. Volume changes MUST persist across sessions (saved to config)
4. Buffering state automatically set when buffer drops below threshold
5. Error state MUST include descriptive errorMessage

---

## 5. PlatformConnection

Represents authentication and session state for external platforms.

### Attributes

| Attribute | Type | Required | Description | Validation |
|-----------|------|----------|-------------|------------|
| `platform` | `enum Platform` | Yes | Platform identifier | Must be valid enum value |
| `isEnabled` | `bool` | Yes | Whether platform is active | true/false |
| `authToken` | `string` | No | OAuth token (if applicable) | Encrypted, max 1024 chars |
| `sessionCookie` | `string` | No | Session cookie (if applicable) | Encrypted, max 2048 chars |
| `lastRequestTime` | `timestamp` | No | Last API request timestamp | ISO 8601 format |
| `requestCount` | `int` | Yes | Requests in current window | >=0 |
| `rateLimitRemaining` | `int` | No | Remaining requests quota | >=0 |
| `rateLimitReset` | `timestamp` | No | When quota resets | ISO 8601 format |
| `lastError` | `string` | No | Last connection error | Max 256 chars |

### Enums

```cpp
enum class Platform {
    Bilibili,
    YouTube,
    SoundCloud   // Future
};
```

### State Transitions

```
[Disabled] → [Enabled] → [Authenticated] → [Active]
                             ↓                ↓
                         [RateLimited]    [Error]
                             ↓                ↓
                         [Active]         [Disabled]
```

### Business Rules

1. Rate limiting tracked per platform (Bilibili: ~10 req/s, YouTube varies)
2. Tokens/cookies encrypted at rest
3. Automatic re-authentication on 401/403 responses
4. Disabled platforms skip initialization
5. Request count resets according to platform's rate limit window

---

## 6. Configuration

Represents user preferences and system settings.

### Attributes

| Attribute | Type | Required | Description | Default |
|-----------|------|----------|-------------|---------|
| `audio.bufferSizeMs` | `int` | Yes | Audio buffer size in ms | 500 |
| `audio.outputDevice` | `string` | Yes | WASAPI device ID | "default" |
| `audio.sampleRate` | `int` | Yes | Target sample rate | 48000 |
| `audio.bitDepth` | `int` | Yes | Target bit depth | 16 |
| `network.cacheSizeMb` | `int` | Yes | Max cache size in MB | 1024 |
| `network.connectionTimeoutMs` | `int` | Yes | Connection timeout | 3000 |
| `network.readTimeoutMs` | `int` | Yes | Read timeout | 10000 |
| `network.retryCount` | `int` | Yes | Max retry attempts | 3 |
| `ui.language` | `string` | Yes | UI language code | "en" |
| `ui.theme` | `string` | Yes | Theme name | "light" |
| `ui.windowWidth` | `int` | Yes | Last window width | 1280 |
| `ui.windowHeight` | `int` | Yes | Last window height | 720 |
| `platforms.<platform>.enabled` | `bool` | Yes | Platform enabled | true |
| `platforms.<platform>.cookies` | `string` | No | Platform cookies | "" |

### Validation Rules

- `bufferSizeMs`: 100-5000
- `outputDevice`: Must be valid WASAPI device or "default"
- `sampleRate`: 8000, 16000, 22050, 44100, 48000, 96000, 192000
- `bitDepth`: 16, 24, 32
- `cacheSizeMb`: 0-10000
- `connectionTimeoutMs`: 100-30000
- `readTimeoutMs`: 1000-60000
- `retryCount`: 0-10
- `language`: ISO 639-1 code ("en", "zh", "ja", etc.)
- `windowWidth/Height`: 800-4096

### Business Rules

1. Configuration changes auto-save after 5-second debounce
2. Invalid values revert to defaults with warning logged
3. Audio settings changes require player restart (warn user)
4. UI settings apply immediately
5. Configuration file missing → use defaults, create new file

---

## Entity Relationship Diagram

```
┌─────────────┐
│  Category   │
│             │
│ - id        │
│ - name      │
│ - color     │
│ - sortOrder │
└──────┬──────┘
       │
       │ 1:N
       │
       ▼
┌─────────────┐
│  Playlist   │
│             │
│ - id        │◄─────┐
│ - name      │      │
│ - items[]   │      │
│ - categoryId│      │
└──────┬──────┘      │
       │             │
       │ 1:N         │ N:1
       │             │
       ▼             │
┌─────────────┐      │
│ AudioSource │      │
│             │      │
│ - id        │──────┘
│ - sourceType│
│ - url/path  │
│ - title     │
│ - duration  │
└─────────────┘

┌──────────────────┐
│  PlaybackState   │
│                  │
│ - currentSource  │──► AudioSource (reference)
│ - playlistId     │──► Playlist (reference)
│ - state          │
│ - position       │
│ - volume         │
└──────────────────┘

┌──────────────────────┐
│ PlatformConnection   │
│                      │
│ - platform           │
│ - isEnabled          │
│ - authToken          │
│ - rateLimitRemaining │
└──────────────────────┘

┌──────────────┐
│Configuration │
│              │
│ - audio.*    │
│ - network.*  │
│ - ui.*       │
│ - platforms.*│
└──────────────┘
```

---

## Data Flow Diagrams

### Playback Flow

```
User Click "Play"
    ↓
PlaybackState.state = Buffering
    ↓
Load AudioSource → Validate URL/Path
    ↓
Network/File → StreamingBuffer
    ↓
FFmpeg Decoder → AudioBuffer (PCM)
    ↓
WASAPI Playback Thread → Audio Output
    ↓
PlaybackState.state = Playing
    ↓
Update UI (signals → main thread)
```

### Playlist Persistence Flow

```
User Modifies Playlist
    ↓
PlaylistManager.savePlaylist(playlist)
    ↓
Update playlist.dateModified
    ↓
Serialize to JSON (jsoncpp)
    ↓
Atomic Write (temp file → rename)
    ↓
Backup previous version
    ↓
Emit PlaylistModified event (EventBus)
    ↓
UI Updates
```

### Network Streaming Flow

```
AudioSource (URL)
    ↓
HTTPClient.get(url) [cpp-httplib]
    ↓
Response Body Chunks (8KB-64KB)
    ↓
StreamingBuffer.write() [mutex-protected]
    ↓
Decoder Thread: StreamingBuffer.read()
    ↓
FFmpeg Decode Frame
    ↓
AudioBuffer.write() [mutex-protected]
    ↓
Playback Thread: AudioBuffer.read()
    ↓
WASAPI Output
```

---

## Persistence Strategy

### Files

- **Playlists**: `~/.bilibiliPlayer/playlists.json`
- **Configuration**: `~/.bilibiliPlayer/config.json`
- **Cache**: `~/.bilibiliPlayer/cache/<source_id>/`
- **Logs**: `~/.bilibiliPlayer/logs/app.log`

### JSON Schema Versioning

All JSON files include `"version": "1.0"` field. On version mismatch, migration logic applies.

### Backup Strategy

- Playlists backed up on save: `playlists.json.bak`
- Maximum 5 backup versions retained
- Backup rotation on application start

---

## Summary

- **6 Core Entities**: AudioSource, Playlist, Category, PlaybackState, PlatformConnection, Configuration
- **Clear Relationships**: Category → Playlist → AudioSource hierarchy
- **Validation Rules**: All attributes have defined constraints
- **State Machines**: PlaybackState and PlatformConnection have defined transitions
- **Persistence**: JSON-based with versioning and backup
- **Thread Safety**: Mutex-protected shared state (StreamingBuffer, AudioBuffer, ConfigManager)

**Data Model Complete**: Ready for contract generation (API/component interfaces).
