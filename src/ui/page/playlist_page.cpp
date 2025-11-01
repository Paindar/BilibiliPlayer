#include "playlist_page.h"
#include "ui_playlist_page.h"
#include "../../playlist/playlist_manager.h"
#include "../../log/log_manager.h"
#include <QHeaderView>
#include <QPixmap>
#include <QUrl>
#include <QUrlQuery>
#include <QTime>
#include <manager/application_context.h>

const QString PlaylistPage::PLAYLIST_PAGE_TYPE = "playlist";

PlaylistPage::PlaylistPage(QWidget *parent)
    : QWidget(parent)
    , INavigablePage(PLAYLIST_PAGE_TYPE)
    , ui(new Ui_PlaylistPageForm)
    , m_playlistManager(nullptr)
{
    ui->setupUi(this);
    setupUI();
    
    // Connect signals
    connect(ui->songListView, &QTreeWidget::itemDoubleClicked, 
            this, &PlaylistPage::onSongDoubleClicked);
    connect(ui->songListView, &QTreeWidget::itemSelectionChanged,
            this, &PlaylistPage::onSelectionChanged);

    SetupPlaylistManager();
}

PlaylistPage::~PlaylistPage()
{
    delete ui;
}

void PlaylistPage::setupUI()
{
    // Configure the tree widget
    ui->songListView->setHeaderLabels({"Title", "Uploader/Artist", "Platform", "Duration"});
    
    // Set column widths
    QHeaderView* header = ui->songListView->header();
    header->setStretchLastSection(false);
    header->resizeSection(0, 300);  // Title
    header->resizeSection(1, 200);  // Uploader/Artist
    header->resizeSection(2, 120);  // Platform
    header->resizeSection(3, 80);   // Duration
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    
    // Set default UI state
    ui->playlistNameLabel->setText("No Playlist Selected");
    ui->creatorLabel->setText("");
    ui->songCountLabel->setText("0 songs");
    ui->descriptionLabel->setText("");
    ui->totalDurationLabel->setText("Total: 00:00");
    ui->coverLabel->setText("No Cover");
}

void PlaylistPage::SetupPlaylistManager()
{
    m_playlistManager = PLAYLIST_MANAGER;
    
    if (m_playlistManager) {
        // Connect to playlist manager signals for real-time updates
        connect(m_playlistManager, &PlaylistManager::playlistUpdated,
                this, [this](const playlist::PlaylistInfo& info, const QUuid& categoryId) {
                    Q_UNUSED(categoryId)
                    if (info.uuid == m_currentPlaylistId) {
                        LOG_DEBUG("Playlist updated, refreshing UI");
                        refreshPlaylist();
                    }
                });
        
        // Connect to song-related signals
        connect(m_playlistManager, &PlaylistManager::playlistSongsChanged,
                this, [this](const QUuid& playlistId) {
                    if (playlistId == m_currentPlaylistId) {
                        LOG_DEBUG("Songs changed in current playlist, refreshing UI");
                        refreshPlaylist();
                    }
                });
                
        connect(m_playlistManager, &PlaylistManager::songAdded,
                this, [this](const playlist::SongInfo& song, const QUuid& playlistId) {
                    Q_UNUSED(song)
                    if (playlistId == m_currentPlaylistId) {
                        LOG_DEBUG("Song added to current playlist, refreshing UI");
                        refreshPlaylist();
                    }
                });
                
        connect(m_playlistManager, &PlaylistManager::songRemoved,
                this, [this](const playlist::SongInfo& song, const QUuid& playlistId) {
                    Q_UNUSED(song)
                    if (playlistId == m_currentPlaylistId) {
                        LOG_DEBUG("Song removed from current playlist, refreshing UI");
                        refreshPlaylist();
                    }
                });
    }
}

void PlaylistPage::setCurrentPlaylist(const QUuid& playlistId)
{
    LOG_DEBUG("Setting current playlist to: {}", playlistId.toString().toStdString());
    m_currentPlaylistId = playlistId;
    refreshPlaylist();
}

