# Data Model: UI Fixes, Localization, and Playlist Navigation

**Date**: 2025-11-19  
**Feature**: `004-ui-fixes-localization`  
**Status**: Complete

## Entity Definitions

### 1. SongInfo (AudioItem in spec)

**Purpose**: Represents a single audio track in the playlist with metadata, platform information, and playback state.

**Actual Implementation** (`src/playlist/playlist.h`): `struct SongInfo`

**Fields**:

| Field | Type | Constraints | Notes |
|-------|------|-----------|-------|
| `title` | `std::string` | Non-empty, max 500 chars | Track display name from source |
| `uploader` | `std::string` | Any string | Artist/channel name |
| `platform` | `int` | PlatformType enum value | Platform identifier (not string; enum from `i_platform.h`) |
| `duration` | `int` | >= 0 | Duration in seconds (not milliseconds) |
| `filepath` | `std::string` | Valid path or stream ID | Local file path or platform stream identifier |
| `coverName` | `std::string` | Filename or empty | Cached cover art filename in resource directory |
| `args` | `std::string` | JSON or empty | Platform-specific arguments for playback/streaming |

**Key Changes for This Feature**:
- **platform**: Use `int` PlatformType enum, NOT string platformName. Convert enum to translatable string for display via `tr(PlatformTypeToString(platform))`
- **duration**: Store in seconds (not milliseconds)
- **filepath vs uri**: `filepath` used for local files; use `args` field to store `interfaceData` for platform-specific streaming parameters
- **Validation**: Reject SongInfo if `platform` is unrecognized enum value

**Relationships**:
- Belongs to `Playlist` (parent-child)
- Originated from `SearchResult` (if added via search)
- Referenced by `PlaylistManager` current position tracking

**State Transitions**:
- `PENDING_METADATA` → `READY` (after metadata fetch)
- `READY` → `PLAYING` (when playback starts)
- `PLAYING` → `PAUSED` (user pauses)
- `ANY` → `DELETED` (user removes from playlist)

---

### 2. PlaylistInfo (Playlist in spec)

**Purpose**: Container for SongInfo objects with navigation state and playback mode configuration.

**Actual Implementation** (`src/playlist/playlist_manager.h`): Managed by `PlaylistManager` with `PlaylistInfo` struct and `m_playlistSongs` map

**Fields** (from `PlaylistInfo`):

| Field | Type | Constraints | Notes |
|-------|------|-----------|-------|
| `id` | `std::string` | Non-empty, unique | User-created or auto-generated playlist identifier |
| `name` | `std::string` | Non-empty, max 100 chars | User-friendly name for playlist |

**Manager State** (from `PlaylistManager`):

| State | Type | Constraints | Notes |
|-------|------|-----------|-------|
| `m_playlistSongs` | `std::map<std::string, std::vector<SongInfo>>` | Playlist ID → Songs | Ordered list of audio tracks per playlist |
| `currentPlaylistId` | `std::string` | Valid or empty | Currently selected playlist |
| `currentIndex` | `int` | -1 to songs.size()-1 | Current playing position per playlist |
| `playbackMode` | `enum` | NORMAL, SHUFFLE, REPEAT_ONE, REPEAT_ALL | Current playback mode |

**Key Changes for This Feature**:
- **currentIndex Validation**: After deletion, verify `currentIndex` still valid for current playlist
  - If `currentIndex >= songs.size()`, set to `songs.size() - 1`
  - If playlist becomes empty, set to `-1`
- **Deletion Safety**: When item at `currentIndex` is deleted:
  - Emit signal to player about current item deletion
  - Adjust index to valid state or trigger next-track playback
  - If random mode: regenerate randomPlayOrder for reduced playlist size
- **Navigation Logic**: `last()` and `next()` methods must respect playback mode and handle edge cases

**Relationships**:
- Contains multiple `AudioItem` objects
- Has one `PlaybackMode` configuration
- Referenced by `PlaylistManager` for all operations
- Persisted to JSON or SQLite (Phase 2)

**State Invariants**:
- `items.size() >= 0` (always)
- `-1 <= currentIndex < items.size()` (always valid or -1)
- If `items.empty()`, then `currentIndex == -1`
- If `currentIndex >= 0`, then `items[currentIndex]` is valid

---

### 3. SearchResult

