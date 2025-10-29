#include "playlist_manager.h"
#include "../config/config_manager.h"
#include "../log/log_manager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <algorithm>
#include <fmt/format.h>

PlaylistManager::PlaylistManager(ConfigManager* configManager, QObject* parent)
    : QObject(parent)
    , m_configManager(configManager)
    , m_autoSaveTimer(nullptr)
    , m_initialized(false)
{
    if (!m_configManager) {
        LOG_CRITICAL("PlaylistManager created with null ConfigManager");
        throw std::runtime_error("ConfigManager cannot be null");
    }
    
    LOG_DEBUG("PlaylistManager created");
}

PlaylistManager::~PlaylistManager()
{
    if (m_initialized) {
        LOG_INFO("PlaylistManager destructor: saving all categories before shutdown");
        saveAllCategories();
    }
    
    if (m_autoSaveTimer) {
        m_autoSaveTimer->stop();
        delete m_autoSaveTimer;
    }
    
    LOG_DEBUG("PlaylistManager destroyed");
}

void PlaylistManager::initialize()
{
    if (m_initialized) {
        LOG_WARN("PlaylistManager already initialized");
        return;
    }
    
    LOG_INFO("Initializing PlaylistManager...");
    
    try {
        // Ensure playlist directory exists
        ensurePlaylistDirectoryExists();
        
        // Setup auto-save timer
        setupAutoSave();
        
        // Load existing categories
        loadCategoriesFromFile();
        
        // Connect config change signal
        if (m_configManager) {
            connect(m_configManager, &ConfigManager::pathsChanged, this, &PlaylistManager::onConfigChanged);
        }
        
        m_initialized = true;
        LOG_INFO("PlaylistManager initialization complete");
        
    } catch (const std::exception& e) {
        LOG_ERROR("PlaylistManager initialization failed: {}", e.what());
        throw;
    }
}

void PlaylistManager::loadCategoriesFromFile()
{
    LOG_INFO("Loading categories from JSON file...");
    
    if (loadCategoriesFromJsonFile()) {
        int totalPlaylists = 0;
        for (const auto& category : m_categories) {
            totalPlaylists += category.playlists.size();
        }
        LOG_INFO("Categories loaded from JSON: {} categories, {} playlists", m_categories.size(), totalPlaylists);
        emit categoriesLoaded(m_categories.size(), totalPlaylists);
        return;
    }
    
    emit categoriesLoaded(1, 0);
}

void PlaylistManager::saveAllCategories()
{
    if (!m_initialized) {
        LOG_WARN("Attempted to save categories before initialization");
        return;
    }
    
    LOG_INFO("Saving all categories to JSON...");
    
    if (saveCategoriesToJsonFile()) {
        int totalPlaylists = 0;
        for (const auto& category : m_categories) {
            totalPlaylists += category.playlists.size();
        }
        LOG_INFO("Save categories complete: {} categories, {} playlists", m_categories.size(), totalPlaylists);
    } else {
        LOG_ERROR("Failed to save categories to JSON file");
    }
}

// Category operations
bool PlaylistManager::addCategory(const playlist::CategoryInfo& category)
{
    if (!m_initialized) {
        LOG_ERROR("PlaylistManager not initialized");
        return false;
    }
    
    if (category.uuid.isNull()) {
        LOG_ERROR("Cannot add category with null UUID");
        return false;
    }
    
    if (categoryExists(category.uuid)) {
        LOG_ERROR("Category with UUID {} already exists", category.uuid.toString().toStdString());
        return false;
    }
    
    m_categories.append(category);
    emit categoryAdded(category);
    LOG_INFO("Added category: {} ({})", category.name.toStdString(), category.uuid.toString().toStdString());
    return true;
}

bool PlaylistManager::removeCategory(const QUuid& categoryId)
{
    if (!m_initialized) {
        LOG_ERROR("PlaylistManager not initialized");
        return false;
    }
    
    if (categoryId.isNull()) {
        LOG_ERROR("Cannot remove category with null UUID");
        return false;
    }
    
    auto it = std::find_if(m_categories.begin(), m_categories.end(),
        [categoryId](const playlist::CategoryInfo& category) {
            return category.uuid == categoryId;
        });
    
    if (it == m_categories.end()) {
        LOG_ERROR("Category with UUID {} not found", categoryId.toString().toStdString());
        return false;
    }
    
    QString categoryName = it->name;
    m_categories.erase(it);
    emit categoryRemoved(categoryId);
    LOG_INFO("Removed category: {} ({})", categoryName.toStdString(), categoryId.toString().toStdString());
    return true;
}

