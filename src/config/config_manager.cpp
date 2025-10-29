#include "config_manager.h"
#include "../log/log_manager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QStandardPaths>
#include <QFileInfo>
#include <QRegularExpression>
#include <fmt/format.h>

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
    , m_settings(nullptr)
{
    LOG_DEBUG("ConfigManager created");
}

ConfigManager::~ConfigManager()
{
    if (m_settings) {
        delete m_settings;
    }
    LOG_DEBUG("ConfigManager destroyed");
}

void ConfigManager::initialize(const QString& workspaceDir)
{
    LOG_INFO("Initializing ConfigManager with workspace: {}", workspaceDir.toStdString());
    
    // Set workspace directory (must be absolute path)
    QDir wsDir(workspaceDir);
    m_workspaceDir = wsDir.absolutePath();
    
    if (!wsDir.exists()) {
        if (!wsDir.mkpath(m_workspaceDir)) {
            std::string error = fmt::format("Failed to create workspace directory: {}", m_workspaceDir.toStdString());
            LOG_ERROR(error);
            throw std::runtime_error(error);
        }
        LOG_INFO("Created workspace directory: {}", m_workspaceDir.toStdString());
    }
    
    // Ensure default directories exist
    ensureDefaultDirectoriesExist();
    
    // Setup QSettings to use workspace-based config file
    QString configPath = getAbsolutePath(getConfigFilePath());
    m_settings = new QSettings(configPath, QSettings::IniFormat, this);
    
    LOG_INFO("ConfigManager initialized with config file: {}", configPath.toStdString());
    
    // Setup default values
    setupDefaults();
    
    // Load existing configuration
    loadFromFile();
}

void ConfigManager::ensureDefaultDirectoriesExist()
{
    QStringList defaultDirs = {
        getTmpDirectory(),
        getConfigDirectory(), 
        getLogDirectory()
    };
    
    for (const QString& relativeDir : defaultDirs) {
        QString absoluteDir = getAbsolutePath(relativeDir);
        QDir dir;
        if (!dir.exists(absoluteDir)) {
            if (!dir.mkpath(absoluteDir)) {
                LOG_ERROR("Failed to create default directory: {}", absoluteDir.toStdString());
            } else {
                LOG_DEBUG("Created default directory: {}", absoluteDir.toStdString());
            }
        }
    }
}

QString ConfigManager::getAbsolutePath(const QString& relativePath) const
{
    if (m_workspaceDir.isEmpty()) {
        LOG_ERROR("Workspace directory not set");
        return relativePath;
    }
    
    QDir wsDir(m_workspaceDir);
    return wsDir.absoluteFilePath(relativePath);
}

bool ConfigManager::isPathInWorkspace(const QString& path) const
{
    if (m_workspaceDir.isEmpty()) {
        return false;
    }
    
    QFileInfo pathInfo(path);
    QString canonicalPath = pathInfo.canonicalFilePath();
    QString canonicalWorkspace = QFileInfo(m_workspaceDir).canonicalFilePath();
    
    return canonicalPath.startsWith(canonicalWorkspace + "/") || canonicalPath == canonicalWorkspace;
}

bool ConfigManager::validateRelativePath(const QString& relativePath) const
{
    if (relativePath.isEmpty()) {
        LOG_WARN("Empty path provided for validation");
        return false;
    }
    
    // Check for directory traversal attempts
    if (relativePath.contains("..")) {
        LOG_ERROR("Path contains directory traversal: {}", relativePath.toStdString());
        return false;
    }
    
    // Check for absolute paths (should be relative only)
    if (QFileInfo(relativePath).isAbsolute()) {
        LOG_ERROR("Absolute path not allowed: {}", relativePath.toStdString());
        return false;
    }
    
    // Check for invalid characters (platform-specific)
    QStringList invalidChars = {"<", ">", ":", "\"", "|", "?", "*"};
    for (const QString& invalidChar : invalidChars) {
        if (relativePath.contains(invalidChar)) {
            LOG_ERROR("Path contains invalid character '{}': {}", invalidChar.toStdString(), relativePath.toStdString());
            return false;
        }
    }
    
    return true;
}

bool ConfigManager::validateAndCreatePath(const QString& relativePath)
{
    if (!validateRelativePath(relativePath)) {
        return false;
    }
    
    QString absolutePath = getAbsolutePath(relativePath);
    
    // Double-check the resulting absolute path is in workspace
    if (!isPathInWorkspace(absolutePath)) {
        LOG_ERROR("Resolved path is outside workspace: {} -> {}", relativePath.toStdString(), absolutePath.toStdString());
        return false;
    }
    
    // Create directory if it doesn't exist
    QDir dir;
    if (!dir.exists(absolutePath)) {
        if (!dir.mkpath(absolutePath)) {
            LOG_ERROR("Failed to create directory: {}", absolutePath.toStdString());
            return false;
        }
        LOG_DEBUG("Created directory: {}", absolutePath.toStdString());
    }
    
    return true;
}

