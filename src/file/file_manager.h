#pragma once

#include <QObject>
#include <QString>
#include <QDir>

class ConfigManager;

/**
 * File Manager - handles file I/O operations
 * Depends on ConfigManager for paths and settings
 */
class FileManager : public QObject
{
    Q_OBJECT
    
public:
    explicit FileManager(const QString& dataPath, const QString& cachePath, QObject* parent = nullptr);
    ~FileManager();
    
    // Directory management
    void ensureDirectoriesExist();
    QString getPlaylistsDirectory() const;
    QString getCacheDirectory() const;
    QString getLogsDirectory() const;
    
    // Playlist file operations
    bool savePlaylistToFile(const class Playlist& playlist);
    bool loadPlaylistFromFile(const QString& playlistId, class Playlist& playlist);
    bool removePlaylistFile(const QString& playlistId);
    QStringList getAvailablePlaylistFiles() const;
    
    // Cache operations
    bool saveCacheData(const QString& key, const QByteArray& data);
    QByteArray loadCacheData(const QString& key) const;
    void clearCache();
    qint64 getCacheSize() const;
    
    // Backup operations
    bool createBackup(const QString& name = "");
    bool restoreFromBackup(const QString& backupName);
    QStringList getAvailableBackups() const;
    
signals:
    void fileOperationCompleted(const QString& operation, bool success);
    void cacheCleared();
    
private:
    QString m_dataPath;
    QString m_cachePath;
    
    QString getPlaylistFilePath(const QString& playlistId) const;
    QString getCacheFilePath(const QString& key) const;
    QString getBackupPath(const QString& backupName = "") const;
};