bool PlaylistManager::updateCategory(const playlist::CategoryInfo& category)
{
    if (!m_initialized) {
        LOG_ERROR("PlaylistManager not initialized");
        return false;
    }
    
    if (category.uuid.isNull()) {
        LOG_ERROR("Cannot update category with null UUID");
        return false;
    }
    
    playlist::CategoryInfo* existingCategory = findCategoryById(category.uuid);
    if (!existingCategory) {
        LOG_ERROR("Category with UUID {} not found", category.uuid.toString().toStdString());
        return false;
    }
    
    *existingCategory = category;
    emit categoryUpdated(category);
    LOG_INFO("Updated category: {} ({})", category.name.toStdString(), category.uuid.toString().toStdString());
    return true;
}

playlist::CategoryInfo PlaylistManager::getCategory(const QUuid& categoryId) const
{
    const playlist::CategoryInfo* category = findCategoryById(categoryId);
    return category ? *category : playlist::CategoryInfo();
}

QList<playlist::CategoryInfo> PlaylistManager::getAllCategories() const
{
    return m_categories;
}

// Playlist operations
bool PlaylistManager::addPlaylist(const playlist::PlaylistInfo& playlist, const QUuid& categoryId)
{
    if (!m_initialized) {
        LOG_ERROR("PlaylistManager not initialized");
        return false;
    }
    
    if (playlist.uuid.isNull()) {
        LOG_ERROR("Cannot add playlist with null UUID");
        return false;
    }
    
    if (playlistExists(playlist.uuid)) {
        LOG_ERROR("Playlist with UUID {} already exists", playlist.uuid.toString().toStdString());
        return false;
    }
    
    // Find target category (use first category if categoryId is null)
    QUuid targetCategoryId = categoryId;
    if (targetCategoryId.isNull() && !m_categories.isEmpty()) {
        targetCategoryId = m_categories.first().uuid;
    }
    
    playlist::CategoryInfo* category = findCategoryById(targetCategoryId);
    if (!category) {
        LOG_ERROR("Target category with UUID {} not found", targetCategoryId.toString().toStdString());
        return false;
    }
    
    category->playlists.append(playlist);
    emit playlistAdded(playlist, targetCategoryId);
    LOG_INFO("Added playlist: {} ({}) to category: {} ({})", 
             playlist.name.toStdString(), playlist.uuid.toString().toStdString(),
             category->name.toStdString(), targetCategoryId.toString().toStdString());
    return true;
}

bool PlaylistManager::removePlaylist(const QUuid& playlistId)
{
    if (!m_initialized) {
        LOG_ERROR("PlaylistManager not initialized");
        return false;
    }
    
    if (playlistId.isNull()) {
        LOG_ERROR("Cannot remove playlist with null UUID");
        return false;
    }
    
    QUuid categoryId;
    playlist::PlaylistInfo* playlist = findPlaylistById(playlistId, &categoryId);
    if (!playlist) {
        LOG_ERROR("Playlist with UUID {} not found", playlistId.toString().toStdString());
        return false;
    }
    
    playlist::CategoryInfo* category = findCategoryById(categoryId);
    if (!category) {
        LOG_ERROR("Parent category not found for playlist {}", playlistId.toString().toStdString());
        return false;
    }
    
    QString playlistName = playlist->name;
    auto it = std::find_if(category->playlists.begin(), category->playlists.end(),
        [playlistId](const playlist::PlaylistInfo& p) {
            return p.uuid == playlistId;
        });
    
    if (it != category->playlists.end()) {
        category->playlists.erase(it);
        emit playlistRemoved(playlistId);
        LOG_INFO("Removed playlist: {} ({}) from category: {}", 
                 playlistName.toStdString(), playlistId.toString().toStdString(),
                 category->name.toStdString());
        return true;
    }
    
    return false;
}

bool PlaylistManager::updatePlaylist(const playlist::PlaylistInfo& playlist)
{
    if (!m_initialized) {
        LOG_ERROR("PlaylistManager not initialized");
        return false;
    }
    
    if (playlist.uuid.isNull()) {
        LOG_ERROR("Cannot update playlist with null UUID");
        return false;
    }
    
    playlist::PlaylistInfo* existingPlaylist = findPlaylistById(playlist.uuid);
    if (!existingPlaylist) {
        LOG_ERROR("Playlist with UUID {} not found", playlist.uuid.toString().toStdString());
        return false;
    }
    
    *existingPlaylist = playlist;
    emit playlistUpdated(playlist);
    LOG_INFO("Updated playlist: {} ({})", playlist.name.toStdString(), playlist.uuid.toString().toStdString());
    return true;
}

