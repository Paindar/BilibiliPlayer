# Playlist Navigation Contract

**Date**: 2025-11-19  
**Feature**: `004-ui-fixes-localization`  
**Component**: Playlist Navigation After Deletion

## Purpose

Establish contract for maintaining correct playlist navigation behavior (last/next buttons) after deleting items and during mode changes.

## State Management

### Invariants

The following invariants MUST always hold after any operation:

```cpp
// State invariants for Playlist:
assert(items.size() >= 0);                    // Always valid
assert(-1 <= currentIndex && currentIndex < items.size());  // Always valid or -1
assert(items.empty() ? currentIndex == -1 : true);          // Empty playlist = no selection
assert(currentIndex >= 0 ? currentIndex < items.size() : true);  // If selected, in range
```

## API Contract

### Deletion Operation

```cpp
// In PlaylistManager or Playlist:

bool deleteItem(const std::string& itemId) {
    // Find item
    auto it = std::find_if(items.begin(), items.end(),
        [&](const AudioItem& item) { return item.getId() == itemId; });
    
    if (it == items.end()) {
        return false;  // Item not found
    }
    
    size_t deleteIndex = std::distance(items.begin(), it);
    
    // CASE 1: Delete non-current item
    if (deleteIndex < currentIndex) {
        // Item before current - decrement currentIndex
        currentIndex--;
    } 
    else if (deleteIndex == currentIndex) {
        // CASE 2: Delete currently playing item
        // Option A: Move to next item
        if (currentIndex < items.size() - 1) {
            // currentIndex stays same (points to next item)
        } else if (!items.empty()) {
            // Move to last item
            currentIndex = items.size() - 2;
        } else {
            // No more items
            currentIndex = -1;
        }
        
        emit currentItemDeleted();  // Signal player to handle
    }
    // CASE 3: Delete after current - currentIndex unchanged
    
    // Remove item
    items.erase(it);
    
    // Validate invariants
    assert(currentIndex == -1 || (currentIndex >= 0 && currentIndex < items.size()));
    
    emit playlistModified();
    return true;
}
```

### Navigation Methods

```cpp
// In PlaylistManager:

// Move to previous item
int getPreviousIndex() {
    if (currentIndex <= 0) {
        switch (playbackMode_) {
            case PlaybackMode::REPEAT_ALL:
                return items.empty() ? -1 : items.size() - 1;  // Wrap to end
            case PlaybackMode::REPEAT_ONE:
                return currentIndex;  // Stay on same item
            default:
                return -1;  // Stop at beginning
        }
    } else {
        return currentIndex - 1;  // Regular previous
    }
}

// Move to next item
int getNextIndex() {
    if (currentIndex >= items.size() - 1) {
        switch (playbackMode_) {
            case PlaybackMode::REPEAT_ALL:
                return 0;  // Wrap to beginning
            case PlaybackMode::REPEAT_ONE:
                return currentIndex;  // Stay on same item
            default:
                return -1;  // Stop at end
        }
    } else {
        return currentIndex + 1;  // Regular next
    }
}

// Handle shuffle mode
int getNextIndexInShuffle() {
    if (shuffleIndices_.empty() || currentShufflePos_ >= shuffleIndices_.size()) {
        return -1;
    }
    
    currentShufflePos_++;
    if (currentShufflePos_ >= shuffleIndices_.size()) {
        if (playbackMode_ == PlaybackMode::REPEAT_ALL) {
            currentShufflePos_ = 0;  // Restart shuffle
        } else {
            return -1;  // End of shuffle sequence
        }
    }
    
    return shuffleIndices_[currentShufflePos_];
}

// Button click handlers
void onLastButtonClicked() {
    int prevIndex = playbackMode_ == PlaybackMode::SHUFFLE 
        ? getPreviousIndexInShuffle()
        : getPreviousIndex();
    
    if (prevIndex >= 0) {
        setCurrentIndex(prevIndex);
        emit currentItemChanged(prevIndex);
    }
}

void onNextButtonClicked() {
    int nextIndex = playbackMode_ == PlaybackMode::SHUFFLE 
        ? getNextIndexInShuffle()
        : getNextIndex();
    
    if (nextIndex >= 0) {
        setCurrentIndex(nextIndex);
        emit currentItemChanged(nextIndex);
    }
}
```

### Playback Mode Changes

```cpp
// When user changes playback mode:

void setPlaybackMode(PlaybackMode newMode) {
    if (newMode == playbackMode_) {
        return;  // No change
    }
    
    PlaybackMode oldMode = playbackMode_;
    playbackMode_ = newMode;
    
    // Handle shuffle mode initialization
    if (newMode == PlaybackMode::SHUFFLE && oldMode != PlaybackMode::SHUFFLE) {
        // Generate shuffle sequence
        shuffleIndices_.clear();
        for (size_t i = 0; i < items.size(); i++) {
            shuffleIndices_.push_back(i);
        }
        std::random_shuffle(shuffleIndices_.begin(), shuffleIndices_.end());
        
        // Find current item in shuffle sequence
        auto pos = std::find(shuffleIndices_.begin(), shuffleIndices_.end(), currentIndex);
        currentShufflePos_ = std::distance(shuffleIndices_.begin(), pos);
    }
    else if (newMode != PlaybackMode::SHUFFLE && oldMode == PlaybackMode::SHUFFLE) {
        // Clear shuffle state
        shuffleIndices_.clear();
        currentShufflePos_ = 0;
    }
    
    emit playbackModeChanged(newMode);
}
```

