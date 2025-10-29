#include "playlist_page.h"
#include "ui_playlist_page.h"
#include <QHeaderView>
#include <QPixmap>
#include <QUrl>
#include <QUrlQuery>
#include <QTime>

const QString PlaylistPage::PLAYLIST_PAGE_TYPE = "playlist";

PlaylistPage::PlaylistPage(QWidget *parent)
    : QWidget(parent)
    , INavigablePage(PLAYLIST_PAGE_TYPE)
    , ui(new Ui_PlaylistPageForm)
{
    ui->setupUi(this);
    setupUI();
    
    // Connect signals
    connect(ui->songListView, &QTreeWidget::itemDoubleClicked, 
            this, &PlaylistPage::onSongDoubleClicked);
    connect(ui->songListView, &QTreeWidget::itemSelectionChanged,
            this, &PlaylistPage::onSelectionChanged);
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
    
    // Set default playlist info
    m_playlistInfo.name = "New Playlist";
    m_playlistInfo.creator = "Unknown";
    m_playlistInfo.description = "No description available.";
    
    updatePlaylistInfo();
}

void PlaylistPage::setPlaylistInfo(const PlaylistInfo& info)
{
    m_playlistInfo = info;
    updatePlaylistInfo();
    updateSongList();
}

void PlaylistPage::addSong(const SongInfo& song)
{
    m_playlistInfo.songs.append(song);
    
    // Add to tree widget
    QTreeWidgetItem* item = createSongItem(song);
    ui->songListView->addTopLevelItem(item);
    
    updatePlaylistInfo();
}

void PlaylistPage::removeSong(int index)
{
    if (index >= 0 && index < m_playlistInfo.songs.size()) {
        m_playlistInfo.songs.removeAt(index);
        
        // Remove from tree widget
        QTreeWidgetItem* item = ui->songListView->topLevelItem(index);
        if (item) {
            delete ui->songListView->takeTopLevelItem(index);
        }
        
        updatePlaylistInfo();
    }
}

void PlaylistPage::clearSongs()
{
    m_playlistInfo.songs.clear();
    ui->songListView->clear();
    updatePlaylistInfo();
}

void PlaylistPage::updatePlaylistInfo()
{
    // Update playlist name and info
    ui->playlistNameLabel->setText(m_playlistInfo.name);
    ui->creatorLabel->setText(QString("Created by: %1").arg(m_playlistInfo.creator));
    ui->songCountLabel->setText(QString("%1 songs").arg(m_playlistInfo.songs.size()));
    ui->descriptionLabel->setText(m_playlistInfo.description);
    
    // Calculate total duration
    int totalSeconds = 0;
    for (const auto& song : m_playlistInfo.songs) {
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
    if (!m_playlistInfo.coverUrl.isEmpty()) {
        // In a real implementation, you would load the image asynchronously
        ui->coverLabel->setText("Loading...");
    } else {
        ui->coverLabel->setText("No Cover");
    }
}

void PlaylistPage::updateSongList()
{
    ui->songListView->clear();
    
    for (const auto& song : m_playlistInfo.songs) {
        QTreeWidgetItem* item = createSongItem(song);
        ui->songListView->addTopLevelItem(item);
    }
}

QTreeWidgetItem* PlaylistPage::createSongItem(const SongInfo& song)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, song.title);
    item->setText(1, song.uploader);
    item->setText(2, song.platform);
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
        SongInfo song = item->data(0, Qt::UserRole).value<SongInfo>();
        emit songDoubleClicked(song);
    }
}

void PlaylistPage::onSelectionChanged()
{
    QList<QTreeWidgetItem*> selectedItems = ui->songListView->selectedItems();
    QList<SongInfo> selectedSongs;
    
    for (QTreeWidgetItem* item : selectedItems) {
        SongInfo song = item->data(0, Qt::UserRole).value<SongInfo>();
        selectedSongs.append(song);
    }
    
    emit songsSelected(selectedSongs);
}

QString PlaylistPage::getNavigationState() const
{
    QUrlQuery query;
    query.addQueryItem("playlistId", m_currentPlaylistId);
    return QString("%1?%2").arg(PLAYLIST_PAGE_TYPE).arg(query.toString());
}

void PlaylistPage::restoreFromState(const QString& state)
{
    QUrl url(state);
    QUrlQuery query(url.query());
    
    m_currentPlaylistId = query.queryItemValue("playlistId");
}