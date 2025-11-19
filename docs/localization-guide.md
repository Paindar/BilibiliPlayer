# Qt Localization Guide for BilibiliPlayer

## Overview
This guide documents the localization (i18n) infrastructure for BilibiliPlayer using Qt's native translation framework.

The workflow is simple:
1. Mark user-visible strings with `tr()`
2. Rebuild project (CMake automatically extracts strings via lupdate)
3. Edit `.ts` files with [Qt Linguist](https://doc.qt.io/qt-6/qtlinguist-index.html)
4. Rebuild project (CMake compiles `.qm` files via lrelease)
5. Select language at runtime via Settings page

## Key Components

- **Source strings** - Marked with `tr()` in C++ code
- **`.ts` files** - XML-based translation templates (maintained in `resource/lang/`)
  - `en_US.ts` - English source strings (auto-extracted)
  - `zh_CN.ts` - Chinese Simplified translations
- **`.qm` files** - Compiled binary translations (generated during build, runtime-only)
- **`QTranslator`** - Qt's runtime translation loader
- **CMake integration** - `qt_create_translation()` automates lupdate/lrelease

## String Marking with tr()

### Basic Usage
Mark all user-visible strings with `tr()`:

```cpp
#include <QWidget>

class MyDialog : public QWidget {
public:
    MyDialog() {
        // Good: String marked for translation
        setWindowTitle(tr("Search Results"));
        
        // Bad: Hardcoded string, won't be translated
        label->setText("Error occurred");
    }
};
```

### Pluralization
Use `tr()` with context for plural forms:

```cpp
// Simple case
QString msg = tr("1 item found");

// With count (Qt handles pluralization)
int count = results.size();
QString msg = tr("%n item(s) found", "", count);
```

### Context for Ambiguous Words
Use `QObject::tr()` with context when the same word has different meanings:

```cpp
// In class definition
class SearchPage : public QWidget {
    void setup() {
        // These will be in separate translation contexts
        tr("Delete");  // Context: SearchPage
    }
};

class PlaylistPage : public QWidget {
    void setup() {
        tr("Delete");  // Context: PlaylistPage
    }
};
```

### Dynamic Strings with Variables
Use `%1`, `%2` placeholders:

```cpp
// Good: Variables in placeholders
QString msg = tr("Loading from %1").arg(hostname);

// Bad: String concatenation
QString msg = "Loading from " + hostname;
```

## Translation Workflow

### Step 1: Mark Strings
Wrap user-visible strings with `tr()` in source code:
```cpp
button->setText(tr("Search"));
QMessageBox::information(this, tr("Success"), tr("Operation complete"));
```

### Step 2: Extract (Rebuild Project)
When you build the project, CMake automatically:
- Runs `lupdate` to scan source for `tr()` calls
- Updates `resource/lang/en_US.ts` with new strings

```powershell
cmake --build build/debug   # Runs lupdate automatically
```

### Step 3: Translate (Qt Linguist)
Edit `.ts` files with Qt Linguist GUI or manually:

```powershell
# Open GUI editor
linguist resource/lang/zh_CN.ts
```

Or edit XML directly:
```xml
<message>
    <source>Search</source>
    <translation>搜索</translation>
</message>
```

### Step 4: Compile (Rebuild Project)
Build again - CMake runs `lrelease` to compile `.ts` → `.qm`:

```powershell
cmake --build build/debug   # Runs lrelease automatically
```

Generated `.qm` files are deployed with the application.

### Step 5: Load at Runtime
The Settings page handles language selection:
```cpp
// SettingsPage::applyLanguage()
QTranslator translator;
translator.load("BilibiliPlayer_zh_CN", "resource/lang/");
QApplication::installTranslator(&translator);
```

## File Format

### .ts (Translation Source) Format
XML-based, human-readable:

```xml
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
    <context>
        <name>SearchPage</name>
        <message>
            <location filename="../../src/ui/page/search_page.cpp" line="42"/>
            <source>Search Results</source>
            <translation>搜索结果</translation>
        </message>
    </context>
</TS>
```

**Fields:**
- `context` → Class/Component name (for disambiguation)
- `message` → One translatable string
- `location` → File and line number where string appears
- `source` → Original English string
- `translation` → Translated string (empty = untranslated)

### .qm (Translation Binary) Format
Compiled by `lrelease`, optimized for runtime loading. Not human-editable.

## Key References

- **Translation Contexts** - Automatically created from class names
  - `SearchPage` - Search UI and controls
  - `PlaylistPage` - Playlist management
  - `AudioPlayer` - Playback controls
  - `MainWindow` - Main application window

## Checking Translation Status

View unfinished translations:

```powershell
# Count finished vs unfinished strings
(Select-String "<translation>.*</translation>" resource/lang/zh_CN.ts).Count
(Select-String "<translation></translation>" resource/lang/zh_CN.ts).Count
```

## Best Practices

### ✅ Do

- **Mark all user strings with `tr()`:**
  ```cpp
  button->setText(tr("Click Me"));
  ```

- **Use placeholders for dynamic content:**
  ```cpp
  msg = tr("Found %1 files").arg(count);
  ```

- **Use Qt Linguist GUI for editing translations**

### ❌ Don't

- **Don't hardcode user strings:**
  ```cpp
  // Bad
  label->setText("Error occurred");
  ```

- **Don't concatenate translatable strings:**
  ```cpp
  // Bad
  msg = tr("Hello") + " " + tr("World");
  ```

- **Don't manually edit `.qm` files** - generated only

- **Don't manually edit `en_US.ts`** - regenerated by lupdate

## Adding a New Language

1. Copy template:
   ```powershell
   Copy-Item resource/lang/en_US.ts resource/lang/ja_JP.ts
   ```

2. Update header:
   ```xml
   <TS version="2.1" language="ja_JP">
   ```

3. Edit translations in Qt Linguist:
   ```powershell
   linguist resource/lang/ja_JP.ts
   ```

4. Rebuild to compile `.qm` file:
   ```powershell
   cmake --build build/debug
   ```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Translations not loading | Verify `.qm` file exists; check language code (case-sensitive) |
| Missing strings in `.ts` | Ensure strings are wrapped with `tr()`; rebuild to extract |
| Broken characters | Save `.ts` files as UTF-8; verify lrelease output |
| Qt Linguist not found | Install from Qt SDK or: `$QtPath\6.8.0\bin\linguist.exe` |

## Resources

- [Qt Linguist Documentation](https://doc.qt.io/qt-6/qtlinguist-index.html)
- [tr() Function Reference](https://doc.qt.io/qt-6/qobject.html#tr)
- [QTranslator Class](https://doc.qt.io/qt-6/qtranslator.html)
- [Translation Workflow](https://doc.qt.io/qt-6/qtlinguist-manager.html)