void PlaylistPage::refreshPlaylist()
{
    LOG_DEBUG("Refreshing playlist UI");
    
    if (!m_playlistManager) {
        LOG_DEBUG("No playlist manager available");
        // Clear UI when no playlist manager is available
        ui->playlistNameLabel->setText("No Playlist Manager");
        ui->creatorLabel->setText("");
        ui->songCountLabel->setText("0 songs");
        ui->descriptionLabel->setText("");
        ui->totalDurationLabel->setText("Total: 00:00");
        ui->coverLabel->setText("No Cover");
        ui->songListView->clear();
        return;
    }
    
    if (m_currentPlaylistId.isNull()) {
        LOG_DEBUG("No current playlist selected");
        // Clear UI when no playlist is selected
        ui->playlistNameLabel->setText("No Playlist Selected");
        ui->creatorLabel->setText("");
        ui->songCountLabel->setText("0 songs");
        ui->descriptionLabel->setText("");
        ui->totalDurationLabel->setText("Total: 00:00");
        ui->coverLabel->setText("No Cover");
        ui->songListView->clear();
        return;
    }
    
    LOG_DEBUG("Updating playlist info and song list for playlist: {}", m_currentPlaylistId.toString().toStdString());
    updatePlaylistInfo();
    updateSongList();
}

void PlaylistPage::addSong(const playlist::SongInfo& song)
{
    if (!m_playlistManager || m_currentPlaylistId.isNull()) {
        LOG_ERROR("Cannot add song: no playlist manager or current playlist");
        return;
    }
    
    if (m_playlistManager->addSongToPlaylist(song, m_currentPlaylistId)) {
        // The playlist will be refreshed via signal connection
        emit playlistModified();
        LOG_DEBUG("Added song '{}' to playlist", song.title.toStdString());
    } else {
        LOG_ERROR("Failed to add song '{}' to playlist", song.title.toStdString());
    }
}

void PlaylistPage::removeSong(int index)
{
    if (!m_playlistManager || m_currentPlaylistId.isNull()) {
        LOG_ERROR("Cannot remove song: no playlist manager or current playlist");
        return;
    }
    
    // Get the song at the specified index
    QTreeWidgetItem* item = ui->songListView->topLevelItem(index);
    if (!item) {
        LOG_ERROR("Invalid song index: {}", index);
        return;
    }
    
    playlist::SongInfo song = item->data(0, Qt::UserRole).value<playlist::SongInfo>();
    
    if (m_playlistManager->removeSongFromPlaylist(song, m_currentPlaylistId)) {
        // The playlist will be refreshed via signal connection
        emit playlistModified();
        LOG_DEBUG("Removed song '{}' from playlist", song.title.toStdString());
    } else {
        LOG_ERROR("Failed to remove song '{}' from playlist", song.title.toStdString());
    }
}

void PlaylistPage::clearSongs()
{
    if (!m_playlistManager || m_currentPlaylistId.isNull()) {
        LOG_ERROR("Cannot clear songs: no playlist manager or current playlist");
        return;
    }
    
    // Get all songs and remove them one by one
    auto songs = m_playlistManager->iterateSongsInPlaylist(m_currentPlaylistId, 
                                                          [](const playlist::SongInfo&) { return true; });
    
    for (const auto& song : songs) {
        m_playlistManager->removeSongFromPlaylist(song, m_currentPlaylistId);
    }
    
    emit playlistModified();
    LOG_DEBUG("Cleared all songs from playlist");
}

void PlaylistPage::updatePlaylistInfo()
{
    if (!m_playlistManager || m_currentPlaylistId.isNull()) {
        return;
    }
    
    // Get playlist info from manager
    auto playlistInfo = m_playlistManager->getPlaylist(m_currentPlaylistId);
    if (!playlistInfo.has_value()) {
        LOG_ERROR("Failed to get playlist info for ID: {}", m_currentPlaylistId.toString().toStdString());
        return;
    }
    
    // Get songs for this playlist
    auto songs = m_playlistManager->iterateSongsInPlaylist(m_currentPlaylistId, 
                                                          [](const playlist::SongInfo&) { return true; });
    
    // Update playlist name and info
    ui->playlistNameLabel->setText(playlistInfo->name);
    ui->creatorLabel->setText(QString("Created by: %1").arg(playlistInfo->creator));
    ui->songCountLabel->setText(QString("%1 songs").arg(songs.size()));
    ui->descriptionLabel->setText(playlistInfo->description);
    
    // Calculate total duration
    int totalSeconds = 0;
    for (const auto& song : songs) {
        // Parse duration string (assuming format like "3:45" or "1:23:45")
        QStringList parts = song.duration.split(':');
        if (parts.size() == 2) {
            // MM:SS format
            totalSeconds += parts[0].toInt() * 60 + parts[1].toInt();
        } else if (parts.size() == 3) {
            // HH:MM:SS format
            totalSeconds += parts[0].toInt() * 3600 + parts[1].toInt() * 60 + parts[2].toInt();
        }
    }
    
    ui->totalDurationLabel->setText(QString("Total: %1").arg(formatDuration(totalSeconds)));
    
    // Set cover image (placeholder for now)
    if (!playlistInfo->coverUrl.isEmpty()) {
        // In a real implementation, you would load the image asynchronously
        ui->coverLabel->setText("Loading...");
    } else {
        ui->coverLabel->setText("No Cover");
    }
}

