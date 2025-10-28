#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMetaType>
#include "../navigator/i_navigable_page.h"

QT_BEGIN_NAMESPACE
class Ui_PlaylistPageForm;
QT_END_NAMESPACE

struct SongInfo {
    QString title;
    QString uploader;
    QString platform;
    QString duration;
    QString url;
    QString coverUrl;
};

Q_DECLARE_METATYPE(SongInfo)

struct PlaylistInfo {
    QString name;
    QString creator;
    QString description;
    QString coverUrl;
    QList<SongInfo> songs;
    int totalDuration = 0; // in seconds
};

class PlaylistPage : public QWidget, public INavigablePage
{
    Q_OBJECT

public:
    static const QString PLAYLIST_PAGE_TYPE;
    
    explicit PlaylistPage(QWidget *parent = nullptr);
    ~PlaylistPage();
    
    void setPlaylistInfo(const PlaylistInfo& info);
    void addSong(const SongInfo& song);
    void removeSong(int index);
    void clearSongs();
    
    // INavigablePage interface
    QString getNavigationState() const override;
    void restoreFromState(const QString& state) override;

signals:
    void songDoubleClicked(const SongInfo& song);
    void songsSelected(const QList<SongInfo>& songs);

private slots:
    void onSongDoubleClicked(QTreeWidgetItem* item, int column);
    void onSelectionChanged();

private:
    void setupUI();
    void updatePlaylistInfo();
    void updateSongList();
    QString formatDuration(int seconds) const;
    QTreeWidgetItem* createSongItem(const SongInfo& song);
    
    Ui_PlaylistPageForm *ui;
    PlaylistInfo m_playlistInfo;
    QString m_currentPlaylistId;
};