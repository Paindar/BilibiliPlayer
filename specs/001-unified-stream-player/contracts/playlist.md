# Playlist Management Component Contract

**Feature**: 001-unified-stream-player  
**Component**: Playlist and Category Management  
**Date**: 2025-11-13 (Updated from actual implementation)

## Purpose

Defines the interface contract for creating, managing, and persisting playlists and categories based on the actual `PlaylistManager` implementation with UUID-based identification and Qt signals.

---

## Data Structures

### CategoryInfo

```cpp
namespace playlist {
    struct CategoryInfo {
        QString name;
        QUuid uuid;  // Unique identifier
        
        bool operator==(const CategoryInfo& other) const {
            return uuid == other.uuid;
        }
    };
}

Q_DECLARE_METATYPE(playlist::CategoryInfo)
```

### PlaylistInfo

```cpp
namespace playlist {
    struct PlaylistInfo {
        QString name;
        QString creator;
        QString description;
        QString coverUri;  // Workspace-relative path or URL
        QUuid uuid;  // Unique identifier
        
        bool operator==(const PlaylistInfo& other) const {
            return uuid == other.uuid;
        }
    };
}

Q_DECLARE_METATYPE(playlist::PlaylistInfo)
```

### SongInfo

```cpp
namespace playlist {
    struct SongInfo {
        QString title;
        QString uploader;
        int platform;  // network::SupportInterface or -1 for local
        int duration;  // seconds
        QString filepath;  // Workspace-relative path or empty
        QString coverName;  // Cover image filename
        QString args;  // Platform-specific params (JSON)
        
        bool operator==(const SongInfo& other) const {
            return title == other.title &&
                   uploader == other.uploader &&
                   platform == other.platform;
        }
    };
}
```

---

## Component Interface: PlaylistManager

### Purpose
Manages all playlist and category operations with UUID-based identification, JSON persistence, and event emission via Qt signals.

### Class Definition

```cpp
class PlaylistManager : public QObject {
    Q_OBJECT
    
public:
    explicit PlaylistManager(ConfigManager* configManager, QObject* parent = nullptr);
    ~PlaylistManager();
    
    // Initialization
    void initialize();
    void loadCategoriesFromFile();
    void saveAllCategories();
    
    // Category operations (UUID-based)
    bool addCategory(const playlist::CategoryInfo& category);
    bool removeCategory(const QUuid& categoryId);
    bool updateCategory(const playlist::CategoryInfo& category);
    std::optional<playlist::CategoryInfo> getCategory(const QUuid& categoryId) const;
    QList<playlist::CategoryInfo> iterateCategories(
        std::function<bool(const playlist::CategoryInfo&)> func) const;
    bool categoryExists(const QUuid& categoryId) const;
    
    // Playlist operations (UUID-based)
    bool addPlaylist(const playlist::PlaylistInfo& playlist, const QUuid& categoryId);
    bool removePlaylist(const QUuid& playlistId, const QUuid& categoryId = QUuid());
    bool updatePlaylist(const playlist::PlaylistInfo& playlist, const QUuid& categoryId);
    std::optional<playlist::PlaylistInfo> getPlaylist(const QUuid& playlistId) const;
    QList<playlist::PlaylistInfo> iteratePlaylistsInCategory(
        const QUuid& categoryId,
        std::function<bool(const playlist::PlaylistInfo&)> func) const;
    bool playlistExists(const QUuid& playlistId) const;
    
    // Song operations
    bool addSongToPlaylist(const playlist::SongInfo& song, const QUuid& playlistId);
    bool removeSongFromPlaylist(const playlist::SongInfo& song, const QUuid& playlistId);
    bool updateSongInPlaylist(const playlist::SongInfo& song, const QUuid& playlistId);
    std::optional<playlist::SongInfo> getSongFromPlaylist(const QUuid& songId, const QUuid& playlistId) const;
    QList<playlist::SongInfo> iterateSongsInPlaylist(
        const QUuid& playlistId,
        std::function<bool(const playlist::SongInfo&)> func) const;
    
    // Current playlist management
    QUuid getCurrentPlaylist();
    void setCurrentPlaylist(const QUuid& playlistId);
    
signals:
    // Category signals
    void categoryAdded(const playlist::CategoryInfo& category);
    void categoryRemoved(const QUuid& categoryId);
    void categoryUpdated(const playlist::CategoryInfo& category);
    
    // Playlist signals
    void playlistAdded(const playlist::PlaylistInfo& playlist, const QUuid& categoryId);
    void playlistRemoved(const QUuid& playlistId, const QUuid& categoryId);
    void playlistUpdated(const playlist::PlaylistInfo& playlist, const QUuid& categoryId);
    
    // Song signals
    void songAdded(const playlist::SongInfo& song, const QUuid& playlistId);
    void songRemoved(const playlist::SongInfo& song, const QUuid& playlistId);
    void songUpdated(const playlist::SongInfo& song, const QUuid& playlistId);
    void playlistSongsChanged(const QUuid& playlistId);
    
    // System signals
    void categoriesLoaded(int categoryCount, int playlistCount);
    void currentPlaylistChanged(const QUuid& playlistId);
    
private slots:
    void onConfigChanged();
    void onAutoSaveTimer();
    
private:
    ConfigManager* m_configManager;
    mutable QReadWriteLock m_dataLock;
    
    // Core data storage
    QHash<QUuid, playlist::CategoryInfo> m_categories;
    QHash<QUuid, playlist::PlaylistInfo> m_playlists;
    QHash<QUuid, QList<QUuid>> m_categoryPlaylists;  // CategoryId -> PlaylistIds
    QHash<QUuid, QList<playlist::SongInfo>> m_playlistSongs;
    
    QTimer* m_autoSaveTimer;
    QUuid m_currentPlaylistId;
    bool m_initialized = false;
};
```