playlist::PlaylistInfo PlaylistManager::getPlaylist(const QUuid& playlistId) const
{
    const playlist::PlaylistInfo* playlist = findPlaylistById(playlistId);
    return playlist ? *playlist : playlist::PlaylistInfo();
}

QList<playlist::PlaylistInfo> PlaylistManager::getAllPlaylists() const
{
    QList<playlist::PlaylistInfo> allPlaylists;
    for (const auto& category : m_categories) {
        allPlaylists.append(category.playlists);
    }
    return allPlaylists;
}

QList<playlist::PlaylistInfo> PlaylistManager::getPlaylistsInCategory(const QUuid& categoryId) const
{
    const playlist::CategoryInfo* category = findCategoryById(categoryId);
    return category ? category->playlists : QList<playlist::PlaylistInfo>();
}

// UUID-based lookup
bool PlaylistManager::categoryExists(const QUuid& categoryId) const
{
    return findCategoryById(categoryId) != nullptr;
}

bool PlaylistManager::playlistExists(const QUuid& playlistId) const
{
    return findPlaylistById(playlistId) != nullptr;
}

// Private slots
void PlaylistManager::onConfigChanged()
{
    LOG_DEBUG("Configuration changed, checking if playlist directory needs to be recreated");
    ensurePlaylistDirectoryExists();
}

void PlaylistManager::onAutoSaveTimer()
{
    if (m_initialized) {
        LOG_DEBUG("Auto-save timer triggered");
        saveAllCategories();
    }
}

// Private helper methods
void PlaylistManager::ensurePlaylistDirectoryExists()
{
    if (!m_configManager) {
        LOG_ERROR("ConfigManager is null, cannot ensure playlist directory exists");
        return;
    }
    
    QString playlistDir = m_configManager->getPlaylistDirectory();
    if (playlistDir.isEmpty()) {
        LOG_ERROR("Playlist directory path is empty");
        return;
    }
    
    QDir dir;
    if (!dir.exists(playlistDir)) {
        if (dir.mkpath(playlistDir)) {
            LOG_INFO("Created playlist directory: {}", playlistDir.toStdString());
        } else {
            LOG_ERROR("Failed to create playlist directory: {}", playlistDir.toStdString());
            throw std::runtime_error("Failed to create playlist directory");
        }
    } else {
        LOG_DEBUG("Playlist directory already exists: {}", playlistDir.toStdString());
    }
}

void PlaylistManager::setupAutoSave()
{
    if (m_autoSaveTimer) {
        m_autoSaveTimer->stop();
        delete m_autoSaveTimer;
    }
    if (m_configManager->getAutoSaveEnabled() == false) {
        LOG_INFO("Auto-save is disabled in configuration");
        m_autoSaveTimer = nullptr;
        return;
    }
    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &PlaylistManager::onAutoSaveTimer);
    
    // Auto-save every 5 minutes
    int intervalMinutes = m_configManager->getAutoSaveInterval();
    m_autoSaveTimer->start(intervalMinutes * 60 * 1000);
    LOG_DEBUG("Auto-save timer setup complete ({} minute interval)", intervalMinutes);
}

bool PlaylistManager::saveCategoriesToJsonFile()
{
    LOG_INFO("Saving categories to JSON file...");
    QJsonArray jsonCategories;
    for (const auto& category : m_categories) {
        QJsonObject jsonCategory;
        jsonCategory["name"] = category.name;
        jsonCategory["uuid"] = category.uuid.toString();
        
        QJsonArray jsonPlaylists;
        for (const auto& playlist : category.playlists) {
            QJsonObject jsonPlaylist;
            jsonPlaylist["name"] = playlist.name;
            jsonPlaylist["creator"] = playlist.creator;
            jsonPlaylist["description"] = playlist.description;
            jsonPlaylist["coverUrl"] = playlist.coverUrl;
            jsonPlaylist["uuid"] = playlist.uuid.toString();
            
            QJsonArray jsonSongs;
            for (const auto& song : playlist.songs) {
                QJsonObject jsonSong;
                jsonSong["title"] = song.title;
                jsonSong["uploader"] = song.uploader;
                jsonSong["platform"] = song.platform;
                jsonSong["duration"] = song.duration;
                jsonSong["args"] = song.args;
                jsonSongs.append(jsonSong);
            }
            jsonPlaylist["songs"] = jsonSongs;
            jsonPlaylists.append(jsonPlaylist);
        }
        jsonCategory["playlists"] = jsonPlaylists;
        jsonCategories.append(jsonCategory);
    }
    QJsonObject rootObj;
    rootObj["categories"] = jsonCategories;
    QJsonDocument doc(rootObj);
    QByteArray data = doc.toJson(QJsonDocument::Indented);
    QFile file(m_configManager->getCategoriesFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR("Failed to open category JSON file for writing: {}", file.fileName().toStdString());
        return false;
    }
    file.write(data);
    file.close();
    LOG_INFO("Categories successfully saved to JSON file: {}", file.fileName().toStdString());
    return true;
}

