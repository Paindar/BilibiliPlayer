#include "playlist_manager.h"
#include "../config/config_manager.h"
#include "../log/log_manager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>
#include <optional>
#include <functional>
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
        int totalPlaylists = m_playlists.size();
        
        // Validate current playlist ID after loading
        if (!m_currentPlaylistId.isNull() && !playlistExists(m_currentPlaylistId)) {
            LOG_WARN("Loaded current playlist ID is invalid, will reset to default");
            m_currentPlaylistId = QUuid();
        }
        
        LOG_INFO("Categories loaded from JSON: {} categories, {} playlists", m_categories.size(), totalPlaylists);
        emit categoriesLoaded(m_categories.size(), totalPlaylists);
        return;
    }
    
    // No file found, ensure default setup exists
    m_currentPlaylistId = QUuid();
    emit categoriesLoaded(0, 0);
}

void PlaylistManager::saveAllCategories()
{
    if (!m_initialized) {
        LOG_WARN("PlaylistManager not initialized, cannot save categories");
        return;
    }
    
    LOG_INFO("Saving all categories...");
    
    if (saveCategoriesToJsonFile()) {
        int totalPlaylists = m_playlists.size();
        LOG_INFO("Save categories complete: {} categories, {} playlists", m_categories.size(), totalPlaylists);
    } else {
        LOG_ERROR("Failed to save categories");
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
    
    QWriteLocker locker(&m_dataLock);
    if (m_categories.contains(category.uuid)) {
        LOG_ERROR("Category with UUID {} already exists", category.uuid.toString().toStdString());
        return false;
    }
    
    m_categories.insert(category.uuid, category);
    m_categoryPlaylists.insert(category.uuid, QList<QUuid>());
    locker.unlock(); // Unlock before emitting signal
    
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
    
    QList<QUuid> playlistIds;
    {
        QReadLocker readLocker(&m_dataLock);
        if (!m_categories.contains(categoryId)) {
            LOG_ERROR("Category with UUID {} not found", categoryId.toString().toStdString());
            return false;
        }
        playlistIds = m_categoryPlaylists.value(categoryId);
    }
    
    // Remove all playlists in this category first (without holding the lock during recursive calls)
    for (const QUuid& playlistId : playlistIds) {
        removePlaylist(playlistId, categoryId);
    }
    
    // Remove category itself
    QWriteLocker writeLocker(&m_dataLock);
    m_categories.remove(categoryId);
    m_categoryPlaylists.remove(categoryId);
    writeLocker.unlock();
    
    emit categoryRemoved(categoryId);
    LOG_INFO("Removed category: {}", categoryId.toString().toStdString());
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
    
    QWriteLocker locker(&m_dataLock);
    if (!m_categories.contains(category.uuid)) {
        LOG_ERROR("Category with UUID {} not found", category.uuid.toString().toStdString());
        return false;
    }
    
    m_categories[category.uuid] = category;
    locker.unlock();
    
    emit categoryUpdated(category);
    LOG_INFO("Updated category: {} ({})", category.name.toStdString(), category.uuid.toString().toStdString());
    return true;
}

std::optional<playlist::CategoryInfo> PlaylistManager::getCategory(const QUuid& categoryId) const
{
    QReadLocker locker(&m_dataLock);
    auto it = m_categories.find(categoryId);
    if (it == m_categories.end()) {
        LOG_ERROR("Category with ID {} not found", categoryId.toString().toStdString());
        return std::nullopt;
    }
    return it.value();
}

QList<playlist::CategoryInfo> PlaylistManager::iterateCategories(std::function<bool(const playlist::CategoryInfo&)> func) const
{
    QReadLocker locker(&m_dataLock);
    QList<playlist::CategoryInfo> result;
    for (const auto& category : m_categories) {
        if (func(category)) {
            result.append(category);
        }
    }
    return result;
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
    
    QWriteLocker locker(&m_dataLock);
    if (m_playlists.contains(playlist.uuid)) {
        LOG_ERROR("Playlist with UUID {} already exists", playlist.uuid.toString().toStdString());
        return false;
    }
    
    // Find target category (use first category if categoryId is null)
    QUuid targetCategoryId = categoryId;
    if (targetCategoryId.isNull() && !m_categories.isEmpty()) {
        targetCategoryId = m_categories.begin().key();
    }
    
    if (!m_categories.contains(targetCategoryId)) {
        LOG_ERROR("Target category with UUID {} not found", targetCategoryId.toString().toStdString());
        return false;
    }
    
    // Add playlist to storage
    m_playlists.insert(playlist.uuid, playlist);
    m_playlistSongs.insert(playlist.uuid, QList<playlist::SongInfo>());
    
    // Add relationship
    m_categoryPlaylists[targetCategoryId].append(playlist.uuid);
    locker.unlock();
    
    emit playlistAdded(playlist, targetCategoryId);
    LOG_INFO("Added playlist: {} ({}) to category: {}", 
             playlist.name.toStdString(), playlist.uuid.toString().toStdString(), targetCategoryId.toString().toStdString());
    return true;
}

bool PlaylistManager::removePlaylist(const QUuid& playlistId, const QUuid& categoryId)
{
    if (!m_initialized) {
        LOG_ERROR("PlaylistManager not initialized");
        return false;
    }
    
    if (playlistId.isNull()) {
        LOG_ERROR("Cannot remove playlist with null UUID");
        return false;
    }
    
    if (!playlistExists(playlistId)) {
        LOG_ERROR("Playlist with UUID {} not found", playlistId.toString().toStdString());
        return false;
    }
    
    // Find which category contains this playlist
    QUuid foundCategoryId = categoryId;
    if (foundCategoryId.isNull()) {
        foundCategoryId = getPlaylistCategoryId(playlistId);
    }
    if (foundCategoryId.isNull()) {
        LOG_ERROR("Cannot find category for playlist {}", playlistId.toString().toStdString());
        return false;
    }
    
    // Get playlist info for logging
    auto playlistIt = m_playlists.find(playlistId);
    QString playlistName = (playlistIt != m_playlists.end()) ? playlistIt.value().name : "Unknown";
    
    // Remove from relationships
    m_categoryPlaylists[foundCategoryId].removeAll(playlistId);
    
    // Remove playlist and its songs
    m_playlists.remove(playlistId);
    m_playlistSongs.remove(playlistId);
    
    emit playlistRemoved(playlistId, foundCategoryId);
    LOG_INFO("Removed playlist: {} ({}) from category: {}", 
             playlistName.toStdString(), playlistId.toString().toStdString(), foundCategoryId.toString().toStdString());
    return true;
}

bool PlaylistManager::updatePlaylist(const playlist::PlaylistInfo& playlist, const QUuid& categoryId)
{
    if (!m_initialized) {
        LOG_ERROR("PlaylistManager not initialized");
        return false;
    }
    
    if (playlist.uuid.isNull()) {
        LOG_ERROR("Cannot update playlist with null UUID");
        return false;
    }
    
    if (!playlistExists(playlist.uuid)) {
        LOG_ERROR("Playlist with UUID {} not found", playlist.uuid.toString().toStdString());
        return false;
    }
    
    m_playlists[playlist.uuid] = playlist;
    emit playlistUpdated(playlist, categoryId);
    LOG_INFO("Updated playlist: {} ({})", playlist.name.toStdString(), playlist.uuid.toString().toStdString());
    return true;
}

std::optional<playlist::PlaylistInfo> PlaylistManager::getPlaylist(const QUuid& playlistId) const
{
    QReadLocker locker(&m_dataLock);
    auto it = m_playlists.find(playlistId);
    if (it == m_playlists.end()) {
        LOG_ERROR("Playlist with ID {} not found", playlistId.toString().toStdString());
        return std::nullopt;
    }
    return it.value();
}

QList<playlist::PlaylistInfo> PlaylistManager::iteratePlaylistsInCategory(const QUuid& categoryId, std::function<bool(const playlist::PlaylistInfo&)> func) const
{
    QReadLocker locker(&m_dataLock);
    QList<playlist::PlaylistInfo> result;
    auto playlistIdsIt = m_categoryPlaylists.find(categoryId);
    if (playlistIdsIt != m_categoryPlaylists.end()) {
        for (const QUuid& playlistId : playlistIdsIt.value()) {
            auto playlistIt = m_playlists.find(playlistId);
            if (playlistIt != m_playlists.end()) {
                if (func(playlistIt.value())) {
                    result.append(playlistIt.value());
                }
            }
        }
    }
    return result;
}

// Song operations
bool PlaylistManager::addSongToPlaylist(const playlist::SongInfo& song, const QUuid& playlistId)
{
    if (!m_initialized) {
        LOG_ERROR("PlaylistManager not initialized");
        return false;
    }
    
    QWriteLocker locker(&m_dataLock);
    if (!m_playlists.contains(playlistId)) {
        LOG_ERROR("Playlist with UUID {} not found", playlistId.toString().toStdString());
        return false;
    }
    
    m_playlistSongs[playlistId].append(song);
    auto playlistIt = m_playlists.find(playlistId);
    QString playlistName = (playlistIt != m_playlists.end()) ? playlistIt.value().name : "Unknown";
    locker.unlock();
    
    // Emit signals
    emit songAdded(song, playlistId);
    emit playlistSongsChanged(playlistId);
    
    LOG_INFO("Added song: {} to playlist: {} ({})",
             song.title.toStdString(), 
             playlistName.toStdString(), 
             playlistId.toString().toStdString());
    return true;
}

bool PlaylistManager::removeSongFromPlaylist(const playlist::SongInfo& song, const QUuid& playlistId)
{
    if (!m_initialized) {
        LOG_ERROR("PlaylistManager not initialized");
        return false;
    }
    
    QWriteLocker locker(&m_dataLock);
    
    auto songsIt = m_playlistSongs.find(playlistId);
    if (songsIt == m_playlistSongs.end()) {
        LOG_ERROR("Playlist with UUID {} not found", playlistId.toString().toStdString());
        return false;
    }
    
    auto& songs = songsIt.value();
    auto it = std::find_if(songs.begin(), songs.end(),
        [&song](const playlist::SongInfo& s) { return s == song; });
    
    if (it != songs.end()) {
        songs.erase(it);
        auto playlistIt = m_playlists.find(playlistId);
        locker.unlock();
        
        // Emit signals
        emit songRemoved(song, playlistId);
        emit playlistSongsChanged(playlistId);
        
        LOG_INFO("Removed song: {} from playlist: {} ({})",
                 song.title.toStdString(), 
                 (playlistIt != m_playlists.end()) ? playlistIt.value().name.toStdString() : "Unknown", 
                 playlistId.toString().toStdString());
        return true;
    }
    
    LOG_WARN("Song {} not found in playlist {}", song.title.toStdString(), playlistId.toString().toStdString());
    return false;
}

bool PlaylistManager::updateSongInPlaylist(const playlist::SongInfo& song, const QUuid& playlistId)
{
    if (!m_initialized) {
        LOG_ERROR("PlaylistManager not initialized");
        return false;
    }
    
    QWriteLocker locker(&m_dataLock);
    
    auto songsIt = m_playlistSongs.find(playlistId);
    if (songsIt == m_playlistSongs.end()) {
        LOG_ERROR("Playlist with UUID {} not found", playlistId.toString().toStdString());
        return false;
    }
    
    auto& songs = songsIt.value();
    auto it = std::find_if(songs.begin(), songs.end(),
        [&song](const playlist::SongInfo& s) { return s == song; });
    
    if (it != songs.end()) {
        *it = song;
        auto playlistIt = m_playlists.find(playlistId);
        locker.unlock();
        
        // Emit signals
        emit songUpdated(song, playlistId);
        emit playlistSongsChanged(playlistId);
        
        LOG_INFO("Updated song: {} in playlist: {} ({})",
                 song.title.toStdString(), 
                 (playlistIt != m_playlists.end()) ? playlistIt.value().name.toStdString() : "Unknown", 
                 playlistId.toString().toStdString());
        return true;
    }
    
    LOG_WARN("Song {} not found in playlist {}", song.title.toStdString(), playlistId.toString().toStdString());
    return false;
}

std::optional<playlist::SongInfo> PlaylistManager::getSongFromPlaylist(const QUuid& songId, const QUuid& playlistId) const
{
    QReadLocker locker(&m_dataLock);
    auto songsIt = m_playlistSongs.find(playlistId);
    if (songsIt != m_playlistSongs.end()) {
        // Since SongInfo doesn't have uuid, we'll need to find by index or other means
        // For now, return the first song if songId matches the index
        int index = 0;
        for (const auto& song : songsIt.value()) {
            if (QUuid::fromString(QString::number(index)) == songId) {
                return song;
            }
            index++;
        }
    }
    
    LOG_ERROR("Song {} not found in playlist {}", songId.toString().toStdString(), playlistId.toString().toStdString());
    return std::nullopt;
}

QList<playlist::SongInfo> PlaylistManager::iterateSongsInPlaylist(const QUuid& playlistId, std::function<bool(const playlist::SongInfo&)> func) const
{
    QReadLocker locker(&m_dataLock);
    QList<playlist::SongInfo> result;
    auto songsIt = m_playlistSongs.find(playlistId);
    if (songsIt != m_playlistSongs.end()) {
        for (const auto& song : songsIt.value()) {
            if (func(song)) {
                result.append(song);
            }
        }
    }
    return result;
}

// UUID-based lookup
bool PlaylistManager::categoryExists(const QUuid& categoryId) const
{
    QReadLocker locker(&m_dataLock);
    return m_categories.contains(categoryId);
}

bool PlaylistManager::playlistExists(const QUuid& playlistId) const
{
    QReadLocker locker(&m_dataLock);
    return m_playlists.contains(playlistId);
}

// Current playlist management
QUuid PlaylistManager::getCurrentPlaylist()
{
    if (!m_initialized) {
        LOG_WARN("PlaylistManager not initialized, cannot get current playlist");
        return QUuid();
    }
    QReadLocker locker(&m_dataLock);
    if (m_currentPlaylistId.isNull()) {
        locker.unlock();
        return ensureDefaultSetup();
    }
    return m_currentPlaylistId;
}

void PlaylistManager::setCurrentPlaylist(const QUuid& playlistId)
{
    if (!m_initialized) {
        LOG_ERROR("PlaylistManager not initialized");
        return;
    }
    
    if (!playlistId.isNull() && !playlistExists(playlistId)) {
        LOG_ERROR("Cannot set current playlist: playlist {} not found", playlistId.toString().toStdString());
        return;
    }
    
    QWriteLocker locker(&m_dataLock);
    m_currentPlaylistId = playlistId;
    locker.unlock();
    
    emit currentPlaylistChanged(playlistId);
    LOG_INFO("Current playlist changed to: {}", playlistId.toString().toStdString());
}

// Slots
void PlaylistManager::onConfigChanged()
{
    LOG_INFO("Config changed, reloading playlist manager settings");
    // Could reload settings or file paths here if needed
}

void PlaylistManager::onAutoSaveTimer()
{
    if (m_initialized) {
        LOG_DEBUG("Auto-save timer triggered, saving categories");
        saveAllCategories();
    }
}

// Helper methods
QUuid PlaylistManager::getPlaylistCategoryId(const QUuid& playlistId) const
{
    QReadLocker locker(&m_dataLock);
    for (auto it = m_categoryPlaylists.begin(); it != m_categoryPlaylists.end(); ++it) {
        if (it.value().contains(playlistId)) {
            return it.key();
        }
    }
    return QUuid(); // Not found
}

// Initialization helpers
void PlaylistManager::ensurePlaylistDirectoryExists()
{
    QString workspaceDir = m_configManager->getWorkspaceDirectory();
    if (workspaceDir.isEmpty()) {
        throw std::runtime_error("Workspace directory not configured");
    }
    
    QDir dir(workspaceDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            throw std::runtime_error("Failed to create workspace directory: " + workspaceDir.toStdString());
        }
        LOG_INFO("Created workspace directory: {}", workspaceDir.toStdString());
    }
}

