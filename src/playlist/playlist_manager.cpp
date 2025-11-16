#include "playlist_manager.h"
#include <config/config_manager.h>
#include <log/log_manager.h>
#include <json/json.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>
#include <optional>
#include <functional>
#include <fstream>
#include <sstream>
#include <fmt/format.h>
#include <util/md5.h>

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
        
        m_initialized = true;
        LOG_INFO("PlaylistManager initialization complete");
        
    } catch (const std::exception& e) {
        LOG_ERROR("PlaylistManager initialization failed: {}", e.what());
        throw;
    }
}

bool PlaylistManager::loadCategoriesFromFile()
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
        return true;
    }
    
    // No file found, ensure default setup exists
    m_currentPlaylistId = QUuid();
    emit categoriesLoaded(0, 0);
    return false;
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
    playlist::SongInfo songWithFilepath = song;
    songWithFilepath.filepath = generateStreamingFilepath(songWithFilepath);
    
    m_playlistSongs[playlistId].append(songWithFilepath);
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
        if (it->filepath.isEmpty()) {
            it->filepath = generateStreamingFilepath(*it);
        }
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
    
    Json::Value root(Json::objectValue);
    Json::Value categoriesArray(Json::arrayValue);
    
    // Convert hashmap structure to JSON (with read lock)
    QReadLocker locker(&m_dataLock);
    for (auto catIt = m_categories.begin(); catIt != m_categories.end(); ++catIt) {
        const playlist::CategoryInfo& category = catIt.value();
        Json::Value categoryObj(Json::objectValue);
        categoryObj["uuid"] = category.uuid.toString().toStdString();
        categoryObj["name"] = category.name.toStdString();
        
        // Get playlists for this category
        Json::Value playlistsArray(Json::arrayValue);
        auto playlistIds = m_categoryPlaylists.value(category.uuid);
        for (const QUuid& playlistId : playlistIds) {
            auto playlistIt = m_playlists.find(playlistId);
            if (playlistIt != m_playlists.end()) {
                const playlist::PlaylistInfo& playlist = playlistIt.value();
                Json::Value playlistObj(Json::objectValue);
                playlistObj["uuid"] = playlist.uuid.toString().toStdString();
                playlistObj["name"] = playlist.name.toStdString();
                playlistObj["creator"] = playlist.creator.toStdString();
                playlistObj["description"] = playlist.description.toStdString();
                playlistObj["coverUri"] = playlist.coverUri.toStdString();
                
                // Get songs for this playlist
                Json::Value songsArray(Json::arrayValue);
                auto songs = m_playlistSongs.value(playlistId);
                for (const playlist::SongInfo& song : songs) {
                    Json::Value songObj(Json::objectValue);
                    songObj["title"] = song.title.toStdString();
                    songObj["uploader"] = song.uploader.toStdString();
                    songObj["platform"] = song.platform;
                    songObj["duration"] = song.duration;
                    songObj["filepath"] = song.filepath.toStdString();
                    songObj["coverName"] = song.coverName.toStdString();
                    songObj["args"] = song.args.toStdString();
                    songsArray.append(songObj);
                }
                playlistObj["songs"] = songsArray;
                playlistsArray.append(playlistObj);
            }
        }
        categoryObj["playlists"] = playlistsArray;
        categoriesArray.append(categoryObj);
    }
    
    root["categories"] = categoriesArray;
    root["currentPlaylistId"] = m_currentPlaylistId.toString().toStdString();
    locker.unlock(); // Unlock before file I/O
    
    // Write to file
    std::ofstream file(fullPath.toStdString());
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for writing: {}", fullPath.toStdString());
        return false;
    }
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &file);
    file.close();
    
    LOG_INFO("Categories saved to: {}", fullPath.toStdString());
    return true;
}