bool PlaylistManager::loadCategoriesFromJsonFile()
{
    LOG_INFO("Loading categories from JSON file...");
    QJsonArray jsonCategories;

    QFile file(m_configManager->getCategoriesFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_WARN("Category JSON file not found: {}", file.fileName().toStdString());
        return false;
    }
    QByteArray data = file.readAll();
    file.close();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();
    jsonCategories = obj["categories"].toArray();
    m_categories.clear();
    for (const auto& jsonCategoryValue : jsonCategories) {
        QJsonObject jsonCategory = jsonCategoryValue.toObject();
        playlist::CategoryInfo category;
        category.name = jsonCategory["name"].toString();
        category.uuid = QUuid(jsonCategory["uuid"].toString());
        
        QJsonArray jsonPlaylists = jsonCategory["playlists"].toArray();
        for (const auto& jsonPlaylistValue : jsonPlaylists) {
            QJsonObject jsonPlaylist = jsonPlaylistValue.toObject();
            playlist::PlaylistInfo playlist;
            playlist.name = jsonPlaylist["name"].toString();
            playlist.creator = jsonPlaylist["creator"].toString();
            playlist.description = jsonPlaylist["description"].toString();
            playlist.coverUrl = jsonPlaylist["coverUrl"].toString();
            playlist.uuid = QUuid(jsonPlaylist["uuid"].toString());
            
            QJsonArray jsonSongs = jsonPlaylist["songs"].toArray();
            for (const auto& jsonSongValue : jsonSongs) {
                QJsonObject jsonSong = jsonSongValue.toObject();
                playlist::SongInfo song;
                song.title = jsonSong["title"].toString();
                song.uploader = jsonSong["uploader"].toString();
                song.platform = jsonSong["platform"].toString();
                song.duration = jsonSong["duration"].toString();
                song.args = jsonSong["args"].toString();
                playlist.songs.append(song);
            }
            category.playlists.append(playlist);
        }
        m_categories.append(category);
    }
    return true;
}

playlist::CategoryInfo* PlaylistManager::findCategoryById(const QUuid& categoryId)
{
    auto it = std::find_if(m_categories.begin(), m_categories.end(),
        [categoryId](const playlist::CategoryInfo& category) {
            return category.uuid == categoryId;
        });
    return (it != m_categories.end()) ? &(*it) : nullptr;
}

const playlist::CategoryInfo* PlaylistManager::findCategoryById(const QUuid& categoryId) const
{
    auto it = std::find_if(m_categories.begin(), m_categories.end(),
        [categoryId](const playlist::CategoryInfo& category) {
            return category.uuid == categoryId;
        });
    return (it != m_categories.end()) ? &(*it) : nullptr;
}

playlist::PlaylistInfo* PlaylistManager::findPlaylistById(const QUuid& playlistId, QUuid* categoryId)
{
    for (auto& category : m_categories) {
        auto it = std::find_if(category.playlists.begin(), category.playlists.end(),
            [playlistId](const playlist::PlaylistInfo& playlist) {
                return playlist.uuid == playlistId;
            });
        if (it != category.playlists.end()) {
            if (categoryId) *categoryId = category.uuid;
            return &(*it);
        }
    }
    return nullptr;
}

const playlist::PlaylistInfo* PlaylistManager::findPlaylistById(const QUuid& playlistId, QUuid* categoryId) const
{
    for (const auto& category : m_categories) {
        auto it = std::find_if(category.playlists.begin(), category.playlists.end(),
            [playlistId](const playlist::PlaylistInfo& playlist) {
                return playlist.uuid == playlistId;
            });
        if (it != category.playlists.end()) {
            if (categoryId) *categoryId = category.uuid;
            return &(*it);
        }
    }
    return nullptr;
}