void PlaylistManager::setupAutoSave()
{
    bool autoSaveEnabled = m_configManager->getAutoSaveEnabled();
    int autoSaveInterval = m_configManager->getAutoSaveInterval();
    
    if (autoSaveEnabled && autoSaveInterval > 0) {
        if (!m_autoSaveTimer) {
            m_autoSaveTimer = new QTimer(this);
            connect(m_autoSaveTimer, &QTimer::timeout, this, &PlaylistManager::onAutoSaveTimer);
        }
        
        m_autoSaveTimer->start(autoSaveInterval * 60 * 1000); // Convert minutes to milliseconds
        LOG_INFO("Auto-save enabled with {} minute interval", autoSaveInterval);
    } else {
        if (m_autoSaveTimer) {
            m_autoSaveTimer->stop();
        }
        LOG_INFO("Auto-save disabled");
    }
}

bool PlaylistManager::saveCategoriesToJsonFile()
{
    QString categoryFilePath = m_configManager->getCategoriesFilePath();
    QString workspaceDir = m_configManager->getWorkspaceDirectory();
    QString fullPath = QDir(workspaceDir).absoluteFilePath(categoryFilePath);
    
    QJsonDocument doc;
    QJsonArray categoriesArray;
    
    // Convert hashmap structure to JSON (with read lock)
    QReadLocker locker(&m_dataLock);
    for (auto catIt = m_categories.begin(); catIt != m_categories.end(); ++catIt) {
        const playlist::CategoryInfo& category = catIt.value();
        QJsonObject categoryObj;
        categoryObj["uuid"] = category.uuid.toString();
        categoryObj["name"] = category.name;
        
        // Get playlists for this category
        QJsonArray playlistsArray;
        auto playlistIds = m_categoryPlaylists.value(category.uuid);
        for (const QUuid& playlistId : playlistIds) {
            auto playlistIt = m_playlists.find(playlistId);
            if (playlistIt != m_playlists.end()) {
                const playlist::PlaylistInfo& playlist = playlistIt.value();
                QJsonObject playlistObj;
                playlistObj["uuid"] = playlist.uuid.toString();
                playlistObj["name"] = playlist.name;
                playlistObj["creator"] = playlist.creator;
                playlistObj["description"] = playlist.description;
                playlistObj["coverUrl"] = playlist.coverUrl;
                
                // Get songs for this playlist
                QJsonArray songsArray;
                auto songs = m_playlistSongs.value(playlistId);
                for (const playlist::SongInfo& song : songs) {
                    QJsonObject songObj;
                    songObj["title"] = song.title;
                    songObj["uploader"] = song.uploader;
                    songObj["platform"] = song.platform;
                    songObj["duration"] = song.duration;
                    songObj["filepath"] = song.filepath;
                    songObj["args"] = song.args;
                    songsArray.append(songObj);
                }
                playlistObj["songs"] = songsArray;
                playlistsArray.append(playlistObj);
            }
        }
        categoryObj["playlists"] = playlistsArray;
        categoriesArray.append(categoryObj);
    }
    
    QJsonObject rootObj;
    rootObj["categories"] = categoriesArray;
    rootObj["currentPlaylistId"] = m_currentPlaylistId.toString();
    locker.unlock(); // Unlock before file I/O
    
    doc.setObject(rootObj);
    
    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR("Failed to open file for writing: {}", fullPath.toStdString());
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    LOG_INFO("Categories saved to: {}", fullPath.toStdString());
    return true;
}

