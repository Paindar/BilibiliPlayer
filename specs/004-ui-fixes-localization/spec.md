# Feature Specification: UI Fixes, Localization, and Playlist Navigation

**Feature Branch**: `004-ui-fixes-localization`  
**Created**: 2025-11-19  
**Status**: Draft  
**Input**: User description: "Fix UI text localization, remove blocking search dialogs, fix platform names, and fix playlist navigation"

## User Scenarios & Testing *(mandatory)*

<!--
  IMPORTANT: User stories should be PRIORITIZED as user journeys ordered by importance.
  Each user story/journey must be INDEPENDENTLY TESTABLE - meaning if you implement just ONE of them,
  you should still have a viable MVP (Minimum Viable Product) that delivers value.
  
  Assign priorities (P1, P2, P3, etc.) to each story, where P1 is the most critical.
  Think of each story as a standalone slice of functionality that can be:
  - Developed independently
  - Tested independently
  - Deployed independently
  - Demonstrated to users independently
-->

### User Story 1 - Replace Hardcoded UI Strings with Localized Text (Priority: P1)

As a developer maintaining the codebase, I want all user-visible UI text to use the localization system so that the application can be easily translated to different languages and current hardcoded text is eliminated.

**Why this priority**: This is foundational work that enables the app to support multiple languages. Removing hardcoded strings prevents inconsistencies and allows for professional translation workflows. This has the highest priority because it's a correctness issue - UI text should never be hardcoded.

**Independent Test**: Can be tested by replacing hardcoded strings in UI components with `tr()` calls and verifying the text still displays correctly in the default language. Delivers value by eliminating technical debt and enabling future localization.

**Acceptance Scenarios**:

1. **Given** the application is running, **When** any UI text is displayed, **Then** the text is sourced from translation files via `tr()` and not hardcoded in source code
2. **Given** a user opens any dialog, menu, button, or label, **When** they view the text, **Then** all visible text uses the localization system
3. **Given** translation files exist in `resource/lang/`, **When** the application loads, **Then** all `tr()` calls resolve to appropriate translations from those files

---

### User Story 2 - Enable Non-Blocking Search Functionality (Priority: P1)

As a user searching for audio content, I want the search operation to run without freezing the UI so I can continue interacting with the application while search results are being retrieved.

**Why this priority**: Blocking UI during search is a critical usability issue that makes the application appear to freeze. Removing blocking message boxes directly improves user experience and is essential for responsive application behavior.

**Independent Test**: Can be tested by initiating a search and verifying the UI remains responsive while the search is in progress. Delivers value by ensuring a smooth user experience during search operations.

**Acceptance Scenarios**:

1. **Given** the user is on the search page, **When** they initiate a search for content, **Then** the UI remains responsive and interactive
2. **Given** a search operation is in progress, **When** the user interacts with other UI elements, **Then** those interactions are processed without delay
3. **Given** search results are being retrieved, **When** the user moves the window or interacts with buttons, **Then** the application responds without freezing
4. **Given** a search operation is in progress, **When** results complete loading, **Then** results are displayed in an unobtrusive manner without modal dialogs blocking user interaction

---

### User Story 3 - Display Correct Platform Names for Search Results (Priority: P1)

As a user searching for audio content from the search page, I want items added from search results to display the correct platform name instead of "Unknown" so I can identify where content came from.

**Why this priority**: Displaying "Unknown" platform names is incorrect behavior that confuses users about content origin. This is a P1 bug that affects data accuracy and user understanding of their playlist.

**Independent Test**: Can be tested by searching for content, adding it to the playlist, and verifying the platform name displays correctly instead of "Unknown". Delivers value by providing accurate metadata for search-sourced content.

**Acceptance Scenarios**:

1. **Given** the user performs a search in the search page, **When** they add an audio item from search results to the playlist, **Then** the item displays with the correct platform name (e.g., "Bilibili")
2. **Given** audio from search results is in the playlist, **When** the user views the playlist details, **Then** the platform information is accurately displayed, not "Unknown"
3. **Given** multiple audio items from search are added, **When** the user reviews the playlist, **Then** each item shows its source platform consistently

---

### User Story 4 - Fix Playlist Navigation After Item Deletion (Priority: P1)

As a user playing audio and managing my playlist, I want the "last" button to navigate correctly after deleting an audio item so the playlist navigation remains functional in all states.

**Why this priority**: Navigation buttons not working correctly after deletion is a critical usability bug that breaks core functionality. This affects user ability to browse their playlist and is a blocker for reliable playback management.

**Independent Test**: Can be tested by adding multiple items to playlist, playing one, deleting items, and verifying last/next buttons work correctly. Delivers value by ensuring consistent and reliable playlist navigation.

**Acceptance Scenarios**:

1. **Given** the user has multiple audio items in the playlist, **When** they delete an item and click the "last" button, **Then** the application correctly navigates to the previous item in the playlist
2. **Given** the currently playing item is deleted, **When** the user clicks "last", **Then** the player navigates to the appropriate previous item without errors
3. **Given** the playlist has been modified (items deleted or reordered), **When** the user navigates using "last" or "next" buttons, **Then** navigation works consistently regardless of playlist state
4. **Given** different playback modes are active (repeat, shuffle, etc.), **When** items are deleted and the user clicks "last", **Then** navigation respects the current playback mode correctly

