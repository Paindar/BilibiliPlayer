# Platform Metadata Contract

**Date**: 2025-11-19  
**Feature**: `004-ui-fixes-localization`  
**Component**: Platform Name Data Flow

## Purpose

Establish contract for ensuring platform name information flows correctly from search results through playlist addition to display.

## Data Flow

```
BilibiliPlatform::search()
    ↓
    Creates SearchResult with platformName = "Bilibili"
    ↓
SearchPage displays results
    ↓
User clicks "Add to Playlist"
    ↓
SearchResult → AudioItem (platform name copied)
    ↓
AudioItem added to Playlist
    ↓
PlaylistPage displays with correct platform name
```

## API Contract

### SearchResult Creation in Platform Implementation

```cpp
// In BilibiliPlatform::search():
std::vector<SearchResult> results;

// For each result from API:
SearchResult item;
item.title = /* from API */;
item.platformName = "Bilibili";  // ✅ REQUIRED - set explicitly
item.uri = /* streaming URL */;
item.duration = /* in milliseconds */;
item.coverUrl = /* thumbnail URL */;
item.platformId = /* API internal ID */;

// NEVER do this:
// item.platformName = "Unknown";  // ❌ ANTI-PATTERN
// item.platformName = "";         // ❌ ANTI-PATTERN

results.push_back(item);
```

### AudioItem Creation from SearchResult

```cpp
// In SearchPage or PlaylistManager::addFromSearch():

// Receive SearchResult from search operation
const SearchResult& searchResult = /* from search results */;

// Validate platform name is set
if (searchResult.platformName.empty()) {
    throw std::runtime_error("SearchResult missing platform name");
}

// Create AudioItem with platform name preserved
AudioItem item;
item.title = searchResult.title;
item.platformName = searchResult.platformName;  // ✅ MUST copy
item.uri = searchResult.uri;
item.duration = searchResult.duration;
item.coverUrl = searchResult.coverUrl;

// Add to playlist
playlist->addItem(item);  // Platform name preserved
```

### Display Contract

```cpp
// In PlaylistPage or playlist view:

// Display AudioItem with platform name
QString displayText = QString("%1 [%2]")
    .arg(QString::fromStdString(item.title))
    .arg(QString::fromStdString(item.platformName));

// OR separate column:
tableWidget->setItem(row, PLATFORM_COLUMN, 
    new QTableWidgetItem(QString::fromStdString(item.platformName)));

// Verify at display time:
if (item.platformName.empty()) {
    displayText = item.title + " [Unknown]";  // Fallback visible to users
    logWarning("AudioItem has empty platform name");
}
```

## Data Structures

### SearchResult

```cpp
struct SearchResult {
    std::string title;
    std::string platformName;      // MUST be non-empty
    std::string uri;
    int64_t duration;              // milliseconds
    std::string coverUrl;
    std::string platformId;        // Platform-specific ID
    
    // Validation
    bool isValid() const {
        return !title.empty() && 
               !platformName.empty() && 
               !uri.empty();
    }
};
```

### AudioItem (relevant fields)

```cpp
class AudioItem {
public:
    std::string getId() const { return m_id_; }
    std::string getTitle() const { return m_title_; }
    std::string getPlatformName() const { return m_platformName_; }
    std::string getUri() const { return m_uri_; }
    
    void setPlatformName(const std::string& name) {
        if (name.empty()) {
            throw std::invalid_argument("Platform name cannot be empty");
        }
        m_platformName_ = name;
    }
    
private:
    std::string m_id_;
    std::string m_title_;
    std::string m_platformName_;     // MUST be non-empty
    std::string m_uri_;
    // ... other fields
};
```

## Platform Identifier Registry

**Known Platforms** (extendable):

| Platform | Identifier | Region | Notes |
|----------|------------|--------|-------|
| Bilibili | `"Bilibili"` | China | Primary platform for this project |
| YouTube | `"YouTube"` | Global | Future expansion |
| Local | `"Local"` | N/A | Local files |

**Implementation**:
```cpp
enum class PlatformType {
    BILIBILI,
    YOUTUBE,
    LOCAL,
    UNKNOWN  // Should not be used if platform can be determined
};

std::string platformTypeToString(PlatformType type) {
    switch (type) {
        case PlatformType::BILIBILI: return "Bilibili";
        case PlatformType::YOUTUBE: return "YouTube";
        case PlatformType::LOCAL: return "Local";
        case PlatformType::UNKNOWN: return "Unknown";
    }
}
```

## Requirements

**FR-004**: System MUST display the correct platform name for audio items added from search results instead of "Unknown"

## Validation & Testing

### Validation Rules

```cpp
// Before storing SearchResult or AudioItem:

// ✅ VALID:
item.platformName = "Bilibili";  // Known platform
item.platformName = "YouTube";   // Known platform

// ❌ INVALID:
item.platformName = "Unknown";   // Explicitly forbidden
item.platformName = "";          // Empty string forbidden
item.platformName = "bilibili";  // Case sensitivity (use "Bilibili")
```

### Test Cases

- **TC-001**: SearchResult created by BilibiliPlatform has platformName = "Bilibili"
- **TC-002**: AudioItem created from SearchResult preserves platform name
- **TC-003**: AudioItem.setPlatformName() rejects empty strings
- **TC-004**: PlaylistPage displays platform name correctly (not "Unknown")
- **TC-005**: Multiple items from search display their respective platform names
- **TC-006**: Playlist persists and restores platform names correctly
- **TC-007**: Search result with empty platformName is rejected with error
- **TC-008**: Platform name is localized in UI display (if needed)

### Code Review Criteria

- SearchResult always has non-empty platformName from platform implementation
- AudioItem creation validates platformName is non-empty
- Platform name transferred from SearchResult → AudioItem during add
- Display code never shows "Unknown" except as error case
- All SearchResult objects logged if platformName is empty (for debugging)

## Implementation Notes

- **Scope**: BilibiliPlatform search(), SearchPage, PlaylistManager, AudioItem, PlaylistPage
- **Files affected**: 
  - `src/network/platform/bili_network_interface.cpp` (set platform name)
  - `src/playlist/audio_item.h/cpp` (store, validate)
  - `src/ui/pages/search_page.cpp` (display)
  - `src/playlist/playlist_manager.cpp` (transfer SearchResult → AudioItem)
- **Impact**: Medium - data flow change, no logic changes
- **Breaking changes**: AudioItem now requires platformName
- **Testing**: Unit tests verify data flow, integration tests verify UI

## Persistence

If using JSON storage:
```json
{
  "items": [
    {
      "id": "unique-id",
      "title": "Video Title",
      "platformName": "Bilibili",
      "uri": "bilibili://video/...",
      "duration": 300000,
      "addedAt": "2025-11-19T10:00:00Z"
    }
  ]
}
```

If using SQLite:
```sql
CREATE TABLE audio_items (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    platform_name TEXT NOT NULL,  -- MUST be non-empty
    uri TEXT NOT NULL,
    duration INTEGER,
    added_at TIMESTAMP
);

-- Constraint to prevent empty platform names
ALTER TABLE audio_items 
ADD CONSTRAINT chk_platform_name 
CHECK (platform_name != '' AND platform_name != 'Unknown');
```

## Success Criteria (from spec)

- **SC-003**: Audio items added from search results display the correct platform name (matching source) in all views

---

**Contract Status**: Ready for implementation.
