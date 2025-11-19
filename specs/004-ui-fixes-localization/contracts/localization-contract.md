# Localization Contract

**Date**: 2025-11-19  
**Feature**: `004-ui-fixes-localization`  
**Component**: UI Text Localization System

## Purpose

Establish contract for replacing all hardcoded UI strings with localized text via Qt `tr()` mechanism.

## API Contract

### Marking UI Strings (Developer Interface)

```cpp
// USAGE IN SOURCE CODE:
// All user-visible strings MUST use tr() macro

// In widgets/dialogs:
button->setText(tr("Play"));
label->setText(tr("Playlist", "Section header"));
statusBar()->showMessage(tr("Loading..."));

// In message boxes (no longer used for search - see async contract):
QMessageBox::information(this, tr("Success"), tr("Operation completed"));

// In menus:
action->setText(tr("&File"));

// Avoid (ANTI-PATTERN):
button->setText("Play");           // ❌ Hardcoded
button->setText(QString("Play"));  // ❌ Hardcoded
```

### Extraction and Compilation

```bash
# Step 1: Extract translatable strings from source
lupdate . -ts resource/lang/en_US.ts

# Step 2: Translate files (manual or via translator tool)
# Edit resource/lang/en_US.ts and resource/lang/zh_CN.ts

# Step 3: Compile to binary format (done by build system)
lrelease resource/lang/*.ts -qm resource/lang/
```

### Translation File Format

**.ts file structure** (XML):
```xml
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
  <context>
    <name>SearchPage</name>
    <message>
      <location filename="../src/ui/pages/search_page.cpp" line="45"/>
      <source>Search</source>
      <translation>搜索</translation>
    </message>
  </context>
</TS>
```

### Loading Translations at Runtime

```cpp
// In main() or application initialization:
QTranslator translator;
if (translator.load("resource/lang/zh_CN.qm")) {
    QApplication::instance()->installTranslator(&translator);
}
```

## Requirements

**FR-001**: All hardcoded UI strings MUST be replaced with `tr()` calls

**FR-008**: All user-facing text in dialogs, menus, buttons, labels, and status messages MUST use localization

## Validation & Testing

### Audit Checklist

```cpp
// ✅ CORRECT:
label->setText(tr("Item Title"));           // With tr()
QMessageBox::warning(this, tr("Error"), tr("An error occurred")); // All strings wrapped

// ❌ INCORRECT:
label->setText("Item Title");               // Missing tr()
label->setText("Item " + itemName);         // Variable concatenation (use %1)
label->setText(QString::number(count) + " items"); // Numeric concat
```

### Test Cases

- **TC-001**: Verify all visible UI text displays using `tr()` by examining source code
- **TC-002**: Verify `.ts` files contain all extracted strings
- **TC-003**: Verify `.qm` compiled files load without errors
- **TC-004**: Verify UI displays in default language after string replacement
- **TC-005**: Verify UI displays translated text when alternate language loaded

### Code Review Criteria

- No hardcoded English strings in UI code
- All strings wrapped with `tr()` or `QCoreApplication::translate()`
- String literals only (not variables)
- Context provided for disambiguation if needed
- `.ts` files updated with `lupdate`

## Implementation Notes

- **Scope**: All UI components (dialogs, pages, widgets, menus, tooltips, status messages)
- **Files affected**: `src/ui/**/*.cpp`, `src/ui/**/*.h`
- **Impact**: Medium - mechanical changes, no logic changes
- **Testing**: Unit tests verify string replacement, integration tests verify UI display

## Success Criteria (from spec)

- **SC-001**: 100% of visible UI text displays using localized strings from `tr()` calls instead of hardcoded values

---

**Contract Status**: Ready for implementation.
