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
        QString title;
        QString uploader; // Artist or uploader name
        int platform; // Boxing for network::SupportInterface enum, may support more interface types later
        int duration;
        QString filepath;
        QString coverName;
        QString args; // Additional arguments or metadata for interface
        bool operator==(const SongInfo& other) const {
            return title == other.title &&
                   uploader == other.uploader &&
                   platform == other.platform;
        }
    };

    struct PlaylistInfo {
        QString name;
        QString creator;
        QString description;
        QString coverUri;
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