bool PlaylistManager::loadCategoriesFromJsonFile()
{
    QString categoryFilePath = m_configManager->getCategoriesFilePath();
    QString workspaceDir = m_configManager->getWorkspaceDirectory();
    QString fullPath = QDir(workspaceDir).absoluteFilePath(categoryFilePath);
    
    QFile file(fullPath);
    if (!file.exists()) {
        LOG_INFO("Category file does not exist: {}, will create default setup", fullPath.toStdString());
        ensureDefaultSetup();
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR("Failed to open category file: {}", fullPath.toStdString());
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR("JSON parse error in category file: {}", parseError.errorString().toStdString());
        return false;
    }
    
    {
        QWriteLocker locker(&m_dataLock);
        
        // Clear existing data
        m_categories.clear();
        m_playlists.clear();
        m_categoryPlaylists.clear();
        m_playlistSongs.clear();
    
        QJsonObject rootObj = doc.object();
        QJsonArray categoriesArray = rootObj["categories"].toArray();
        
        // Load categories and playlists
        for (const QJsonValue& categoryValue : categoriesArray) {
            QJsonObject categoryObj = categoryValue.toObject();
            
            playlist::CategoryInfo category;
            category.uuid = QUuid::fromString(categoryObj["uuid"].toString());
            category.name = categoryObj["name"].toString();
            
            // Add category
            m_categories.insert(category.uuid, category);
            m_categoryPlaylists.insert(category.uuid, QList<QUuid>());
            
            // Load playlists for this category
            QJsonArray playlistsArray = categoryObj["playlists"].toArray();
            for (const QJsonValue& playlistValue : playlistsArray) {
                QJsonObject playlistObj = playlistValue.toObject();
                
                playlist::PlaylistInfo playlist;
                playlist.uuid = QUuid::fromString(playlistObj["uuid"].toString());
                playlist.name = playlistObj["name"].toString();
                playlist.creator = playlistObj["creator"].toString();
                playlist.description = playlistObj["description"].toString();
                playlist.coverUrl = playlistObj["coverUrl"].toString();
                
                // Add playlist
                m_playlists.insert(playlist.uuid, playlist);
                m_categoryPlaylists[category.uuid].append(playlist.uuid);
                
                // Load songs for this playlist
                QList<playlist::SongInfo> songs;
                QJsonArray songsArray = playlistObj["songs"].toArray();
                for (const QJsonValue& songValue : songsArray) {
                    QJsonObject songObj = songValue.toObject();
                    
                    playlist::SongInfo song;
                    song.title = songObj["title"].toString();
                    song.uploader = songObj["uploader"].toString();
                    song.platform = songObj["platform"].toInt();
                    song.duration = songObj["duration"].toString();
                    song.filepath = songObj["filepath"].toString();
                    song.args = songObj["args"].toString();
                    
                    songs.append(song);
                }
                m_playlistSongs.insert(playlist.uuid, songs);
            }
        }
    
        // Load current playlist ID
        QString currentPlaylistIdStr = rootObj["currentPlaylistId"].toString();
        m_currentPlaylistId = QUuid::fromString(currentPlaylistIdStr);
    }
    
    LOG_INFO("Loaded {} categories with {} playlists from {}", 
             m_categories.size(), m_playlists.size(), fullPath.toStdString());
    return true;
}

