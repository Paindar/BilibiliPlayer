#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QUuid>
class QTimer;
#include <QReadWriteLock>
#include <QTextStream>
#include <QFile>
#include <optional>
#include <functional>
#include "playlist.h"

// Forward declarations
class ConfigManager;

/**
 * Playlist Manager - manages category and playlist data using workspace-relative paths
 * Each category and playlist has unique QUuid identifier for safe management
 * All file operations are constrained to workspace directory
 * Uses JSON format for saving category-playlist hierarchical data
 * 
 * Config use:
 * CategoryFilePath: path to JSON file storing category and playlist data
 * AutoSaveEnabled: whether to auto-save changes periodically
 * AutoSaveInterval: interval in minutes for auto-saving (default 5 minutes)
 */
class PlaylistManager : public QObject
{
    Q_OBJECT
    
public:
    explicit PlaylistManager(ConfigManager* configManager, QObject* parent = nullptr);
    ~PlaylistManager();
    
    // Initialization - load existing data from workspace
    void initialize();
    void loadCategoriesFromFile();
    void saveAllCategories();
    
    // Category operations with UUID-based identification
    bool addCategory(const playlist::CategoryInfo& category);
    bool removeCategory(const QUuid& categoryId);
    bool updateCategory(const playlist::CategoryInfo& category);
    std::optional<playlist::CategoryInfo> getCategory(const QUuid& categoryId) const;
    QList<playlist::CategoryInfo> iterateCategories(std::function<bool(const playlist::CategoryInfo&)> func) const;
    
    // Playlist operations with UUID-based identification
    bool addPlaylist(const playlist::PlaylistInfo& playlist, const QUuid& categoryId);
    bool removePlaylist(const QUuid& playlistId, const QUuid& categoryId = QUuid());
    bool updatePlaylist(const playlist::PlaylistInfo& playlist, const QUuid& categoryId);
    std::optional<playlist::PlaylistInfo> getPlaylist(const QUuid& playlistId) const;
    QList<playlist::PlaylistInfo> iteratePlaylistsInCategory(const QUuid& categoryId, std::function<bool(const playlist::PlaylistInfo&)> func) const;
    // Song operations
    bool addSongToPlaylist(const playlist::SongInfo& song, const QUuid& playlistId);
    bool removeSongFromPlaylist(const playlist::SongInfo& song, const QUuid& playlistId);
    bool updateSongInPlaylist(const playlist::SongInfo& song, const QUuid& playlistId);
    std::optional<playlist::SongInfo> getSongFromPlaylist(const QUuid& songId, const QUuid& playlistId) const;
    QList<playlist::SongInfo> iterateSongsInPlaylist(const QUuid& playlistId, std::function<bool(const playlist::SongInfo&)> func) const;

    // UUID-based lookup
    bool categoryExists(const QUuid& categoryId) const;
    bool playlistExists(const QUuid& playlistId) const;
    
    // Current playlist management
    QUuid getCurrentPlaylist();
    void setCurrentPlaylist(const QUuid& playlistId);
signals:
    void categoryAdded(const playlist::CategoryInfo& category);
    void categoryRemoved(const QUuid& categoryId);
    void categoryUpdated(const playlist::CategoryInfo& category);
    void playlistAdded(const playlist::PlaylistInfo& playlist, const QUuid& categoryId);
    void playlistRemoved(const QUuid& playlistId, const QUuid& categoryId);
    void playlistUpdated(const playlist::PlaylistInfo& playlist, const QUuid& categoryId);
    void songAdded(const playlist::SongInfo& song, const QUuid& playlistId);
    void songRemoved(const playlist::SongInfo& song, const QUuid& playlistId);
    void songUpdated(const playlist::SongInfo& song, const QUuid& playlistId);
    void playlistSongsChanged(const QUuid& playlistId);
    void categoriesLoaded(int categoryCount, int playlistCount);
    void currentPlaylistChanged(const QUuid& playlistId);

private slots:
    void onConfigChanged();
    void onAutoSaveTimer();
    
private:
    // Initialization helpers
    void ensurePlaylistDirectoryExists();
    void setupAutoSave();
    bool saveCategoriesToJsonFile();
    bool loadCategoriesFromJsonFile();
    
    // Helper methods
    QUuid getPlaylistCategoryId(const QUuid& playlistId) const;
    QString generateStreamingFilepath(const playlist::SongInfo& song) const;
    
    // Default setup helpers
    QUuid ensureDefaultSetup();
    
    ConfigManager* m_configManager;
    
    // Thread synchronization
    mutable QReadWriteLock m_dataLock;
    
    // Core data storage
    QHash<QUuid, playlist::CategoryInfo> m_categories;
    QHash<QUuid, playlist::PlaylistInfo> m_playlists;
    
    // Relationship mappings
    QHash<QUuid, QList<QUuid>> m_categoryPlaylists; // CategoryId -> List of PlaylistIds
    QHash<QUuid, QList<playlist::SongInfo>> m_playlistSongs;
    
    QTimer* m_autoSaveTimer;
    QUuid m_currentPlaylistId;
    bool m_initialized = false;
};