QString ConfigManager::sanitizeRelativePath(const QString& relativePath) const
{
    QString sanitized = relativePath;
    
    // Remove any directory traversal attempts
    sanitized.replace("..", "");
    
    // Replace invalid characters with underscores
    QStringList invalidChars = {"<", ">", ":", "\"", "|", "?", "*"};
    for (const QString& invalidChar : invalidChars) {
        sanitized.replace(invalidChar, "_");
    }
    
    // Clean up multiple slashes
    sanitized.replace(QRegularExpression("/+"), "/");
    
    // Remove leading/trailing slashes
    if (sanitized.startsWith("/")) {
        sanitized = sanitized.mid(1);
    }
    if (sanitized.endsWith("/")) {
        sanitized.chop(1);
    }
    
    LOG_DEBUG("Sanitized path: '{}' -> '{}'", relativePath.toStdString(), sanitized.toStdString());
    return sanitized;
}

QString ConfigManager::buildSafeFilePath(const QString& subdirectory, const QString& filename) const
{
    // Sanitize both subdirectory and filename
    QString safePath = sanitizeRelativePath(subdirectory);
    QString safeFilename = sanitizeRelativePath(filename);
    
    // Build the combined path
    QString relativePath = safePath.isEmpty() ? safeFilename : safePath + "/" + safeFilename;
    
    // Validate the result
    if (!validateRelativePath(relativePath)) {
        LOG_ERROR("Failed to build safe file path: {} / {}", subdirectory.toStdString(), filename.toStdString());
        return QString();
    }
    
    QString absolutePath = getAbsolutePath(relativePath);
    
    // Final validation
    if (!isPathInWorkspace(absolutePath)) {
        LOG_ERROR("Built file path is outside workspace: {}", absolutePath.toStdString());
        return QString();
    }
    
    LOG_TRACE("Built safe file path: {} / {} -> {}", subdirectory.toStdString(), filename.toStdString(), absolutePath.toStdString());
    return absolutePath;
}

void ConfigManager::setupDefaults()
{
    if (!m_settings) return;
    
    LOG_DEBUG("Setting up default configuration values");
    
    // User-configurable directories (relative to workspace)
    if (!m_settings->contains("directories/playlists")) {
        m_settings->setValue("directories/playlists", "config/playlists");
    }
    if (!m_settings->contains("directories/themes")) {
        m_settings->setValue("directories/themes", "config/themes");
    }
    if (!m_settings->contains("directories/audio_cache")) {
        m_settings->setValue("directories/audio_cache", "tmp/audio");
    }
    if (!m_settings->contains("directories/cover_cache")) {
        m_settings->setValue("directories/cover_cache", "tmp/covers");
    }
    
    // Network settings
    if (!m_settings->contains("network/api_base_url")) {
        m_settings->setValue("network/api_base_url", "https://api.bilibili.com");
    }
    if (!m_settings->contains("network/timeout")) {
        m_settings->setValue("network/timeout", 30000);
    }
    if (!m_settings->contains("network/proxy_url")) {
        m_settings->setValue("network/proxy_url", "");
    }
    
    // UI settings
    if (!m_settings->contains("ui/theme")) {
        m_settings->setValue("ui/theme", "dark");
    }
    if (!m_settings->contains("ui/language")) {
        m_settings->setValue("ui/language", "en");
    }
    if (!m_settings->contains("ui/accent_color")) {
        m_settings->setValue("ui/accent_color", "#3498db");
    }
    
    // Playlist settings
    if (!m_settings->contains("playlists/max_recent")) {
        m_settings->setValue("playlists/max_recent", 10);
    }
    if (!m_settings->contains("playlists/auto_save")) {
        m_settings->setValue("playlists/auto_save", true);
    }
    
    m_settings->sync();
    LOG_DEBUG("Default configuration values set");
}

void ConfigManager::loadFromFile()
{
    if (!m_settings) {
        LOG_ERROR("Settings not initialized");
        return;
    }
    
    LOG_INFO("Loading configuration from file");
    
    // Cache frequently used values
    m_currentTheme = m_settings->value("ui/theme", "dark").toString();
    m_currentLanguage = m_settings->value("ui/language", "en").toString();
    
    // Ensure user-configurable directories exist
    ensureUserDirectoriesExist();
    
    LOG_INFO("Configuration loaded successfully");
}