// Default setup helpers
QUuid PlaylistManager::ensureDefaultSetup()
{
    QUuid categoryUuid;
    QWriteLocker locker(&m_dataLock);
    if (m_categories.empty()) {
        playlist::CategoryInfo defaultCategory;
        defaultCategory.name = "Default Category";
        defaultCategory.uuid = QUuid::createUuid();
        
        m_categories.insert(defaultCategory.uuid, defaultCategory);
        m_categoryPlaylists.insert(defaultCategory.uuid, QList<QUuid>());
        LOG_INFO("Created default category: {} ({})", 
                 defaultCategory.name.toStdString(), defaultCategory.uuid.toString().toStdString());
    }
    else {
        categoryUuid = m_categories.begin().key();
    }
    
    if (m_categoryPlaylists.value(categoryUuid).isEmpty()) {
        playlist::PlaylistInfo favoritePlaylist;
        favoritePlaylist.name = "Favorite Playlist";
        favoritePlaylist.creator = "System";
        favoritePlaylist.description = "Your favorite songs collection";
        favoritePlaylist.coverUrl = "";
        favoritePlaylist.uuid = QUuid::createUuid();
        
        m_playlists.insert(favoritePlaylist.uuid, favoritePlaylist);
        m_playlistSongs.insert(favoritePlaylist.uuid, QList<playlist::SongInfo>());
        m_categoryPlaylists[categoryUuid].append(favoritePlaylist.uuid);

        emit playlistAdded(favoritePlaylist, categoryUuid);
        LOG_INFO("Created favorite playlist: {} ({}) in category: {}", 
             favoritePlaylist.name.toStdString(), favoritePlaylist.uuid.toString().toStdString(), categoryUuid.toString().toStdString());
    } else {
        QUuid firstPlaylistId = m_categoryPlaylists.value(categoryUuid).first();
        m_currentPlaylistId = firstPlaylistId;
        LOG_INFO("Default setup exists with category: {} and playlist: {}", 
                 categoryUuid.toString().toStdString(), firstPlaylistId.toString().toStdString());
    }
    return m_currentPlaylistId;
}