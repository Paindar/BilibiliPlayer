#pragma once

#include <QObject>
#include <QString>
#include <QSettings>
#include <QColor>
#include <QDir>

/**
 * Configuration Manager - handles all application settings
 * All paths are relative to workspace directory for security and portability
 * Must be initialized first as other managers depend on it
 */
class ConfigManager : public QObject
{
    Q_OBJECT
    
public:
    explicit ConfigManager(QObject* parent = nullptr);
    ~ConfigManager();
    
    // Initialization with workspace directory
    void initialize(const QString& workspaceDir);
    void loadFromFile();
    void saveToFile();
    
    // Workspace management
    QString getWorkspaceDirectory() const { return m_workspaceDir; }
    QString getAbsolutePath(const QString& relativePath) const;
    bool isPathInWorkspace(const QString& path) const;
    
    // Path validation and security
    bool validateRelativePath(const QString& relativePath) const;
    bool validateAndCreatePath(const QString& relativePath);
    QString sanitizeRelativePath(const QString& relativePath) const;
    QString buildSafeFilePath(const QString& subdirectory, const QString& filename) const;
    
    // Default workspace structure paths (relative to workspace)
    QString getTmpDirectory() const { return "tmp"; }
    QString getConfigDirectory() const { return "config"; }
    QString getLogDirectory() const { return "log"; }
    QString getConfigFilePath() const { return "config.json"; } // Fixed file at workspace root
    
    // User-configurable paths (relative to workspace)
    QString getPlaylistDirectory() const;
    QString getThemeDirectory() const;
    QString getAudioCacheDirectory() const;
    QString getCoverCacheDirectory() const;
    void setPlaylistDirectory(const QString& relativePath);
    void setThemeDirectory(const QString& relativePath);
    void setAudioCacheDirectory(const QString& relativePath);
    void setCoverCacheDirectory(const QString& relativePath);
    
    // Network settings (needed by NetworkManager)
    QString getApiBaseUrl() const;
    int getNetworkTimeout() const;
    QString getProxyUrl() const;
    void setApiBaseUrl(const QString& url);
    void setNetworkTimeout(int timeoutMs);
    void setProxyUrl(const QString& url);
    
    // UI settings (needed by MainWindow)
    QString getTheme() const;
    QString getLanguage() const;
    QColor getAccentColor() const;
    void setTheme(const QString& theme);
    void setLanguage(const QString& language);
    void setAccentColor(const QColor& color);
    
    // Playlist settings (needed by PlaylistManager)
    QString getCategoriesFilePath() const;
    bool getAutoSaveEnabled() const;
    int getAutoSaveInterval() const;
    QString setCategoriesFilePath(const QString& relativePath);
    void setAutoSaveEnabled(bool enabled);
    void setAutoSaveInterval(int intervalMinutes);
    
    // Error logging
    void logNetworkError(const QString& error);
    QStringList getRecentErrors() const;
    
signals:
    void themeChanged(const QString& theme);
    void languageChanged(const QString& language);
    void accentColorChanged(const QColor& color);
    void pathsChanged();
    void networkSettingsChanged();
    void playlistDirectoryChanged(const QString& relativePath);
    
private:
    void setupDefaults();
    void ensureDefaultDirectoriesExist();
    void ensureUserDirectoriesExist();
    
    QSettings* m_settings;
    QString m_workspaceDir;
    
    // Cached values for performance
    QString m_currentTheme;
    QString m_currentLanguage;
};