**Purpose**: Single result item returned from search operation, containing metadata for adding to playlist.

**Actual Implementation** (`src/network/platform/i_platform.h`): `struct SearchResult`

**Fields**:

| Field | Type | Constraints | Notes |
|-------|------|-----------|-------|
| `title` | `std::string` | Non-empty | Track title from search |
| `uploader` | `std::string` | Any string | Artist/channel name from source |
| `platform` | `PlatformType` enum | Valid enum value | Platform identifier (not string) |
| `duration` | `int` | >= 0 | Track duration in seconds |
| `coverUrl` | `std::string` | Valid URL or empty | Thumbnail/cover art URL |
| `coverImg` | `QPixmap` or cache | Image data | Pre-loaded cover image if available |
| `description` | `std::string` | Any string | Track description or metadata |
| `interfaceData` | `std::string` | JSON or platform format | Platform-specific data for streaming/playback |

**Key Changes for This Feature**:
- **platform**: Use `PlatformType` enum (from `i_platform.h`), NOT string platformName
- **duration**: Already in seconds (matches SongInfo)
- **interfaceData**: Store platform-specific parameters here (transfer to SongInfo.args on add-to-playlist)
- **Validation**: Reject SearchResult if `platform` is invalid enum value
- **Transfer**: When SearchResult → SongInfo, map `platform` directly and copy `interfaceData` to `args`

**Relationships**:
- Created by `SearchService` / `BilibiliPlatform`
- Converted to `AudioItem` when user adds to playlist
- Displayed in SearchPage UI

---

### 4. UIString & Platform Name Localization

**Purpose**: Represents any user-visible text in the application that must use localization via `tr()`.

**Fields**:

| Field | Type | Constraints | Notes |
|-------|------|-----------|-------|
| `context` | `const char*` | Non-empty | Qt context for disambiguation (class name) |
| `text` | `const char*` | Non-empty | English text to translate |
| `localized` | `QString` | Localized text or English | Result of `QCoreApplication::translate(context, text)` |

**Platform Name Localization** (from C5 clarification):
- Platform names (e.g., "Bilibili", "YouTube") are user-facing and must be translatable
- Implementation: Wrap platform enum display names with `tr()` when showing in UI
- Example: `tr("Bilibili")` displays as "B站" (Chinese), "Bilibili" (English), etc.
- Store platform enum as `int` in SongInfo; convert to translatable string only for display

**Key Changes for This Feature**:
- **Audit**: Identify all hardcoded strings in UI components and platform display names
- **Marking**: Wrap with `tr()` macro or `QCoreApplication::translate()`
- **Platform Names**: Create helper function to convert `PlatformType` enum to translatable string
- **Extraction**: Run `lupdate` to extract and update `.ts` files
- **Fallback**: Missing translations show raw English string (C3 clarification)

**Validation Rules**:
- No hardcoded English strings in UI source code (except `tr()` arguments)
- All user-visible strings must use `tr()`
- `tr()` calls must have string literals (not variables)
- Context should be class name for disambiguation
- Platform enum values must have corresponding `tr()` entries in translation files

**Relationships**:
- Used by `SearchPage`, `PlaylistPage`, all UI dialogs/widgets
- Loaded from `resource/lang/*.qm` files at runtime

---

### 5. AsyncSearchOperation

**Purpose**: Encapsulates a search operation running on a worker thread with cancellation support for rapid navigation.

**Fields**:

| Field | Type | Constraints | Notes |
|-------|------|-----------|-------|
| `query` | `std::string` | Non-empty, max 200 chars | Search query from user |
| `threadId` | `QThread::id()` | Valid thread ID | Worker thread running search |
| `results` | `std::vector<SearchResult>` | 0..N results | Populated as search completes |
| `cancelled` | `std::atomic<bool>` | true/false | Signal to cancel operation (C6 clarification) |
| `isComplete` | `std::atomic<bool>` | true/false | Operation finished |

**Key Changes for This Feature**:
- **Non-Blocking**: Search runs on worker thread, signals emit when complete (C2)
- **Cancellation**: Can be cancelled via `cancelled` atomic flag when user navigates away (C6)
- **Progress**: Emits progress signals for UI status updates, shown in SearchingPage (C2)
- **Cleanup**: Parent navigation away cancels pending async operations to ensure responsiveness (C6)