### Edge Cases

- What happens when a user deletes the currently playing item in the playlist?
- How does the application handle search operations that take longer than expected without blocking UI?
- What occurs when a user navigates between items rapidly after deletion?
- How does the system handle cases where translation strings are missing from language files?
- What happens when switching playback modes (normal, shuffle, repeat) after deleting items?

## Clarifications Collected from Implementation Review

### C1: Active Item Deletion Behavior
**Q**: When user deletes the currently playing item, what should happen?  
**A**: Play next track immediately if available; stop playback if playlist becomes empty.  
**Impact**: Update PlaylistManager deletion handler to trigger next-track playback or stop signal.

### C2: Search Progress UI Display
**Q**: How to indicate search is in progress without blocking with modal dialog?  
**A**: Add dedicated "SearchingPage" to SearchPage's contentStack (QStackedWidget), similar to emptyPage pattern.  
**Impact**: UI framework already supports QStackedWidget; implement by adding SearchingPage widget to stack.

### C3: Missing Translation Strings Fallback
**Q**: When `tr()` cannot find a translation string, what should be displayed?  
**A**: Show raw English string as fallback (Qt default behavior when translation key not found).  
**Impact**: Ensures UI never shows empty/broken text; helps identify missing translations in development.

### C4: Random Mode After Item Deletion
**Q**: When item deleted while in shuffle/random mode, what happens to play order?  
**A**: Regenerate randomPlayOrder to reflect updated playlist size.  
**Impact**: PlaylistManager must invalidate and rebuild random play order on deletion.

### C5: Platform Name Localization
**Q**: Should platform names like "Bilibili" be translatable?  
**A**: Yes, platform names are user-facing and should be wrapped with `tr()` for i18n support.  
**Impact**: Platform enum display names must use localization; e.g., `tr("Bilibili")` → translates to "B站" in Chinese.

### C6: Rapid Navigation Behavior
**Q**: How to handle rapid successive navigation clicks?  
**A**: Cancel pending async operations (search, metadata fetch) if user navigates away before completion; ensure smooth UI responsiveness.  
**Impact**: Add cancellation flags to AsyncSearchOperation; use `std::atomic<bool>` for thread-safe shutdown.

## Requirements *(mandatory)*

<!--
  ACTION REQUIRED: The content in this section represents placeholders.
  Fill them out with the right functional requirements.
-->

### Functional Requirements

- **FR-001**: System MUST replace all hardcoded UI text strings with localization function calls using the existing `tr()` mechanism
- **FR-002**: System MUST NOT display modal message boxes during search operations that block user interaction
- **FR-003**: System MUST execute search operations asynchronously without blocking the UI thread
- **FR-004**: System MUST display the correct platform name for audio items added from search results instead of "Unknown"
- **FR-005**: System MUST maintain consistent last/next button navigation after playlist items are deleted
- **FR-006**: System MUST respect playback modes (normal, shuffle, repeat) when navigating after item deletion
- **FR-007**: System MUST load appropriate translations from language files in `resource/lang/` when UI is initialized
- **FR-008**: All user-facing text in dialogs, menus, buttons, labels, and status messages MUST use the localization system

### Key Entities

- **AudioItem**: Represents a playlist audio item with properties including platform name, title, and metadata. The platform name attribute must be populated correctly when items come from search results.
- **Playlist**: Collection of AudioItems with state information about current position, deletion state, and playback mode. Navigation methods must handle deletion state correctly.
- **SearchResult**: Data returned from search operations containing platform information that must be correctly transferred to AudioItem when added to playlist.
- **UIComponent**: Any UI element (button, label, dialog, menu item) that displays text to users. Must use localization strings instead of hardcoded values.

## Success Criteria *(mandatory)*

<!--
  ACTION REQUIRED: Define measurable success criteria.
  These must be technology-agnostic and measurable.
-->

### Measurable Outcomes

- **SC-001**: 100% of visible UI text displays using localized strings from `tr()` calls instead of hardcoded values
- **SC-002**: Search operations complete without any UI freezing or blocking dialogs appearing during the operation
- **SC-003**: Audio items added from search results display the correct platform name (matching source) in all views
- **SC-004**: Playlist navigation functions (last/next buttons) work correctly after 100% of deletion scenarios tested
- **SC-005**: Last/next button navigation remains functional when switching between different playback modes
- **SC-006**: All translation strings referenced in UI code are present in language files without missing key errors

## Assumptions

- The localization infrastructure using `tr()` is already in place and functional
- Translation files exist in `resource/lang/` with the required language definitions
- The existing search operations can be refactored to run asynchronously without major architectural changes
- Platform name information is available in search results and can be reliably extracted and stored
- The playlist navigation logic can be debugged and fixed without requiring database migrations or major data model changes

## Dependencies

- **Existing Localization System**: Feature depends on the existing `tr()` localization mechanism being functional
- **UI Framework**: Qt framework must support asynchronous operations and thread-safe signal emission for non-blocking search
- **Language Files**: Translation files in `resource/lang/` must be accessible and properly formatted
- **Search Module**: Search functionality must be refactorable to support asynchronous operation
- **Playlist Module**: Playlist navigation and state management must be debuggable and fixable within the scope of this feature
