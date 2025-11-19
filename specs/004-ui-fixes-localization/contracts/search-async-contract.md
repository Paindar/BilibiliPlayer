# Search Async Contract

**Date**: 2025-11-19  
**Feature**: `004-ui-fixes-localization`  
**Component**: Non-Blocking Search Functionality

## Purpose

Establish contract for asynchronous search operations that do not block the UI thread and replace blocking message boxes with non-blocking feedback.

## Architecture

```
┌─ Main UI Thread
│  ├─ SearchPage (UI, user interactions)
│  ├─ Signal/Slot connections (search start, complete, progress)
│  └─ Status bar / inline feedback (non-blocking)
│
└─ Worker Thread
   ├─ BilibiliPlatform::search() (HTTP requests)
   ├─ Emit progress signals
   └─ Emit completion signal
```

## API Contract

### Search Initiation

```cpp
// In SearchPage or search service:

// Create worker thread for search
QThread* searchThread = new QThread;
BilibiliPlatform* platform = new BilibiliPlatform();
platform->moveToThread(searchThread);

// Connect signals
connect(platform, &BilibiliPlatform::searchProgress, 
        this, &SearchPage::onSearchProgress);
connect(platform, &BilibiliPlatform::searchCompleted, 
        this, &SearchPage::onSearchCompleted);
connect(platform, &BilibiliPlatform::searchError, 
        this, &SearchPage::onSearchError);

// Start thread and search
searchThread->start();
QMetaObject::invokeMethod(platform, [platform, query]() {
    platform->search(query);  // Runs on worker thread
}, Qt::QueuedConnection);
```

### Signal/Slot Definitions

```cpp
// In BilibiliPlatform:
signals:
    void searchProgress(const std::vector<SearchResult>& partialResults);
    void searchCompleted(const std::vector<SearchResult>& allResults);
    void searchError(const QString& errorMessage);

// In SearchPage:
private slots:
    void onSearchProgress(const std::vector<SearchResult>& results);
    void onSearchCompleted(const std::vector<SearchResult>& results);
    void onSearchError(const QString& errorMessage);
```

### UI Feedback (Non-Blocking)

```cpp
// INSTEAD OF MESSAGE BOX:
// ❌ ANTI-PATTERN (blocks UI):
QMessageBox::information(this, "Search", "Searching...");
results = platform->search(query);  // Blocks thread
QMessageBox::information(this, "Results", "Found X items");

// ✅ CORRECT (non-blocking):
void SearchPage::onSearchStarted() {
    statusBar()->showMessage("Searching...");
    searchButton->setEnabled(false);
}

void SearchPage::onSearchProgress(const std::vector<SearchResult>& partial) {
    statusBar()->showMessage(QString("Found %1 items...").arg(partial.size()));
    displayResults(partial);  // Update UI incrementally
}

void SearchPage::onSearchCompleted(const std::vector<SearchResult>& all) {
    statusBar()->showMessage(QString("Search complete - %1 results").arg(all.size()));
    searchButton->setEnabled(true);
    displayResults(all);
}

void SearchPage::onSearchError(const QString& error) {
    statusBar()->showMessage("Search failed: " + error);
    searchButton->setEnabled(true);
}
```

### Cancellation Support

```cpp
// User can cancel search at any time (UI remains responsive)
void SearchPage::onCancelSearchClicked() {
    if (searchThread && searchThread->isRunning()) {
        // Signal platform to cancel (if supported)
        QMetaObject::invokeMethod(platform, [this]() {
            if (platform) platform->cancel();  // Platform checks exitFlag_
        }, Qt::QueuedConnection);
        
        searchThread->quit();
        searchThread->wait();
    }
}

// In BilibiliPlatform::search():
std::vector<SearchResult> BilibiliPlatform::search(const QString& query) {
    std::vector<SearchResult> results;
    
    // ... perform search, checking exitFlag_ periodically ...
    
    for (/* each page of results */) {
        if (exitFlag_) break;  // Cancellation signal
        
        // ... fetch and process results ...
        
        // Emit progress (thread-safe via Qt signals)
        emit searchProgress(results);
    }
    
    emit searchCompleted(results);
    return results;
}
```

## Thread Safety Guarantees

**Qt Signal/Slot Mechanism Guarantees**:
- Signals emitted from worker thread are automatically queued on UI thread
- No manual locking required for signal parameters
- Signal parameters copied safely across thread boundary
- Qt::AutoConnection (default) detects cross-thread signals and uses queue

**Implementation Notes**:
- Avoid passing raw pointers in signals - use values or shared_ptr
- SearchResult should be copyable for signal parameters
- Do not access UI objects from worker thread directly - use signals

## Requirements

**FR-002**: System MUST NOT display modal message boxes during search operations that block user interaction

**FR-003**: System MUST execute search operations asynchronously without blocking the UI thread

## Validation & Testing

### Test Cases

- **TC-001**: Search operation completes without UI freezing
- **TC-002**: UI remains responsive while search in progress
- **TC-003**: User can interact with other UI elements during search
- **TC-004**: Search can be cancelled mid-operation
- **TC-005**: Errors display in status bar, not modal dialog
- **TC-006**: Multiple searches don't overlap (previous cancelled before new starts)
- **TC-007**: Worker thread properly cleaned up after search completes

### Performance Benchmarks

- **Search progress signal**: Emitted every 5-10 results or 100ms (whichever first)
- **UI responsiveness**: Button clicks respond within 50ms during search
- **Memory**: No unbounded growth in results vector (clear old results)

## Code Review Criteria

- No `QMessageBox` calls during search operations
- All search work moved to separate thread
- Signals used for UI updates (not direct calls)
- Thread lifecycle properly managed (start/quit/wait)
- Error handling via signals, not exceptions to UI
- Cancellation support for user control

## Implementation Notes

- **Scope**: SearchPage, SearchService, BilibiliPlatform search methods
- **Files affected**: `src/ui/pages/search_page.cpp`, `src/network/platform/bili_network_interface.cpp`
- **Impact**: High - changes search architecture, improves UX
- **Breaking changes**: API changes signal emission pattern
- **Testing**: Integration tests for async operation and UI responsiveness

## Success Criteria (from spec)

- **SC-002**: Search operations complete without any UI freezing or blocking dialogs appearing during the operation

---

**Contract Status**: Ready for implementation.