**Rapid Navigation Handling** (from C6 clarification):
- When user navigates away from SearchPage while search in progress:
  1. Set `cancelled = true` on active AsyncSearchOperation
  2. Worker thread checks flag periodically and exits early if set
  3. UI responds immediately to navigation (smooth responsiveness)
  4. Any pending signals are safely discarded (avoid use-after-free)
- Implementation: Use `std::atomic<bool>` for thread-safe flag checking without locks

**Relationships**:
- Created by `SearchService` / `SearchPage`
- Runs via `QThread` with signal/slot communication
- Emits signals when progress/complete

---

## Data Validation Rules

### AudioItem Validation

```
MUST NOT create AudioItem where:
  - title is empty
  - platformName is empty
  - platformName is "Unknown"
  - uri is empty or invalid

SHOULD validate during construction:
  - platformName must match known platforms or be explicitly allowed
  - duration >= 0
  - All string fields max length enforced
```

### SearchResult Validation

```
MUST NOT return SearchResult where:
  - platformName is empty or "Unknown"
  - title is empty
  - uri is invalid for streaming

Platform implementation (e.g., BilibiliPlatform) MUST:
  - Set platformName to platform identifier
  - Never return "Unknown" as platformName
```

### Playlist Navigation Validation

```
After any operation modifying items:
  - Verify: -1 <= currentIndex < items.size()
  - If violation: Adjust to valid state
  - If items become empty: Set currentIndex = -1
  - Emit stateChanged signal for UI update

Navigation methods (last/next) MUST:
  - Check currentIndex validity before use
  - Respect playbackMode when calculating next position
  - Handle shuffle and repeat modes correctly
  - Never throw exceptions for invalid state - return safe default
```

---

## State Transitions Diagram

### Playlist Item Deletion Flow (from C1 clarification)

```
User deletes item from playlist:
  1. Validate item exists and is valid
  2. Check if item is currently playing
  3. Remove item from playlist
  4. Validate currentIndex still valid:
     - If deleted item was at currentIndex:
         a. If random mode: regenerate randomPlayOrder (C4)
         b. If next item exists: emit "playNext" signal
         c. If no items left: emit "stop" signal
     - Else: adjust currentIndex if needed
  5. If currentIndex now invalid:
     - Set to items.size() - 1 (if items not empty)
     - Set to -1 (if items now empty)
  6. Emit "playlistModified" signal
  7. UI updates display and player state

Expected Outcomes (C1):
  - Playing item deleted → play next immediately (if available)
  - Last item deleted in empty playlist → stop playback
  - Random mode after deletion → regenerate play order (C4)
```

### Search Operation Flow (from C2 & C6 clarifications)

```
User initiates search:
  1. Create AsyncSearchOperation on worker thread
  2. Show SearchingPage in contentStack (C2)
  3. Emit "searchStarted" signal (UI shows loading state)
  4. Worker thread calls BilibiliPlatform::search()
  5. For each result:
     - BilibiliPlatform sets platform enum correctly
     - Worker checks cancelled flag (C6)
     - If cancelled: exit early with partial results
     - Worker emits "searchProgress" with partial results
  6. When complete:
     - Worker emits "searchCompleted" with all results
     - Signal safely queued on UI thread
  7. UI displays results (non-blocking)
  8. User can interact during entire process
  9. User navigates away while search pending (C6):
     - Set cancelled = true
     - Worker thread exits
     - New page loads immediately
     - Smooth UI responsiveness ensured
  10. User clicks "Add" on result:
     - SearchResult → SongInfo (platform enum copied)
     - Add to playlist
     - Hide SearchingPage
```

---

## Relationships Diagram

```
SearchPage
    ↓
    └─ Search query → BilibiliPlatform::search()
                         ↓
                    SearchResult (with platformName)
                         ↓
    ┌─ User clicks "Add" ─┴─→ AudioItem (platformName preserved)
    ↓                           ↓
PlaylistPage                 Playlist (append item)
    ↓                           ↓
Displays items ←─ PlaylistManager (navigation, deletion)
                                ↓
                    Button clicks: next(), last()
                    Validates currentIndex
                    Respects playbackMode
                    Handles deletion safely
```

---

**Data Model Status**: Complete. Ready for contract generation.