### Rapid Navigation

```cpp
// Handle user rapidly clicking next/prev:
// (Qt's button throttling may prevent most, but be safe)

void onNextButtonClicked() {
    // Check if enough time has passed since last navigation
    if (lastNavigationTime_.elapsed() < 50) {  // ms
        return;  // Ignore rapid clicks
    }
    
    int nextIdx = getNextIndex();
    if (nextIdx >= 0) {
        setCurrentIndex(nextIdx);
        lastNavigationTime_.restart();
        emit currentItemChanged(nextIdx);
    }
}
```

## State Transition Table

| Current State | Action | Next State | Notes |
|---------------|--------|-----------|-------|
| currentIndex = 0 | Click "last" (NORMAL) | currentIndex = -1 | Stop at start |
| currentIndex = 0 | Click "last" (REPEAT_ALL) | currentIndex = size-1 | Wrap to end |
| currentIndex = mid | Click "last" (any) | currentIndex = mid-1 | Move prev |
| currentIndex = size-1 | Click "next" (NORMAL) | currentIndex = -1 | Stop at end |
| currentIndex = size-1 | Click "next" (REPEAT_ALL) | currentIndex = 0 | Wrap to start |
| currentIndex = i | Delete item i | currentIndex = i (now points to next) or i-1 | See deletion logic |
| currentIndex = i < delete | Delete after | currentIndex = i | Unchanged |
| Any | Change to SHUFFLE | Reset shuffle sequence | Generate new shuffle |
| SHUFFLE | Change from | currentIndex = original | Keep logical position |

## Defensive Programming

```cpp
// Always validate before using currentIndex:

const AudioItem* PlaylistManager::getCurrentItem() {
    if (currentIndex < 0 || currentIndex >= items.size()) {
        return nullptr;  // Never return invalid
    }
    return &items[currentIndex];
}

// Safe index change:
void PlaylistManager::setCurrentIndex(int newIndex) {
    if (newIndex < -1 || newIndex >= items.size()) {
        throw std::out_of_range("Invalid index");
    }
    
    int oldIndex = currentIndex;
    currentIndex = newIndex;
    
    if (oldIndex != newIndex) {
        emit currentIndexChanged(newIndex);
    }
}
```

## Requirements

**FR-005**: System MUST maintain consistent last/next button navigation after playlist items are deleted

**FR-006**: System MUST respect playback modes (normal, shuffle, repeat) when navigating after item deletion

## Validation & Testing

### Test Cases

- **TC-001**: Delete item at index 0, navigate with "last" → no crash, remains at -1 or wraps
- **TC-002**: Delete currently playing item (currentIndex), next navigation is valid
- **TC-003**: Delete item before currentIndex, currentIndex decrements, navigation valid
- **TC-004**: Delete item after currentIndex, currentIndex unchanged, navigation valid
- **TC-005**: Delete all items, currentIndex = -1, buttons disabled or inactive
- **TC-006**: Navigate after deletion in NORMAL mode - respects boundaries
- **TC-007**: Navigate after deletion in REPEAT_ALL mode - wraps correctly
- **TC-008**: Navigate after deletion in REPEAT_ONE mode - stays on item
- **TC-009**: Switch to SHUFFLE, shuffle sequence valid, navigation works
- **TC-010**: Switch away from SHUFFLE, index mapping preserved
- **TC-011**: Rapid consecutive deletions don't corrupt state
- **TC-012**: Rapid consecutive navigation doesn't corrupt state
- **TC-013**: Delete item, navigate, add new item - all indices valid
- **TC-014**: Deletion and mode change in same tick - state consistent

### Code Review Criteria

- currentIndex always validated before use
- Deletion always adjusts currentIndex if needed
- Navigation methods handle all edge cases (empty, at boundary, etc.)
- Playback mode changes handled correctly
- No off-by-one errors
- Shuffle sequence properly managed
- Signal emission after state changes
- No infinite loops or hangs

## Implementation Notes

- **Scope**: PlaylistManager, Playlist, player UI buttons
- **Files affected**: 
  - `src/playlist/playlist_manager.h/cpp`
  - `src/playlist/playlist.h/cpp`
  - `src/ui/pages/playlist_page.cpp`
- **Impact**: High - core functionality
- **Testing**: Comprehensive unit and integration tests required
- **Edge cases**: Empty playlist, single item, rapid operations, mode changes

## Success Criteria (from spec)

- **SC-004**: Playlist navigation functions (last/next buttons) work correctly after 100% of deletion scenarios tested
- **SC-005**: Last/next button navigation remains functional when switching between different playback modes

---

**Contract Status**: Ready for implementation.
