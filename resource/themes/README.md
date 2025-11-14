# Theme Files

This directory contains custom theme JSON files for the BilibiliPlayer application.

## Theme Structure

Each theme is defined in a JSON file with the following properties:

```json
{
  "name": "Theme Name",
  "description": "Theme description",
  "backgroundImagePath": "path/to/image.jpg",
  "backgroundMode": "tiled|stretched|centered|none",
  "backgroundColor": "#ffffff",
  "buttonNormalColor": "#e0e0e0",
  "buttonHoverColor": "#d0d0d0",
  "buttonPressedColor": "#c0c0c0",
  "buttonDisabledColor": "#f0f0f0",
  "buttonBorderColor": "#a0a0a0",
  "fontPrimaryColor": "#000000",
  "fontSecondaryColor": "#606060",
  "fontDisabledColor": "#a0a0a0",
  "fontLinkColor": "#0066cc",
  "borderColor": "#cccccc",
  "accentColor": "#409eff",
  "selectionColor": "#409eff",
  "windowBackgroundColor": "#ffffff",
  "panelBackgroundColor": "#f5f5f5"
}
```

## Built-in Themes

The following themes are built-in and don't require JSON files:
- **Light**: Clean light theme with blue accents
- **Dark**: Dark theme for reduced eye strain
- **Custom**: Base for user customization

## Creating Custom Themes

1. Create a new JSON file in this directory
2. Copy the structure above
3. Modify colors using hex color codes (#RRGGBB)
4. Optionally set a background image path
5. Select background mode if using an image:
   - `tiled`: Repeat image as tiles
   - `stretched`: Stretch to fill window
   - `centered`: Show once in center
   - `none`: No background image

## Example Themes

- `ocean_blue.json`: Ocean-inspired blue theme

## Using Themes

Themes can be selected in Settings → General → Theme
