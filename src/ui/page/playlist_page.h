#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMetaType>
#include <QUuid>
#include "../navigator/i_navigable_page.h"
#include "../../playlist/playlist.h"

QT_BEGIN_NAMESPACE
class Ui_PlaylistPageForm;
QT_END_NAMESPACE

class PlaylistManager;

class PlaylistPage : public QWidget, public INavigablePage
{
    Q_OBJECT

public:
    static const QString PLAYLIST_PAGE_TYPE;
    
    explicit PlaylistPage(QWidget *parent = nullptr);
    ~PlaylistPage();
    
    void setCurrentPlaylist(const QUuid& playlistId);
    void refreshPlaylist();
    
    void addSong(const playlist::SongInfo& song);
    void removeSong(int index);
    void clearSongs();
    
    // Getters
    QUuid getCurrentPlaylistId() const { return m_currentPlaylistId; }
    
    // INavigablePage interface
    QString getNavigationState() const override;
    void restoreFromState(const QString& state) override;

signals:
    void songDoubleClicked(const playlist::SongInfo& song);
    void songsSelected(const QList<playlist::SongInfo>& songs);
    void playlistModified();

private slots:
    void onSongDoubleClicked(QTreeWidgetItem* item, int column);
    void onSelectionChanged();

private:
    void setupUI();
    void SetupPlaylistManager();
    void updatePlaylistInfo();
    void updateSongList();
    QString formatDuration(int seconds) const;
    QTreeWidgetItem* createSongItem(const playlist::SongInfo& song);
    
    Ui_PlaylistPageForm *ui;
    PlaylistManager* m_playlistManager;
    QUuid m_currentPlaylistId;
};