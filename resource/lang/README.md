# Translation Files

This directory contains Qt translation files (.ts and .qm) for internationalization.

## Supported Languages

- **English (en_US)**: Source language, no translation file needed
- **Simplified Chinese (zh_CN)**: BilibiliPlayer_zh_CN.ts

## Generating Translation Files

To generate/update .ts files:
```bash
lupdate ../src -ts BilibiliPlayer_zh_CN.ts BilibiliPlayer_zh_CN.ts
```

To compile .ts files to .qm files:
```bash
lrelease BilibiliPlayer_zh_CN.ts
```

## Adding Translations

1. Mark all translatable strings in code with `tr("string")`
2. Run `lupdate` to extract strings to .ts files
3. Edit .ts files with Qt Linguist
4. Run `lrelease` to compile to .qm files
5. Deploy .qm files with the application

## File Structure

- `*.ts` - Translation source files (XML, human-editable with Qt Linguist)
- `*.qm` - Compiled translation files (binary, used at runtime)