bool PlaylistManager::loadCategoriesFromJsonFile()
{
    QString categoryFilePath = m_configManager->getCategoriesFilePath();
    QString workspaceDir = m_configManager->getWorkspaceDirectory();
    QString fullPath = QDir(workspaceDir).absoluteFilePath(categoryFilePath);
    
    QFileInfo fileInfo(fullPath);
    if (!fileInfo.exists()) {
        LOG_INFO("Category file does not exist: {}, will create default setup", fullPath.toStdString());
        ensureDefaultSetup();
        return false;
    }
    
    std::ifstream file(fullPath.toStdString());
    if (!file.is_open()) {
        LOG_ERROR("Failed to open category file: {}", fullPath.toStdString());
        return false;
    }
    
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, file, &root, &errs)) {
        LOG_ERROR("JSON parse error in category file: {}", errs);
        file.close();
        return false;
    }
    file.close();
    
    {
        QWriteLocker locker(&m_dataLock);
        
        // Clear existing data
        m_categories.clear();
        m_playlists.clear();
        m_categoryPlaylists.clear();
        m_playlistSongs.clear();
    
        const Json::Value& categoriesArray = root["categories"];
        
        // Load categories and playlists
        for (const Json::Value& categoryValue : categoriesArray) {
            playlist::CategoryInfo category;
            category.uuid = QUuid::fromString(QString::fromStdString(categoryValue["uuid"].asString()));
            category.name = QString::fromStdString(categoryValue["name"].asString());
            
            // Add category
            m_categories.insert(category.uuid, category);
            m_categoryPlaylists.insert(category.uuid, QList<QUuid>());
            
            // Load playlists for this category
            const Json::Value& playlistsArray = categoryValue["playlists"];
            for (const Json::Value& playlistValue : playlistsArray) {
                playlist::PlaylistInfo playlist;
                playlist.uuid = QUuid::fromString(QString::fromStdString(playlistValue["uuid"].asString()));
                playlist.name = QString::fromStdString(playlistValue["name"].asString());
                playlist.creator = QString::fromStdString(playlistValue["creator"].asString());
                playlist.description = QString::fromStdString(playlistValue["description"].asString());
                playlist.coverUri = QString::fromStdString(playlistValue["coverUri"].asString());
                
                // Add playlist
                m_playlists.insert(playlist.uuid, playlist);
                m_categoryPlaylists[category.uuid].append(playlist.uuid);
                
                // Load songs for this playlist
                QList<playlist::SongInfo> songs;
                const Json::Value& songsArray = playlistValue["songs"];
                for (const Json::Value& songValue : songsArray) {
                    playlist::SongInfo song;
                    song.title = QString::fromStdString(songValue["title"].asString());
                    song.uploader = QString::fromStdString(songValue["uploader"].asString());
                    song.platform = songValue["platform"].asInt();
                    song.duration = songValue["duration"].asInt();
                    song.filepath = QString::fromStdString(songValue["filepath"].asString());
                    song.coverName = QString::fromStdString(songValue["coverName"].asString());
                    song.args = QString::fromStdString(songValue["args"].asString());
                    
                    songs.append(song);
                }
                m_playlistSongs.insert(playlist.uuid, songs);
            }
        }
    
        // Load current playlist ID
        QString currentPlaylistIdStr = QString::fromStdString(root["currentPlaylistId"].asString());
        m_currentPlaylistId = QUuid::fromString(currentPlaylistIdStr);
    }
    
    LOG_INFO("Loaded {} categories with {} playlists from {}", 
             m_categories.size(), m_playlists.size(), fullPath.toStdString());
    return true;
}

QString PlaylistManager::generateStreamingFilepath(const playlist::SongInfo& song) const
{
    // Generate filepath using only platform and args hash for safety and uniqueness
    // song.platform is not an enum type directly; cast it to the actual enum used by NetworkManager
    QString platformStr = QString::number(static_cast<int>(song.platform));
    
    // Create a comprehensive hash from multiple song properties for uniqueness
    std::string titleHash = util::md5Hash(song.title.toStdString()),
                uploaderHash = util::md5Hash(song.uploader.toStdString()),
                argsHash = util::md5Hash(song.args.toStdString());
    std::string mixedStr = fmt::format("{}{}{}", titleHash, uploaderHash, argsHash);
    std::string result = util::md5Hash(mixedStr);
    
    // Safe filename format: "platform_hash.audio"
    QString filename = QString("%1_%2.audio").arg(platformStr).arg(QString::fromStdString(result));
    
    // Get audio cache directory from ConfigManager
    QString cacheDir = m_configManager->getAudioCacheDirectory();
    if (cacheDir.isEmpty()) {
        cacheDir = QDir::cleanPath(QDir::currentPath() + "/tmp");
    }

    // Ensure directory exists
    QDir().mkpath(cacheDir);

    return QDir(cacheDir).filePath(filename);
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
        categoryUuid = defaultCategory.uuid;
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
        favoritePlaylist.coverUri = "";
        favoritePlaylist.uuid = QUuid::createUuid();
        
        m_playlists.insert(favoritePlaylist.uuid, favoritePlaylist);
        m_playlistSongs.insert(favoritePlaylist.uuid, QList<playlist::SongInfo>());
        m_categoryPlaylists[categoryUuid].append(favoritePlaylist.uuid);
        m_currentPlaylistId = favoritePlaylist.uuid;
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