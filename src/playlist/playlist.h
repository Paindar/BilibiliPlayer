#pragma once

#include <QString>
#include <QList>
#include <QUuid>

namespace playlist
{
    struct SongInfo {
        QString title;
        QString uploader; // Artist or uploader name
        QString platform;
        QString duration;
        QString args; // Additional arguments or metadata for interface
    };

    struct PlaylistInfo {
        QString name;
        QString creator;
        QString description;
        QString coverUrl;
        QList<SongInfo> songs;
        QUuid uuid; // Unique identifier for the playlist
    };

    struct CategoryInfo {
        QString name;
        QUuid uuid; // Unique identifier for the category
        QList<PlaylistInfo> playlists;
    };
}