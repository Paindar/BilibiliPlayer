#pragma once

#include <QString>
#include <QList>
#include <QUuid>
#include <QMetaType>

namespace playlist
{
    enum class PlayMode {
        SingleLoop = 0,     // Loop current song
        PlaylistLoop = 1,   // Loop entire playlist
        Random = 2          // Random next song
    };

    struct SongInfo {
        QString title;      // Song title
        QString uploader;   // Artist or uploader name
        int platform;       // Boxing for network::SupportInterface enum, may support more interface types in the future
        int duration;       // Duration in seconds, may be 0 or INT_MAX if unknown
        QString filepath;   // Local file path, empty if it hasn't been downloaded
        QString coverName;  // Local cover image filename (not including tmp/cover/ path), empty if not available
        QString args;       // Additional arguments or metadata for interface
        QUuid uuid;         // Unique identifier for the song (for SQLite storage in Phase 2)
        bool operator==(const SongInfo& other) const {
            return uuid == other.uuid;
        }
    };

    struct PlaylistInfo {
        QString name;       // Playlist name
        QString creator;    // Creator or author of the playlist
        QString description; // Playlist description
        QString coverUri;   // URI for playlist cover image
        QUuid uuid; // Unique identifier for the playlist
        bool operator==(const PlaylistInfo& other) const {
            return uuid == other.uuid;
        }
    };

    struct CategoryInfo {
        QString name;
        QUuid uuid; // Unique identifier for the category
        bool operator==(const CategoryInfo& other) const {
            return uuid == other.uuid;
        }
    };
}

// Declare metatypes for Qt's type system
Q_DECLARE_METATYPE(playlist::SongInfo)
Q_DECLARE_METATYPE(playlist::PlaylistInfo)
Q_DECLARE_METATYPE(playlist::CategoryInfo)
Q_DECLARE_METATYPE(playlist::PlayMode)