void ConfigManager::ensureUserDirectoriesExist()
{
    QStringList userDirs = {
        getPlaylistDirectory(),
        getThemeDirectory(),
        getAudioCacheDirectory(),
        getCoverCacheDirectory()
    };
    
    for (const QString& relativeDir : userDirs) {
        QString absoluteDir = getAbsolutePath(relativeDir);
        QDir dir;
        if (!dir.exists(absoluteDir)) {
            if (!dir.mkpath(absoluteDir)) {
                LOG_ERROR("Failed to create user directory: {}", absoluteDir.toStdString());
            } else {
                LOG_DEBUG("Created user directory: {}", absoluteDir.toStdString());
            }
        }
    }
}

void ConfigManager::saveToFile()
{
    if (!m_settings) {
        LOG_ERROR("Settings not initialized");
        return;
    }
    
    LOG_DEBUG("Saving configuration to file");
    m_settings->sync();
    
    if (m_settings->status() != QSettings::NoError) {
        LOG_ERROR("Failed to save configuration file");
    } else {
        LOG_DEBUG("Configuration saved successfully");
    }
}

// User-configurable paths
QString ConfigManager::getPlaylistDirectory() const
{
    QString relativePath = m_settings ? m_settings->value("directories/playlists", "config/playlists").toString() : "config/playlists";
    return getAbsolutePath(relativePath);
}

QString ConfigManager::getThemeDirectory() const
{
    QString relativePath = m_settings ? m_settings->value("directories/themes", "config/themes").toString() : "config/themes";
    return getAbsolutePath(relativePath);
}

QString ConfigManager::getAudioCacheDirectory() const
{
    QString relativePath = m_settings ? m_settings->value("directories/audio_cache", "tmp/audio").toString() : "tmp/audio";
    return getAbsolutePath(relativePath);
}

QString ConfigManager::getCoverCacheDirectory() const
{
    QString relativePath = m_settings ? m_settings->value("directories/cover_cache", "tmp/covers").toString() : "tmp/covers";
    return getAbsolutePath(relativePath);
}

void ConfigManager::setPlaylistDirectory(const QString& relativePath)
{
    if (!m_settings) return;
    
    QString sanitizedPath = sanitizeRelativePath(relativePath);
    if (!validateAndCreatePath(sanitizedPath)) {
        LOG_ERROR("Invalid playlist directory path: {}", relativePath.toStdString());
        return;
    }
    
    m_settings->setValue("directories/playlists", sanitizedPath);
    LOG_INFO("Playlist directory changed to: {}", sanitizedPath.toStdString());
    emit playlistDirectoryChanged(sanitizedPath);
}

void ConfigManager::setThemeDirectory(const QString& relativePath)
{
    if (!m_settings) return;
    
    QString sanitizedPath = sanitizeRelativePath(relativePath);
    if (!validateAndCreatePath(sanitizedPath)) {
        LOG_ERROR("Invalid theme directory path: {}", relativePath.toStdString());
        return;
    }
    
    m_settings->setValue("directories/themes", sanitizedPath);
    LOG_INFO("Theme directory changed to: {}", sanitizedPath.toStdString());
}

void ConfigManager::setAudioCacheDirectory(const QString& relativePath)
{
    if (!m_settings) return;
    
    QString sanitizedPath = sanitizeRelativePath(relativePath);
    if (!validateAndCreatePath(sanitizedPath)) {
        LOG_ERROR("Invalid audio cache directory path: {}", relativePath.toStdString());
        return;
    }
    
    m_settings->setValue("directories/audio_cache", sanitizedPath);
    LOG_INFO("Audio cache directory changed to: {}", sanitizedPath.toStdString());
}

void ConfigManager::setCoverCacheDirectory(const QString& relativePath)
{
    if (!m_settings) return;
    
    QString sanitizedPath = sanitizeRelativePath(relativePath);
    if (!validateAndCreatePath(sanitizedPath)) {
        LOG_ERROR("Invalid cover cache directory path: {}", relativePath.toStdString());
        return;
    }
    
    m_settings->setValue("directories/cover_cache", sanitizedPath);
    LOG_INFO("Cover cache directory changed to: {}", sanitizedPath.toStdString());
}

// Network settings
QString ConfigManager::getApiBaseUrl() const
{
    return m_settings ? m_settings->value("network/api_base_url", "https://api.bilibili.com").toString() : "https://api.bilibili.com";
}

int ConfigManager::getNetworkTimeout() const
{
    return m_settings ? m_settings->value("network/timeout", 30000).toInt() : 30000;
}

