#include "playlist_page.h"
#include "ui_playlist_page.h"
#include <playlist/playlist_manager.h>
#include <network/platform/i_platform.h>
#include <log/log_manager.h>
#include <config/config_manager.h>
#include <ui/util/elided_label.h>
#include <QHeaderView>
#include <QPixmap>
#include <QUrl>
#include <QUrlQuery>
#include <QTime>
#include <QMenu>
#include <QAction>
#include <QEvent>
#include <QTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QPropertyAnimation>
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
    
    // Enable context menu
    ui->songListView->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Connect signals
    connect(ui->songListView, &QTreeWidget::itemDoubleClicked, 
            this, &PlaylistPage::onSongDoubleClicked);
    connect(ui->songListView, &QTreeWidget::itemSelectionChanged,
            this, &PlaylistPage::onSelectionChanged);
    connect(ui->songListView, &QTreeWidget::customContextMenuRequested,
            this, &PlaylistPage::showContextMenu);

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
    
    // Configure header for interactive resizing
    QHeaderView* header = ui->songListView->header();
    header->setStretchLastSection(false);
    header->setSectionsMovable(false);  // Prevent column reordering
    header->setSectionResizeMode(QHeaderView::Interactive);  // Enable manual resizing
    
    // Load saved column widths or use defaults
    loadColumnWidths();
    
    // Save column widths when user resizes them
    connect(header, &QHeaderView::sectionResized,
            this, &PlaylistPage::saveColumnWidths);
    
    // Set default UI state
    ui->playlistNameLabel->setText(tr("No Playlist Selected"));
    ui->creatorLabel->setText("");
    ui->songCountLabel->setText(tr("0 songs"));
    ui->descriptionLabel->setText("");
    ui->coverLabel->setText(tr("No Cover"));

    // Connect Add Local Files button (added to .ui)
    if (ui->addLocalFilesButton) {
        connect(ui->addLocalFilesButton, &QPushButton::clicked,
                this, &PlaylistPage::onAddLocalFilesClicked);
    }
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
        ui->playlistNameLabel->setText(tr("No Playlist Manager"));
        ui->creatorLabel->setText("");
        ui->songCountLabel->setText(tr("0 songs"));
        ui->descriptionLabel->setText("");
        ui->coverLabel->setText(tr("No Cover"));
        ui->songListView->clear();
        return;
    }
    
    if (m_currentPlaylistId.isNull()) {
        LOG_DEBUG("No current playlist selected");
        // Clear UI when no playlist is selected
        ui->playlistNameLabel->setText(tr("No Playlist Selected"));
        ui->creatorLabel->setText("");
        ui->songCountLabel->setText(tr("0 songs"));
        ui->descriptionLabel->setText("");
        ui->coverLabel->setText(tr("No Cover"));
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
    ui->creatorLabel->setText(QString(tr("Created by: %1")).arg(playlistInfo->creator));
    ui->songCountLabel->setText(QString(tr("%1 songs")).arg(songs.size()));
    ui->descriptionLabel->setText(playlistInfo->description);
    
    // Set cover image (placeholder for now)
    if (!playlistInfo->coverUri.isEmpty()) {
        // In a real implementation, you would load the image asynchronously
        if (QFile::exists(playlistInfo->coverUri)) {
            QPixmap pix(playlistInfo->coverUri);
            LOG_DEBUG("Loading cover image from local path: {}", playlistInfo->coverUri.toStdString());
            if (!pix.isNull()) 
                ui->coverLabel->setPixmap(pix.scaled(150,150, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            ui->coverLabel->setText(tr("Loading..."));
            m_currentCoverPath = playlistInfo->coverUri;
            LOG_DEBUG("Requesting cover image download from {}", playlistInfo->coverUri.toStdString());
        }
    } else {
        ui->coverLabel->setText(tr("No Cover"));
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
        // Create your custom QLabel
        ScrollingLabel* titleLabel = new ScrollingLabel(this);
        titleLabel->setText(song.title);

        // Replace column 0's text with this label
        ui->songListView->setItemWidget(item, 0, titleLabel);
    }
    
    LOG_DEBUG("Song list updated, total items in UI: {}", ui->songListView->topLevelItemCount());
}

QTreeWidgetItem* PlaylistPage::createSongItem(const playlist::SongInfo& song)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();
    // item->setText(0, song.title);
    item->setText(1, song.uploader);
    
    // Convert platform enum to string using i18n-enabled function
    QString platformStr = network::getPlatformName(static_cast<network::PlatformType>(song.platform));
    item->setText(2, platformStr);
    item->setText(3, formatDuration(song.duration/1000)); // Convert ms to seconds
    
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

void PlaylistPage::onNetworkDownload(QString url, QString savePath)
{
    if (savePath == m_currentCoverPath) {
        if (ui->coverLabel) {
            QPixmap pix(savePath);
            LOG_DEBUG("Cover image downloaded, updating UI from {}", savePath.toStdString());
            if (!pix.isNull()) 
                ui->coverLabel->setPixmap(pix.scaled(150,150, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
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

void PlaylistPage::showContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = ui->songListView->itemAt(pos);
    if (!item) {
        return; // No item at this position
    }
    
    QMenu contextMenu(this);
    
    QAction* deleteAction = contextMenu.addAction("Delete");
    connect(deleteAction, &QAction::triggered, this, &PlaylistPage::deleteSong);
    
    contextMenu.exec(ui->songListView->mapToGlobal(pos));
}

void PlaylistPage::deleteSong()
{
    QList<QTreeWidgetItem*> selectedItems = ui->songListView->selectedItems();
    if (selectedItems.isEmpty()) {
        LOG_WARN("No song selected for deletion");
        return;
    }
    
    if (!m_playlistManager || m_currentPlaylistId.isNull()) {
        LOG_ERROR("Cannot delete song: no playlist manager or current playlist");
        return;
    }
    
    // Delete all selected songs
    for (QTreeWidgetItem* item : selectedItems) {
        playlist::SongInfo song = item->data(0, Qt::UserRole).value<playlist::SongInfo>();
        
        if (m_playlistManager->removeSongFromPlaylist(song, m_currentPlaylistId)) {
            LOG_DEBUG("Deleted song '{}' from playlist", song.title.toStdString());
        } else {
            LOG_ERROR("Failed to delete song '{}' from playlist", song.title.toStdString());
        }
    }
}

void PlaylistPage::loadColumnWidths()
{
    auto configManager = CONFIG_MANAGER;
    if (!configManager) {
        LOG_WARN("ConfigManager not available, using default column widths");
        resetColumnWidths();
        return;
    }

    QHeaderView* header = ui->songListView->header();
    
    // Load column widths from config (with minimum width sanity check)
    int titleWidth = configManager->getPlaylistTitleWidth();
    int uploaderWidth = configManager->getPlaylistUploaderWidth();
    int platformWidth = configManager->getPlaylistPlatformWidth();
    int durationWidth = configManager->getPlaylistDurationWidth();
    
    // Apply widths with minimum check
    header->resizeSection(0, titleWidth > 20 ? titleWidth : 300);
    header->resizeSection(1, uploaderWidth > 20 ? uploaderWidth : 200);
    header->resizeSection(2, platformWidth > 20 ? platformWidth : 120);
    header->resizeSection(3, durationWidth > 20 ? durationWidth : 80);
    
    LOG_DEBUG("Loaded column widths from config");
}

void PlaylistPage::saveColumnWidths()
{
    auto configManager = CONFIG_MANAGER;
    if (!configManager) {
        return;
    }

    QHeaderView* header = ui->songListView->header();
    
    // Save each column width using specific ConfigManager methods
    configManager->setPlaylistTitleWidth(header->sectionSize(0));
    configManager->setPlaylistUploaderWidth(header->sectionSize(1));
    configManager->setPlaylistPlatformWidth(header->sectionSize(2));
    configManager->setPlaylistDurationWidth(header->sectionSize(3));
    
    configManager->saveToFile();
    LOG_DEBUG("Saved column widths to config");
}

void PlaylistPage::resetColumnWidths()
{
    QHeaderView* header = ui->songListView->header();
    
    // Default widths: Title (40%), Uploader (25%), Platform (20%), Duration (15%)
    // Assuming ~750px total width
    header->resizeSection(0, 300);  // Title
    header->resizeSection(1, 200);  // Uploader/Artist
    header->resizeSection(2, 120);  // Platform
    header->resizeSection(3, 80);   // Duration
    
    LOG_DEBUG("Reset column widths to defaults");
}

void PlaylistPage::onAddLocalFilesClicked()
{
    if (!m_playlistManager || m_currentPlaylistId.isNull()) {
        LOG_WARN("Cannot add local files: no playlist manager or current playlist");
        return;
    }

    QStringList paths = QFileDialog::getOpenFileNames(this,
                                                     tr("Select Local Media"),
                                                     QDir::homePath(),
                                                     tr("Audio Files (*.mp3 *.wav *.flac);;All Files (*)"));
    if (paths.isEmpty()) return;

    for (const QString& path : paths) {
        playlist::SongInfo song;
        song.title = QFileInfo(path).baseName();
        song.uploader = QString();
        song.platform = 0; // Local platform
        song.duration = 0;
        song.filepath = path; // PlaylistManager will convert to file:// if necessary
        song.coverName = QString();
        song.args = QString();
        addSong(song);
    }
}