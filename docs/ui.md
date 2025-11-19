# MainWindow Layout Specification

## 1. Overview

The MainWindow does not utilize native title or status bars. Its client area is partitioned into four distinct regions:

1. **Custom Titlebar Area**: Occupies the top 15% of the window height.
2. **Player Status Area**: Occupies the bottom 25% of the window height.
3. **Menu Area**: Occupies the leftmost 40% of the remaining central space.
4. **Content Area**: Occupies the remaining 60% of the central space.

Each region is fully occupied by a dedicated custom widget. To ensure the MainWindow's background settings are fully visible, all child widgets must maintain transparent backgrounds, with the exception of the MainWindow itself.

## 2. Titlebar

The Titlebar fully occupies the Custom Titlebar Area. It is divided into three sections:

- The leftmost **25%** of the width remains empty.
- The rightmost **15%** contains three window control buttons: **Minimize**, **Maximize**, and **Close**.
- The remaining central area contains a **Search Text Input** field and a **Search Scope Button** positioned inside the input field at its far right.

### 2.1 Search Text Input

- When empty, the input field displays a gray placeholder text: "Search...".
- Upon pressing the Enter key while focused, passing keyword and selectedPlatform to SearchPage(see below) and make it emits the `searchRequested(Keyword, SelectedPlatform)` signal.

### 2.2 Search Scope Button

- Clicking this button opens a multi-selection checkbox list, allowing the user to select which platforms to include in the search.

### 2.3 Minimize, Maximize, and Close Buttons

- Clicking these buttons minimizes, maximizes/restores, or closes the main window, respectively.

## 3. Player Status Bar

The Player Status Bar fully occupies the Player Status Area. It is divided into three sections with a strict **3:5:2** width ratio:

- The left **30%** is the **Audio Information Area**.
- The central **50%** is the **Player Control Area**.
- The right **20%** is the **Volume Control Area**.
**Signals:**
 - **Accepts**: `PlaybackReset()`. Upon receiving this signal, reset the data in AudioInformationRect and playback progress in Player Control Area.

### 3.1 Audio Information Area

This area displays basic audio metadata: cover art, track title, and artist/uploader.

- The cover art is displayed as a square, filling the leftmost part of the area.
- The track title and artist are displayed vertically to the right of the cover art.

**Signals:**
- **Accepts**: `AudioInformationUpdate(CoverImgPath, Title, Author)`. Upon receiving this signal, the area updates its displayed content accordingly.
- **Emits**: None.

### 3.2 Player Control Area

This area is divided vertically into two parts:

- **Upper Part**: Contains a horizontal layout of four buttons: **Previous**, **Play/Pause**, **Next**, and **Play Mode**.
- **Lower Part**: A progress bar displaying the current playback position.

**Signals:**
- **Emits**: `PlayerOperation(OperationType)` when any of its buttons are clicked.
- **Accepts**:
  - `PlaymodeUpdate(NewPlaymode)`: Updates the icon/text of the Play Mode button.
  - `PlaybackProgressUpdate(int progress, int duration)`: Updates the progress bar's range and value.

### 3.3 Volume Control Area

This area contains two horizontally arranged controls: a **Volume Slider** and a **Mute Button**.

**Signals:**
- **Emits**:
  - `VolumeChange(volume)`: Continuously emitted while the Volume Slider is being dragged.
  - `VolumeMute()`: Emitted when the Mute Button is clicked.
- **Accepts**: None.

## 4. Menu Area

The Menu Area contains four buttons stacked vertically, from top to bottom: **Home**, **Search**, **Playlist**, and **Settings**. Clicking any button except **Playlist** immediately displays the corresponding subpage in the Content Area.

### 4.1 Playlist Button

Clicking the **Playlist** button expands/collapses a tree view directly below it within the Menu Area. The tree structure is as follows:

- **Root Level**: The "Playlist" button itself.
- **Second Level**: "Category" nodes for organizing playlists.
- **Third Level**: Individual "Playlist" nodes.

Clicking on a specific playlist node triggers the display of the corresponding Playlist Page in the Content Area.

**State Persistence**:
- The collapsed/expanded state of the playlist tree should be preserved between application sessions.
- The category containing the currently active playlist (`currentPlaylist`) must be forcibly expanded.

## 5. Content Area

The Content Area is responsible for displaying the subpage associated with the currently selected item in the Menu Area.