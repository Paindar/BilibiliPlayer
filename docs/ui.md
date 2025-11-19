# MainWindow Layout Specification

## 1. Overview

The MainWindow does not utilize native title or status bars. Its client area is partitioned into four distinct regions, separated by split lines:

1. **Custom Titlebar Area**: Occupies the top 15% of the window height.
2. **Player Status Area**: Occupies the bottom 25% of the window height.
3. **Menu Area**: Occupies the leftmost 40% of the remaining central space.
4. **Content Area**: Occupies the remaining 60% of the central space.

Each region is fully occupied by a dedicated custom widget. To ensure the MainWindow's background settings are fully visible, all child widgets must maintain transparent backgrounds, with the exception of the MainWindow itself.

The Content Area acts as a container (`QWidget* contentWidget = nullptr;`) that switches between different subpage widgets (e.g., `SearchPage`, `PlaylistPage`).

## 2. Titlebar

The Titlebar fully occupies the Custom Titlebar Area. It is divided into three sections:

*   The leftmost **25%** contains a empty placeholder QWidget.
*   The rightmost **15%** contains three window control buttons: **Minimize**, **Maximize**, and **Close**.
*   The remaining central area contains a **Search Text Input** field and a **Search Scope Button** positioned inside the input field at its far right.

**Signals:**
*   `windowMinimized()`
*   `windowMaximized()` 
*   `windowClosed()`

**Slots:**
*   `void navigateToSearchPage(const QString& keyword, const QStringList& platforms)`: Passes search parameters to the SearchPage and switches to it.

### 2.1 Search Text Input

*   When empty, the input field displays a gray placeholder text: "Search...".
*   Upon pressing the Enter key while focused, it triggers the Titlebar to call `navigateToSearchPage(Keyword, SelectedPlatform)`.

### 2.2 Search Scope Button

*   Clicking this button opens a multi-selection checkbox list, allowing the user to select which platforms to include in the search.

### 2.3 Minimize, Maximize, and Close Buttons

*   Clicking these buttons triggers the Titlebar to emit the `windowMinimized()`, `windowMaximized()`, or `windowClosed()` signals, respectively.

## 3. Player Status Bar

The Player Status Bar fully occupies the Player Status Area. It is divided into three sections with a strict **3:5:2** width ratio:

*   The left **30%** is the **Audio Information Area**.
*   The central **50%** is the **Player Control Area**.
*   The right **20%** is the **Volume Control Area**.

**Signals:**
*   `playerOperation(int OperationType)`: Emitted when playback control buttons are clicked.
*   `volumeChange(int volume)`: Emitted continuously while the volume slider is dragged.
*   `volumeMute()`: Emitted when the mute button is clicked.

**Slots:**
*   `void playbackReset()`: Resets audio information and playback progress.
*   `void currentSongRemoved(bool next)`: Handles song removal - emits PLAY_Next if next is true, otherwise PLAY_STOP.
*   `void audioInformationUpdate(const QString& coverPath, const QString& title, const QString& author)`: Updates the audio information display.
*   `void playmodeUpdate(int newPlaymode)`: Updates the play mode button appearance.
*   `void playbackProgressUpdate(int progress, int duration)`: Updates the playback progress bar.

### 3.1 Audio Information Area

This area displays basic audio metadata: cover art, track title, and artist/uploader.

*   The cover art is displayed as a square, filling the leftmost part of the area.
*   The track title and artist are displayed vertically to the right of the cover art.
*   The track title uses a scrolling label (implemented in `ui/util/elided_label.h`) to handle overflow.

### 3.2 Player Control Area

This area is divided vertically into two parts:

*   **Upper Part**: Contains a horizontal layout of four buttons: **Previous**, **Play/Pause**, **Next**, and **Play Mode**.
*   **Lower Part**: A progress bar displaying the current playback position.

### 3.3 Volume Control Area

This area contains two horizontally arranged controls: a **Volume Slider** and a **Mute Button**.

## 4. Menu Area

The Menu Area contains four buttons stacked vertically, from top to bottom: **Home**, **Search**, **Playlist**, and **Settings**. Clicking any button except **Playlist** immediately displays the corresponding subpage in the Content Area.

**Signals:**
*   `addNewCategory(const QString& category)`: Emitted when a new category is created.
*   `addNewPlaylist(const QString& category, const QString& playlist)`: Emitted when a new playlist is created.

### 4.1 Playlist Button

Clicking the **Playlist** button expands/collapses a tree view directly below it within the Menu Area. The tree structure is as follows:

*   **Root Level**: The "Playlist" button itself.
*   **Second Level**: "Category" nodes for organizing playlists.
*   **Third Level**: Individual "Playlist" nodes.

Clicking on a specific playlist node triggers the display of the corresponding `PlaylistPage` in the Content Area.

**State Persistence:**
*   The collapsed/expanded state of the playlist tree should be preserved between application sessions.
*   The category containing the currently active playlist (`currentPlaylist`) must be forcibly expanded.

**UI Elements:**
*   A persistent button on the top-level menu (far right) allows adding a new category.
*   Each category displays an "Add Playlist" button (far right) only on hover.
*   New categories and playlists use default titles.

## 5. Content Area

The Content Area is a container widget responsible for housing and switching between subpage widgets (`HomePage`, `SearchPage`, `PlaylistPage`, `SettingsPage`). It does not directly handle application-specific signals.

## 6. Home Page

*Reserved for future implementation.*

## 7. Search Page