---

## Method Contracts

### Initialization

#### `initialize()`

**Preconditions**:
- ConfigManager must be configured with workspace directory
- Should be called once after construction

**Postconditions**:
- Loads categories and playlists from JSON file (via `loadCategoriesFromFile()`)
- Creates default category/playlist if none exist
- Sets up auto-save timer (default: 5 minutes)
- Emits `categoriesLoaded` signal
- `m_initialized` set to true

**Error Handling**:
- Creates empty category structure if JSON load fails
- Logs warning and continues with default setup

**Thread Safety**:
- Not thread-safe (call from main thread during startup)

**Example**:
```cpp
PlaylistManager* pm = new PlaylistManager(configManager);
pm->initialize();  // Loads playlists.json
```

---

#### `loadCategoriesFromFile()`

**Preconditions**:
- ConfigManager must provide valid `CategoryFilePath` setting

**Postconditions**:
- Reads JSON from file (default: `playlists.json` in workspace)
- Parses categories, playlists, and songs
- Populates `m_categories`, `m_playlists`, `m_categoryPlaylists`, `m_playlistSongs`
- Validates UUIDs and relationships

**Error Handling**:
- Returns silently if file doesn't exist (first run)
- Logs error if JSON parsing fails
- Skips invalid entries (missing UUIDs, malformed data)

**JSON Format**:
```json
{
  "categories": [
    {
      "uuid": "3fa85f64-5717-4562-b3fc-2c963f66afa6",
      "name": "Favorites",
      "playlists": [
        {
          "uuid": "8b7c9d1e-2f3a-4b5c-6d7e-8f9a0b1c2d3e",
          "name": "My Playlist",
          "creator": "User",
          "description": "My favorite songs",
          "coverUri": "covers/playlist1.jpg",
          "songs": [
            {
              "title": "Song Title",
              "uploader": "Artist",
              "platform": 1,
              "duration": 180,
              "filepath": "",
              "coverName": "cover1.jpg",
              "args": "{\"bvid\":\"BV1x7411F744\",\"cid\":7631925,\"qn\":80}"
            }
          ]
        }
      ]
    }
  ]
}
```

---

#### `saveAllCategories()`

**Preconditions**:
- None (can be called anytime)

**Postconditions**:
- Serializes all categories, playlists, songs to JSON
- Writes to `CategoryFilePath` (from ConfigManager)
- Uses workspace-relative paths (via `ConfigManager::getAbsolutePath`)
- Atomic write (temp file + rename)

**Error Handling**:
- Logs error if file write fails
- Does not throw exceptions
- Returns false on failure

**Threading**:
- Thread-safe (read lock)
- Can be called from auto-save timer thread

---

### Category Operations

#### `addCategory(category)`

