#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QUuid>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QFile>
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
    playlist::CategoryInfo getCategory(const QUuid& categoryId) const;
    QList<playlist::CategoryInfo> getAllCategories() const;
    
    // Playlist operations with UUID-based identification
    bool addPlaylist(const playlist::PlaylistInfo& playlist, const QUuid& categoryId);
    bool removePlaylist(const QUuid& playlistId);
    bool updatePlaylist(const playlist::PlaylistInfo& playlist);
    playlist::PlaylistInfo getPlaylist(const QUuid& playlistId) const;
    QList<playlist::PlaylistInfo> getAllPlaylists() const;
    QList<playlist::PlaylistInfo> getPlaylistsInCategory(const QUuid& categoryId) const;
    
    // UUID-based lookup
    bool categoryExists(const QUuid& categoryId) const;
    bool playlistExists(const QUuid& playlistId) const;
signals:
    void categoryAdded(const playlist::CategoryInfo& category);
    void categoryRemoved(const QUuid& categoryId);
    void categoryUpdated(const playlist::CategoryInfo& category);
    void playlistAdded(const playlist::PlaylistInfo& playlist, const QUuid& categoryId);
    void playlistRemoved(const QUuid& playlistId);
    void playlistUpdated(const playlist::PlaylistInfo& playlist);
    void categoriesLoaded(int categoryCount, int playlistCount);

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
    playlist::CategoryInfo* findCategoryById(const QUuid& categoryId);
    const playlist::CategoryInfo* findCategoryById(const QUuid& categoryId) const;
    playlist::PlaylistInfo* findPlaylistById(const QUuid& playlistId, QUuid* categoryId = nullptr);
    const playlist::PlaylistInfo* findPlaylistById(const QUuid& playlistId, QUuid* categoryId = nullptr) const;
    
    ConfigManager* m_configManager;
    QList<playlist::CategoryInfo> m_categories;
    QTimer* m_autoSaveTimer;
    bool m_initialized = false;
};