QString ConfigManager::getProxyUrl() const
{
    return m_settings ? m_settings->value("network/proxy_url", "").toString() : "";
}

void ConfigManager::setApiBaseUrl(const QString& url)
{
    if (!m_settings) return;
    m_settings->setValue("network/api_base_url", url);
    LOG_INFO("API base URL changed to: {}", url.toStdString());
    emit networkSettingsChanged();
}

void ConfigManager::setNetworkTimeout(int timeoutMs)
{
    if (!m_settings) return;
    m_settings->setValue("network/timeout", timeoutMs);
    LOG_INFO("Network timeout changed to: {}ms", timeoutMs);
    emit networkSettingsChanged();
}

void ConfigManager::setProxyUrl(const QString& url)
{
    if (!m_settings) return;
    m_settings->setValue("network/proxy_url", url);
    LOG_INFO("Proxy URL changed to: {}", url.toStdString());
    emit networkSettingsChanged();
}

// UI settings
QString ConfigManager::getTheme() const
{
    return m_currentTheme;
}

QString ConfigManager::getLanguage() const
{
    return m_currentLanguage;
}

QColor ConfigManager::getAccentColor() const
{
    QString colorStr = m_settings ? m_settings->value("ui/accent_color", "#3498db").toString() : "#3498db";
    return QColor(colorStr);
}

void ConfigManager::setTheme(const QString& theme)
{
    if (!m_settings) return;
    
    if (m_currentTheme != theme) {
        m_currentTheme = theme;
        m_settings->setValue("ui/theme", theme);
        LOG_INFO("Theme changed to: {}", theme.toStdString());
        emit themeChanged(theme);
    }
}

void ConfigManager::setLanguage(const QString& language)
{
    if (!m_settings) return;
    
    if (m_currentLanguage != language) {
        m_currentLanguage = language;
        m_settings->setValue("ui/language", language);
        LOG_INFO("Language changed to: {}", language.toStdString());
        emit languageChanged(language);
    }
}

void ConfigManager::setAccentColor(const QColor& color)
{
    if (!m_settings) return;
    
    m_settings->setValue("ui/accent_color", color.name());
    LOG_INFO("Accent color changed to: {}", color.name().toStdString());
    emit accentColorChanged(color);
}

// Playlist settings

QString ConfigManager::getCategoriesFilePath() const
{
    return m_settings ? m_settings->value("playlists/category_file_path", "config/playlists/categories.json").toString() : "config/playlists/categories.json";
}

bool ConfigManager::getAutoSaveEnabled() const
{
    return m_settings ? m_settings->value("playlists/auto_save", true).toBool() : true;
}

int ConfigManager::getAutoSaveInterval() const
{
    return m_settings ? m_settings->value("playlists/auto_save_interval", 5).toInt() : 5;
}

QString ConfigManager::setCategoriesFilePath(const QString& relativePath)
{
    if (!m_settings) return QString();
    
    QString sanitizedPath = sanitizeRelativePath(relativePath);
    if (!validateAndCreatePath(sanitizedPath)) {
        LOG_ERROR("Invalid categories file path: {}", relativePath.toStdString());
        return QString();
    }
    
    m_settings->setValue("playlists/category_file_path", sanitizedPath);
    LOG_INFO("Categories file path changed to: {}", sanitizedPath.toStdString());
    return sanitizedPath;
}

void ConfigManager::setAutoSaveEnabled(bool enabled)
{
    if (!m_settings) return;
    
    m_settings->setValue("playlists/auto_save", enabled);
    LOG_INFO("Auto-save enabled: {}", enabled);
}

void ConfigManager::setAutoSaveInterval(int intervalMinutes)
{
    if (!m_settings) return;
    
    m_settings->setValue("playlists/auto_save_interval", intervalMinutes);
    LOG_INFO("Auto-save interval changed to: {} minutes", intervalMinutes);
}

// Error logging
void ConfigManager::logNetworkError(const QString& error)
{
    if (!m_settings) return;
    
    // Keep last 100 errors
    QStringList errors = m_settings->value("errors/network", QStringList()).toStringList();
    errors.prepend(QString("%1: %2").arg(QDateTime::currentDateTime().toString()).arg(error));
    
    while (errors.size() > 100) {
        errors.removeLast();
    }
    
    m_settings->setValue("errors/network", errors);
    LOG_DEBUG("Logged network error: {}", error.toStdString());
}

QStringList ConfigManager::getRecentErrors() const
{
    return m_settings ? m_settings->value("errors/network", QStringList()).toStringList() : QStringList();
}