**Preconditions**:
- `category.name` must be non-empty and unique
- `category.uuid` must be null or unique (will generate if null)

**Postconditions**:
- Category added to `m_categories`
- Empty playlist list created in `m_categoryPlaylists`
- `categoryAdded` signal emitted
- Auto-save triggered (debounced)
- Returns true

**Error Handling**:
- Returns false if name is empty or duplicate UUID
- No signal emitted on failure

**Thread Safety**:
- Thread-safe (write lock)

**Example**:
```cpp
playlist::CategoryInfo category;
category.name = "Rock Music";
category.uuid = QUuid::createUuid();
if (pm->addCategory(category)) {
    qDebug() << "Category added:" << category.uuid;
}
```

---

#### `removeCategory(categoryId)`

**Preconditions**:
- `categoryId` must be valid existing category

**Postconditions**:
- Category removed from `m_categories`
- **Playlists in category are NOT deleted** (become uncategorized)
- Playlist-category associations removed from `m_categoryPlaylists`
- `categoryRemoved` signal emitted
- Auto-save triggered
- Returns true

**Error Handling**:
- Returns false if category not found

**Example**:
```cpp
pm->removeCategory(categoryUuid);
// Playlists in that category still exist, just uncategorized
```

---

#### `updateCategory(category)`

**Preconditions**:
- `category.uuid` must match existing category
- `category.name` must be non-empty

**Postconditions**:
- Category name/metadata updated in `m_categories`
- `categoryUpdated` signal emitted
- Auto-save triggered
- Returns true

**Error Handling**:
- Returns false if UUID not found
- Logs warning if name is empty (no change)

---

#### `getCategory(categoryId)`

**Preconditions**:
- None

**Postconditions**:
- Returns `std::optional<CategoryInfo>`
- Contains category if found, empty optional if not found

**Thread Safety**:
- Thread-safe (read lock)

**Example**:
```cpp
auto catOpt = pm->getCategory(uuid);
if (catOpt.has_value()) {
    qDebug() << "Category:" << catOpt->name;
}
```

---

#### `iterateCategories(func)`

**Preconditions**:
- `func` is callable with signature `bool(const CategoryInfo&)`
- `func` returns true to continue iteration, false to stop

**Postconditions**:
- Calls `func` for each category
- Returns list of categories where `func` returned true
- Order not guaranteed (QHash iteration order)

**Thread Safety**:
- Thread-safe (read lock)
- `func` should NOT call PlaylistManager methods (deadlock risk)

**Example**:
```cpp
QList<playlist::CategoryInfo> allCats = pm->iterateCategories(
    [](const playlist::CategoryInfo& cat) { return true; }
);
```

---

### Playlist Operations

#### `addPlaylist(playlist, categoryId)`

**Preconditions**:
- `playlist.name` must be non-empty
- `playlist.uuid` must be null or unique (will generate if null)
- `categoryId` must be valid category UUID or null (uncategorized)

**Postconditions**:
- Playlist added to `m_playlists`
- Playlist UUID added to `m_categoryPlaylists[categoryId]` list
- Empty song list created in `m_playlistSongs`
- `playlistAdded` signal emitted with playlist and categoryId
- Auto-save triggered
- Returns true

**Error Handling**:
- Returns false if name is empty or duplicate UUID
- If categoryId invalid, playlist added as uncategorized

**Example**:
```cpp
playlist::PlaylistInfo pl;
pl.name = "Workout Mix";
pl.creator = "Me";
pl.uuid = QUuid::createUuid();
if (pm->addPlaylist(pl, categoryUuid)) {
    qDebug() << "Playlist added:" << pl.uuid;
}
```

---

#### `removePlaylist(playlistId, categoryId)`

**Preconditions**:
- `playlistId` must be valid existing playlist
- `categoryId` is optional (for validation)

**Postconditions**:
- Playlist removed from `m_playlists`
- Playlist UUID removed from all categories in `m_categoryPlaylists`
- Songs in playlist removed from `m_playlistSongs`
- `playlistRemoved` signal emitted with playlistId and categoryId
- If playlist is current playlist, `m_currentPlaylistId` reset
- Auto-save triggered
- Returns true

**Error Handling**:
- Returns false if playlist not found

**Example**:
```cpp
pm->removePlaylist(playlistUuid);
// All songs in playlist are deleted
```

---