void PlaylistPage::updateSongList()
{
    ui->songListView->clear();
    
    if (!m_playlistManager || m_currentPlaylistId.isNull()) {
        LOG_DEBUG("Cannot update song list: missing manager or playlist ID");
        return;
    }
    
    // Get songs from playlist manager
    auto songs = m_playlistManager->iterateSongsInPlaylist(m_currentPlaylistId, 
                                                          [](const playlist::SongInfo&) { return true; });
    
    LOG_DEBUG("Retrieved {} songs for playlist {}", songs.size(), m_currentPlaylistId.toString().toStdString());
    
    for (const auto& song : songs) {
        LOG_DEBUG("Adding song to UI: {}", song.title.toStdString());
        QTreeWidgetItem* item = createSongItem(song);
        ui->songListView->addTopLevelItem(item);
    }
    
    LOG_DEBUG("Song list updated, total items in UI: {}", ui->songListView->topLevelItemCount());
}

QTreeWidgetItem* PlaylistPage::createSongItem(const playlist::SongInfo& song)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, song.title);
    item->setText(1, song.uploader);
    
    // Convert platform enum to string - you may need to implement this conversion
    QString platformStr;
    switch (song.platform) {
        case 1: platformStr = "Bilibili"; break;
        // Add other platforms as needed
        default: platformStr = "Unknown"; break;
    }
    item->setText(2, platformStr);
    item->setText(3, song.duration);
    
    // Store song data for retrieval
    item->setData(0, Qt::UserRole, QVariant::fromValue(song));
    
    return item;
}

QString PlaylistPage::formatDuration(int seconds) const
{
    if (seconds < 3600) {
        // Less than an hour: MM:SS
        return QTime(0, 0, 0).addSecs(seconds).toString("mm:ss");
    } else {
        // Hour or more: HH:MM:SS
        return QTime(0, 0, 0).addSecs(seconds).toString("hh:mm:ss");
    }
}

void PlaylistPage::onSongDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    
    if (item) {
        playlist::SongInfo song = item->data(0, Qt::UserRole).value<playlist::SongInfo>();
        emit songDoubleClicked(song);
    }
}

void PlaylistPage::onSelectionChanged()
{
    QList<QTreeWidgetItem*> selectedItems = ui->songListView->selectedItems();
    QList<playlist::SongInfo> selectedSongs;
    
    for (QTreeWidgetItem* item : selectedItems) {
        playlist::SongInfo song = item->data(0, Qt::UserRole).value<playlist::SongInfo>();
        selectedSongs.append(song);
    }
    
    emit songsSelected(selectedSongs);
}

QString PlaylistPage::getNavigationState() const
{
    QUrlQuery query;
    query.addQueryItem("playlistId", m_currentPlaylistId.toString());
    return QString("%1?%2").arg(PLAYLIST_PAGE_TYPE).arg(query.toString());
}

void PlaylistPage::restoreFromState(const QString& state)
{
    QUrl url(state);
    QUrlQuery query(url.query());
    
    QString playlistIdStr = query.queryItemValue("playlistId");
    if (!playlistIdStr.isEmpty()) {
        QUuid playlistId = QUuid::fromString(playlistIdStr);
        if (!playlistId.isNull()) {
            setCurrentPlaylist(playlistId);
        }
    }
}