The `SearchPage` can be initialized with two parameters: `keyword` and `platform`. If provided, it automatically starts searching after display.

**Layout:**
*   **Upper Section**: Occupies full width and 30% height, containing a text input and a search button arranged horizontally.
*   **Lower Section**: Occupies the remaining space, filled with a `QStackedWidget` with three states:
    1.  **Search Not Started**
    2.  **Searching**
    3.  **Search Results**

**Signals:**
*   `searchRequested(const QString& keyword, const QStringList& platforms)`: Emitted to initiate a search with the specified parameters.
*   `searchResultDoubleClicked(const SearchResult& result)`: Emitted when a search result is double-clicked.

**Slots:**
*   `void searchProgressStart()`: Switches the display to "Searching" state.
*   `void searchProgressUpdate(int progress)`: Updates the search progress indicator.
*   `void searchResult(const QList<SearchResult>& results)`: Switches to "Search Results" state and displays the results.

**Interactions:**
*   Clicking the search button or pressing Enter in the input box (with valid keyword and platform) triggers the `SearchPage` to emit `searchRequested(Keyword, SelectedPlatform)`.

### 7.1 Search Not Started State

Displays a centered label: "Enter keywords to search".

### 7.2 Searching State

Displays a centered horizontal layout with a label "Searching..." and a progress bar.

### 7.3 Search Results State

Displays results in a `ListView`. Each item shows:
*   Cover art on the left.
*   Title, author, and description arranged vertically on the right.
*   Title scrolls horizontally on hover if overflow occurs.

## 8. Playlist Page

The `PlaylistPage` requires a `playlist UUID` parameter to load playlist information and content.

**Layout:**
*   **Upper Section**: Playlist information.
    *   Left **40%**: Cover art centered at 80% width and height.
    *   Right **60%**: Playlist title, creator, description, creation time, and buttons: **Play Playlist**, **Add Local File**, **Delete Playlist**.
*   **Lower Section**: Song list with headers: Title, Artist, Platform, Duration.

**Signals:**
*   `addLocalMedia(const QString& path)`: Emitted to add a local media file to the playlist.
*   `playSongFromPlaylist(const QString& category, const QString& playlist, const QString& audio)`: Emitted to play a specific song from the playlist.
*   `deleteSongFromPlaylist(const QString& category, const QString& playlist, const QString& audio)`: Emitted to remove a song from the playlist.
*   `updateSongInfo(const QString& audio)`: Emitted when song metadata is modified.
*   `updatePlaylistInfo(const QString& playlist)`: Emitted when playlist information is updated.
*   `deletePlaylist(const QString& category, const QString& playlist)`: Emitted to delete the entire playlist.

**Slots:**
*   `void startPlaySong(const QString& category, const QString& playlist, const QString& song)`: Highlights the currently playing song.
*   `void stopPlaySong(const QString& category, const QString& playlist, const QString& song)`: Removes highlight from the song.
*   `void songInfoUpdated(const QString& playlist, const QString& song)`: Updates song information in the list.

**Interactions:**
*   Hovering over a song title triggers horizontal scrolling if overflow occurs.
*   Right-clicking a song opens a context menu with options to **Edit** or **Delete**.

### 8.1 Playlist Title and Description

*   Double-clicking the title or description enters edit mode (description can be empty; title cannot).
*   Exiting edit mode with valid text triggers the `PlaylistPage` to emit `updatePlaylistInfo(playlist)`.

### 8.2 Play Playlist Button

*   Clicking this button (if the playlist is not empty) triggers the `PlaylistPage` to emit `playSongFromPlaylist(category, playlist, audio)`, where `audio` is the first song.

### 8.3 Delete Playlist Button

*   Clicking prompts a confirmation dialog. If confirmed, navigates to the next playlist (or Home if none) and triggers the `PlaylistPage` to emit `deletePlaylist(category, playlist)`.

### 8.4 Add Local File Button

*   Clicking opens a system file dialog. Selected files trigger the `PlaylistPage` to emit `addLocalMedia(path)`. File filters are provided by the backend; any file is allowed before backend implementation.

### 8.5 Delete Song Action

*   If the song to be deleted is currently playing, checks if the playlist will be empty. If empty, the `PlaylistPage` emits `currentSongRemoved(true)`; otherwise, emits `currentSongRemoved(false)`. (Note: This signal is sent to the Player Status Bar).

### 8.6 Edit Song Info Action

*   Clicking "Edit" opens a dialog to modify the song's title and artist. Saving triggers the `PlaylistPage` to emit `updateSongInfo(audio)`.

## 9. Settings Page

The `SettingsPage` displays all configurable parameters (equivalent to those settable via `Set()` in the configuration manager) in a list, with **Save** (blue) and **Cancel** (red) buttons at the bottom.

**Signals:**
*   `requireConfigMap()`: Emitted on initialization or cancel to request current configuration.
*   `settingsUpdate(const QVariantMap& configMap)`: Emitted when settings are saved.

**Slots:**
*   `void provideConfigMap(const QVariantMap& configMap)`: Receives and populates the current configuration values.

**Initialization:**
*   On load, the `SettingsPage` emits `requireConfigMap()`.
*   Upon receiving `provideConfigMap(ConfigMap)`, the page populates settings from the map.

**Interactions:**
*   Clicking **Save** triggers the `SettingsPage` to emit `settingsUpdate(ConfigMap)`.
*   Clicking **Cancel** triggers the `SettingsPage` to emit `requireConfigMap()` to reload initial values.