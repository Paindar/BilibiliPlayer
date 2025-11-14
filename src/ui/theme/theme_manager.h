#pragma once

#include <QObject>
#include <QString>
#include <QColor>
#include <QMap>
#include <QPixmap>

/**
 * @brief Theme structure containing all customizable visual properties
 */
struct Theme {
    QString name;
    QString description;
    
    // Background settings
    QString backgroundImagePath;     // Path to background image
    QString backgroundMode;          // "tiled", "stretched", "centered", "none"
    QColor backgroundColor;          // Fallback background color
    
    // Button colors
    QColor buttonNormalColor;
    QColor buttonHoverColor;
    QColor buttonPressedColor;
    QColor buttonDisabledColor;
    QColor buttonBorderColor;
    
    // Font colors
    QColor fontPrimaryColor;
    QColor fontSecondaryColor;
    QColor fontDisabledColor;
    QColor fontLinkColor;
    
    // Border and accent
    QColor borderColor;
    QColor accentColor;
    QColor selectionColor;
    
    // Window colors
    QColor windowBackgroundColor;
    QColor panelBackgroundColor;
    
    // Validation
    bool isValid() const;
    
    // Serialization
    QMap<QString, QVariant> toMap() const;
    static Theme fromMap(const QMap<QString, QVariant>& map);
};

/**
 * @brief ThemeManager - manages application themes and styling
 */
class ThemeManager : public QObject
{
    Q_OBJECT
    
public:
    explicit ThemeManager(QObject* parent = nullptr);
    ~ThemeManager();
    
    // Theme loading and management
    bool loadTheme(const QString& themeName);
    bool loadThemeFromFile(const QString& filePath);
    bool saveThemeToFile(const QString& filePath, const Theme& theme);
    
    // Current theme
    Theme getCurrentTheme() const { return m_currentTheme; }
    QString getCurrentThemeName() const { return m_currentTheme.name; }
    
    // Apply theme
    void applyTheme(const Theme& theme);
    void applyCurrentTheme();
    
    // Built-in themes
    static Theme getLightTheme();
    static Theme getDarkTheme();
    static Theme getCustomTheme();
    
    // Theme list
    QStringList getAvailableThemes() const;
    QStringList getBuiltInThemeNames() const;
    
    // Theme directory
    void setThemeDirectory(const QString& directory);
    QString getThemeDirectory() const { return m_themeDirectory; }
    
    // Preview
    QString generateStyleSheet(const Theme& theme) const;
    
signals:
    void themeChanged(const QString& themeName);
    void themeApplied(const Theme& theme);  // Signal emitted when theme is ready to apply
    
private:
    Theme m_currentTheme;
    QString m_themeDirectory;
    QMap<QString, Theme> m_builtInThemes;
    
    void initializeBuiltInThemes();
    QString colorToStyleSheet(const QColor& color) const;
    QString backgroundImageToStyleSheet(const QString& imagePath, const QString& mode) const;
};