#### `updatePlaylist(playlist, categoryId)`

**Preconditions**:
- `playlist.uuid` must match existing playlist
- `categoryId` must be valid (can change playlist's category)

**Postconditions**:
- Playlist metadata updated in `m_playlists`
- If categoryId changed, playlist moved to new category in `m_categoryPlaylists`
- `playlistUpdated` signal emitted
- Auto-save triggered
- Returns true

**Error Handling**:
- Returns false if playlist UUID not found

**Example**:
```cpp
auto plOpt = pm->getPlaylist(playlistUuid);
if (plOpt.has_value()) {
    plOpt->description = "Updated description";
    pm->updatePlaylist(*plOpt, newCategoryUuid);
}
```

---

#### `iteratePlaylistsInCategory(categoryId, func)`

**Preconditions**:
- `categoryId` must be valid
- `func` is callable with signature `bool(const PlaylistInfo&)`

**Postconditions**:
- Calls `func` for each playlist in category
- Returns list of playlists where `func` returned true

**Thread Safety**:
- Thread-safe (read lock)

**Example**:
```cpp
QList<playlist::PlaylistInfo> playlists = pm->iteratePlaylistsInCategory(
    categoryUuid,
    [](const playlist::PlaylistInfo& pl) {
        return pl.creator == "Me";  // Filter by creator
    }
);
```

---

### Song Operations

#### `addSongToPlaylist(song, playlistId)`

**Preconditions**:
- `playlistId` must be valid existing playlist
- `song` must have non-empty title

**Postconditions**:
- Song appended to end of `m_playlistSongs[playlistId]`
- `songAdded` signal emitted
- `playlistSongsChanged` signal emitted
- Auto-save triggered
- Returns true

**Error Handling**:
- Returns false if playlist not found
- Duplicate songs allowed (user may want same song multiple times)

**Thread Safety**:
- Thread-safe (write lock)

**Example**:
```cpp
playlist::SongInfo song;
song.title = "Song Title";
song.uploader = "Artist";
song.platform = network::SupportInterface::Bilibili;
song.duration = 180;
song.args = R"({"bvid":"BV1x7411F744","cid":7631925,"qn":80})";
pm->addSongToPlaylist(song, playlistUuid);
```

---

#### `removeSongFromPlaylist(song, playlistId)`

**Preconditions**:
- `playlistId` must be valid
- `song` must match existing song (by equality operator)

**Postconditions**:
- **First matching song** removed from `m_playlistSongs[playlistId]`
- `songRemoved` signal emitted
- `playlistSongsChanged` signal emitted
- Auto-save triggered
- Returns true

**Error Handling**:
- Returns false if playlist not found or song not in playlist
- Only removes first match (if duplicates exist)

---

#### `updateSongInPlaylist(song, playlistId)`

**Preconditions**:
- `playlistId` must be valid
- `song` must match existing song (finds by equality, updates fields)

**Postconditions**:
- Song metadata updated in `m_playlistSongs[playlistId]`
- `songUpdated` signal emitted
- `playlistSongsChanged` signal emitted
- Auto-save triggered
- Returns true

**Error Handling**:
- Returns false if playlist not found or song not found

---

#### `iterateSongsInPlaylist(playlistId, func)`

**Preconditions**:
- `playlistId` must be valid
- `func` is callable with signature `bool(const SongInfo&)`

**Postconditions**:
- Calls `func` for each song in playlist (in order)
- Returns list of songs where `func` returned true

**Thread Safety**:
- Thread-safe (read lock)

**Example**:
```cpp
QList<playlist::SongInfo> songs = pm->iterateSongsInPlaylist(
    playlistUuid,
    [](const playlist::SongInfo& s) { return true; }  // Get all songs
);
```

---

### Current Playlist Management

#### `setCurrentPlaylist(playlistId)`

**Preconditions**:
- `playlistId` must be valid or null (clear current)

**Postconditions**:
- `m_currentPlaylistId` updated
- `currentPlaylistChanged` signal emitted
- Saved to config (on next auto-save)

**Thread Safety**:
- Thread-safe (write lock)

**Example**:
```cpp
pm->setCurrentPlaylist(playlistUuid);
// AudioPlayerController will receive currentPlaylistChanged signal
```

---

#### `getCurrentPlaylist()`

**Preconditions**:
- None

**Postconditions**:
- Returns current playlist UUID or null UUID if none set

**Thread Safety**:
- Thread-safe (read lock)

---

## Signal Contracts

### `playlistSongsChanged(const QUuid& playlistId)`

**Emitted When**:
- Song added to playlist
- Song removed from playlist
- Song updated in playlist

**Use Case**:
- AudioPlayerController subscribes to reload playlist if currently playing
- UI playlist view subscribes to refresh song list

**Example**:
```cpp
connect(pm, &PlaylistManager::playlistSongsChanged,
    [this](const QUuid& plId) {
        if (plId == currentDisplayedPlaylistId) {
            refreshPlaylistView();
        }
    });
```

---

### `categoriesLoaded(int categoryCount, int playlistCount)`

**Emitted When**:
- `initialize()` completes
- `loadCategoriesFromFile()` completes

**Parameters**:
- `categoryCount`: Number of categories loaded
- `playlistCount`: Total playlists across all categories

**Use Case**:
- UI waits for this signal before displaying playlist tree

---

## Auto-Save Mechanism

### Configuration

Controlled by ConfigManager settings:
- `AutoSaveEnabled`: bool (default: true)
- `AutoSaveInterval`: int (minutes, default: 5)

### Behavior

- Auto-save timer triggered on any modification (add/remove/update)
- Timer is debounced (resets on each modification)
- Actual save occurs after `AutoSaveInterval` of inactivity
- Manual `saveAllCategories()` always saves immediately

**Example**:
```ini
[PlaylistManager]
AutoSaveEnabled=true
AutoSaveInterval=5  # Save after 5 minutes of no changes
```

---

## Persistence Format

### JSON Schema

```json
{
  "categories": [
    {
      "uuid": "string (UUID)",
      "name": "string",
      "playlists": [
        {
          "uuid": "string (UUID)",
          "name": "string",
          "creator": "string",
          "description": "string",
          "coverUri": "string (workspace-relative path)",
          "songs": [
            {
              "title": "string",
              "uploader": "string",
              "platform": "int",
              "duration": "int (seconds)",
              "filepath": "string (workspace-relative)",
              "coverName": "string",
              "args": "string (JSON)"
            }
          ]
        }
      ]
    }
  ]
}
```

### Workspace-Relative Paths

All file paths stored relative to workspace directory:
- Converted to absolute via `ConfigManager::getAbsolutePath()`
- Validated via `ConfigManager::validateRelativePath()`
- Ensures portability across systems

**Example**:
```json
{
  "coverUri": "covers/playlist1.jpg",  // Relative
  "filepath": "music/song1.mp3"        // Relative
}
```

At runtime:
```cpp
QString absPath = configManager->getAbsolutePath(song.filepath);
// D:/Project/BilibiliPlayer/workspace/music/song1.mp3
```

---

## Thread Safety

### Synchronization Mechanism

Uses `QReadWriteLock` for reader-writer pattern:
- **Read operations** (get, iterate, exists): Shared read lock (multiple readers)
- **Write operations** (add, remove, update): Exclusive write lock (single writer)

### Lock Hierarchy

1. **Read Lock**: `m_dataLock.lockForRead()`
   - Used by: `getCategory`, `getPlaylist`, `iterate*`, `*Exists`
   - Multiple threads can read concurrently

2. **Write Lock**: `m_dataLock.lockForWrite()`
   - Used by: `addCategory`, `removePlaylist`, `addSongToPlaylist`, etc.
   - Exclusive access (blocks all readers and writers)

### Deadlock Prevention

- **Do NOT call PlaylistManager methods from iteration callbacks**
- Callbacks execute while holding read lock
- Calling write methods would deadlock

**Example (WRONG)**:
```cpp
pm->iterateCategories([pm](const CategoryInfo& cat) {
    pm->removeCategory(cat.uuid);  // DEADLOCK! Already holding read lock
    return true;
});
```

**Example (CORRECT)**:
```cpp
QList<QUuid> toRemove;
pm->iterateCategories([&toRemove](const CategoryInfo& cat) {
    if (cat.name.startsWith("Temp")) {
        toRemove.append(cat.uuid);
    }
    return true;
});
// Now remove outside iteration
for (const QUuid& uuid : toRemove) {
    pm->removeCategory(uuid);
}
```

---

## Integration with ConfigManager

### Configuration Keys

```ini
[PlaylistManager]
CategoryFilePath=playlists.json
AutoSaveEnabled=true
AutoSaveInterval=5

[Workspace]
WorkspaceDirectory=D:/Project/BilibiliPlayer/workspace
```

### Workspace-Relative Paths

```cpp
// Saving
QString relPath = configManager->validateRelativePath(absPath);
song.filepath = relPath;  // Store relative

// Loading
QString absPath = configManager->getAbsolutePath(song.filepath);
QFile file(absPath);  // Use absolute
```

---

## Integration with AudioPlayerController

### Loading Playlist for Playback

```cpp
// User double-clicks playlist in UI
connect(playlistView, &PlaylistView::playlistActivated,
    [this, audioPlayer](const QUuid& plId) {
        audioPlayer->playPlaylist(plId, 0);
    });

// AudioPlayerController::playPlaylist implementation
void AudioPlayerController::playPlaylist(const QUuid& plId, int idx) {
    auto songs = playlistManager->iterateSongsInPlaylist(plId,
        [](const SongInfo&) { return true; });
    m_currentPlaylist = songs;
    m_currentIndex = idx;
    playCurrentSongUnsafe();
}
```

### Reloading on Changes

```cpp
// AudioPlayerController subscribes to playlist changes
connect(playlistManager, &PlaylistManager::playlistSongsChanged,
    this, &AudioPlayerController::onPlaylistChanged);

void AudioPlayerController::onPlaylistChanged(const QUuid& plId) {
    if (plId == m_currentPlaylistId) {
        // Reload playlist, preserve current index if possible
        auto songs = playlistManager->iterateSongsInPlaylist(plId, /*...*/);
        m_currentPlaylist = songs;
    }
}
```

---

## Error Handling Patterns

### Validation Errors

```cpp
playlist::SongInfo song;
song.title = "";  // Invalid!
if (!pm->addSongToPlaylist(song, playlistId)) {
    QMessageBox::warning(this, "Error", "Song title cannot be empty");
}
```

### File I/O Errors

```cpp
pm->initialize();  // Loads playlists.json
// If JSON corrupt or missing, default category created
// No exception thrown, logs warning
```

---

## Performance Considerations

1. **Iteration Performance**: O(n) where n = number of items
   - Categories: Typically < 20
   - Playlists per category: Typically < 50
   - Songs per playlist: Can be 100-1000s

2. **Auto-Save Debouncing**: Prevents excessive file writes
   - Batch modifications trigger single save after 5 min
   - Manual save always immediate

3. **JSON Parsing**: Synchronous on load
   - Typical load time: < 100ms for 10 categories, 50 playlists, 1000 songs
   - Async initialization recommended for large libraries

4. **Memory Usage**: All playlists loaded in memory
   - 1000 songs ≈ 500KB RAM (SongInfo structs)
   - Acceptable for desktop application

---

## Testing Considerations

See test file:
- `test/playlist_manager_test.cpp`: CRUD operations, persistence, thread safety

**Test Coverage**:
- Category CRUD
- Playlist CRUD with category associations
- Song CRUD within playlists
- JSON save/load round-trip
- UUID uniqueness validation
- Workspace-relative path conversion

---

## Future Enhancements

### Smart Playlists

```cpp
bool addSmartPlaylist(const QString& name, const QString& filterExpression);
// filterExpression: "uploader == 'Artist' AND duration > 180"
// Auto-populates songs matching filter
```

### Playlist Import/Export

```cpp
bool exportPlaylist(const QUuid& playlistId, const QString& filePath);
// Export to M3U/PLS format
bool importPlaylist(const QString& filePath);
// Import from external playlist file
```

### Undo/Redo Support

```cpp
class PlaylistCommand : public QUndoCommand {
    // Implement undo/redo for playlist operations
};
```

---

## Thread Safety Summary

| Operation | Lock Type | Concurrent Reads | Notes |
|-----------|-----------|------------------|-------|
| `add*` | Write | Blocked | Exclusive |
| `remove*` | Write | Blocked | Exclusive |
| `update*` | Write | Blocked | Exclusive |
| `get*` | Read | Allowed | Multiple readers |
| `iterate*` | Read | Allowed | Do NOT modify during iteration |
| `saveAllCategories` | Read | Allowed | Read-only serialization |

---
