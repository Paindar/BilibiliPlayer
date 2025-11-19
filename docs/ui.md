# MainWindow 
MainWindow has no vanilla title bar and status bar. The rect of MainWindow is devided into 4 parts. Part 1 is "Custom Topbar Rect", at the top of 15% place; Part 2 is "Player Status Rect", at the bottom of 25% place. The rest places left 2 part, Part 3 and Part4. Part 3 is Menu Rect, occupy the 40% place, Part 4 is Content Rect, occupy the right of 60% place.

## CustomTitleBar:
CustomTitleBar fully places in Custom Topbar Rect. it has 3 part: the far left of 25% place keep empty; the far right of 15% place settles 3 buttons: Minimum, Maximum, Close; left place settles a TextInput as searchText and a small button "Search Range Button" inside the far right of SearchText. 
### SearchText:
When SearchText is empty, it will show gray sentence: "Searching...".
when click Enter in SearchText, emit searchRequested(Keyword, SelectedPlatform). 
### Search Range Button:
Click it to open a multi-checkbox so that user can select the range of searching platform.

### Minimun Button:
Minimum the window.

### Maximum Button:
Maximum the window.

### Close Button:
Close the window.

## PlayerStatusRect
PlayerStatusRect fully places in