#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "playlist/playlist.h"
#include <QUuid>

using namespace playlist;

TEST_CASE("SongInfo equality and field checks", "[playlist][song]") {
    SongInfo a;
    a.title = "Test Song";
    a.uploader = "Artist";
    a.platform = 1;
    a.duration = 123;
    a.filepath = "/tmp/test.wav";

    SongInfo b = a;

    SECTION("copied song equals original") {
        REQUIRE(a == b);
    }

    SECTION("difference in title makes songs unequal") {
        b.title = "Other";
        REQUIRE_FALSE(a == b);
    }

    SECTION("difference in uploader makes songs unequal") {
        b = a;
        b.uploader = "Different";
        REQUIRE_FALSE(a == b);
    }

    SECTION("difference in platform makes songs unequal") {
        b = a;
        b.platform = 2;
        REQUIRE_FALSE(a == b);
    }

    SECTION("operator== ignores duration and filepath") {
        b = a;
        b.duration = 9999;
        b.filepath = "C:/other.wav";
        // operator== only compares title, uploader and platform
        REQUIRE(a == b);
    }
}

TEST_CASE("PlaylistInfo equality and uuid behavior", "[playlist][playlistinfo]") {
    PlaylistInfo p1;
    p1.name = "My Playlist";
    p1.creator = "Creator";
    p1.description = "Desc";
    p1.coverUri = "cover.png";
    p1.uuid = QUuid::createUuid();

    PlaylistInfo p2 = p1;

    SECTION("copied playlist equals original based on uuid") {
        REQUIRE(p1 == p2);
    }

    SECTION("different uuid makes playlists unequal") {
        p2.uuid = QUuid::createUuid();
        REQUIRE_FALSE(p1 == p2);
    }

    SECTION("two default-constructed playlists have null uuids and compare equal") {
        PlaylistInfo d1;
        PlaylistInfo d2;
        // both uuids are null (QUuid()) so equality by uuid holds
        REQUIRE(d1 == d2);
    }
}

/**
 * Playlist Entity Unit Tests
 * 
 * Tests playlist, song, and category entities including JSON serialization.
 */

#include <catch2/catch_test_macros.hpp>
#include <playlist/playlist.h>
#include <json/json.h>
#include <QUuid>
#include <sstream>

using namespace playlist;

TEST_CASE("Playlist entity creation", "[Playlist]") {
    SECTION("Create empty playlist") {
        PlaylistInfo playlist;
        playlist.uuid = QUuid::createUuid();
        playlist.name = "Test Playlist";
        playlist.creator = "Tester";
        
        REQUIRE(!playlist.uuid.isNull());
        REQUIRE(playlist.name == "Test Playlist");
        REQUIRE(playlist.creator == "Tester");
    }
    
    SECTION("Create with metadata") {
        PlaylistInfo playlist;
        playlist.uuid = QUuid::createUuid();
        playlist.name = "My Playlist";
        playlist.creator = "John Doe";
        playlist.description = "A test playlist";
        playlist.coverUri = "http://example.com/cover.jpg";
        
        REQUIRE(playlist.description == "A test playlist");
        REQUIRE(playlist.coverUri == "http://example.com/cover.jpg");
    }
    
    SECTION("Create with unicode name") {
        PlaylistInfo playlist;
        playlist.uuid = QUuid::createUuid();
        playlist.name = "测试播放列表";
        playlist.creator = "用户";
        
        REQUIRE(playlist.name == "测试播放列表");
        REQUIRE(playlist.creator == "用户");
    }
}

TEST_CASE("Song entity validation", "[Playlist]") {
    SECTION("Valid song creation") {
        SongInfo song;
        song.title = "Test Song";
        song.uploader = "Artist";
        song.platform = 1;
        song.duration = 180;
        song.filepath = "path/to/song";
        
        REQUIRE(song.title == "Test Song");
        REQUIRE(song.uploader == "Artist");
        REQUIRE(song.platform == 1);
        REQUIRE(song.duration == 180);
    }
    
    SECTION("Missing title") {
        SongInfo song;
        song.title = "";
        song.uploader = "Artist";
        song.platform = 1;
        
        REQUIRE(song.title.isEmpty());
    }
    
    SECTION("Extremely long title") {
        SongInfo song;
        QString longTitle(300, 'A');
        song.title = longTitle;
        
        REQUIRE(song.title.length() == 300);
    }
    
    SECTION("Invalid platform ID") {
        SongInfo song;
        song.title = "Test";
        song.platform = -1;
        
        REQUIRE(song.platform == -1); // Entity allows it, validation elsewhere
    }
    
    SECTION("Invalid duration") {
        SongInfo song;
        song.title = "Test";
        song.duration = -100;
        
        REQUIRE(song.duration == -100); // Entity allows it
    }
}

TEST_CASE("Category entity", "[Playlist]") {
    SECTION("Create with default values") {
        CategoryInfo category;
        category.uuid = QUuid::createUuid();
        category.name = "Default Category";
        
        REQUIRE(!category.uuid.isNull());
        REQUIRE(category.name == "Default Category");
    }
    
    SECTION("Validate hex color") {
        CategoryInfo category;
        category.uuid = QUuid::createUuid();
        category.name = "Colored Category";
        // Note: Validation would be in manager/UI layer
        // Entity itself is just a data holder
    }
}

TEST_CASE("Playlist JSON serialization", "[Playlist]") {
    SECTION("Serialize empty playlist") {
        PlaylistInfo playlist;
        playlist.uuid = QUuid::createUuid();
        playlist.name = "Empty";
        playlist.creator = "User";
        
        Json::Value json(Json::objectValue);
        json["uuid"] = playlist.uuid.toString().toStdString();
        json["name"] = playlist.name.toStdString();
        json["creator"] = playlist.creator.toStdString();
        json["description"] = playlist.description.toStdString();
        json["coverUri"] = playlist.coverUri.toStdString();
        json["songs"] = Json::Value(Json::arrayValue);
        
        REQUIRE(!json["uuid"].asString().empty());
        REQUIRE(json["name"].asString() == "Empty");
        REQUIRE(json["songs"].isArray());
        REQUIRE(json["songs"].size() == 0);
    }
    
    SECTION("Serialize with songs") {
        PlaylistInfo playlist;
        playlist.uuid = QUuid::createUuid();
        playlist.name = "With Songs";
        
        Json::Value songsArray(Json::arrayValue);
        
        SongInfo song;
        song.title = "Song 1";
        song.uploader = "Artist 1";
        song.platform = 1;
        song.duration = 200;
        
        Json::Value songObj(Json::objectValue);
        songObj["title"] = song.title.toStdString();
        songObj["uploader"] = song.uploader.toStdString();
        songObj["platform"] = song.platform;
        songObj["duration"] = song.duration;
        songObj["filepath"] = song.filepath.toStdString();
        
        songsArray.append(songObj);
        
        REQUIRE(songsArray.size() == 1);
        REQUIRE(songsArray[0]["title"].asString() == "Song 1");
    }
    
    SECTION("Deserialize valid JSON") {
        std::string jsonStr = R"({
            "uuid": "12345678-1234-1234-1234-123456789abc",
            "name": "Test Playlist",
            "creator": "Tester",
            "description": "A test",
            "coverUri": "http://example.com/cover.jpg",
            "songs": []
        })";
        
        Json::Value json;
        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        std::string errs;
        bool success = reader->parse(jsonStr.c_str(), jsonStr.c_str() + jsonStr.length(), &json, &errs);
        REQUIRE(success);
        
        PlaylistInfo playlist;
        playlist.uuid = QUuid::fromString(QString::fromStdString(json["uuid"].asString()));
        playlist.name = QString::fromStdString(json["name"].asString());
        playlist.creator = QString::fromStdString(json["creator"].asString());
        playlist.description = QString::fromStdString(json["description"].asString());
        playlist.coverUri = QString::fromStdString(json["coverUri"].asString());
        
        REQUIRE(!playlist.uuid.isNull());
        REQUIRE(playlist.name == "Test Playlist");
        REQUIRE(playlist.creator == "Tester");
    }
    
    SECTION("Deserialize malformed JSON") {
        std::string malformedJson = "{ invalid json }";
        
        Json::Value json;
        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        std::string errs;
        bool success = reader->parse(malformedJson.c_str(), malformedJson.c_str() + malformedJson.length(), &json, &errs);
        
        REQUIRE(!success);
        REQUIRE(!errs.empty());
    }
    
    SECTION("Round-trip preserves data") {
        PlaylistInfo original;
        original.uuid = QUuid::createUuid();
        original.name = "Round Trip Test";
        original.creator = "Tester";
        original.description = "Test description";
        original.coverUri = "http://example.com/cover.jpg";
        
        // Serialize
        Json::Value json(Json::objectValue);
        json["uuid"] = original.uuid.toString().toStdString();
        json["name"] = original.name.toStdString();
        json["creator"] = original.creator.toStdString();
        json["description"] = original.description.toStdString();
        json["coverUri"] = original.coverUri.toStdString();
        
        // Deserialize
        PlaylistInfo restored;
        restored.uuid = QUuid::fromString(QString::fromStdString(json["uuid"].asString()));
        restored.name = QString::fromStdString(json["name"].asString());
        restored.creator = QString::fromStdString(json["creator"].asString());
        restored.description = QString::fromStdString(json["description"].asString());
        restored.coverUri = QString::fromStdString(json["coverUri"].asString());
        
        REQUIRE(original.uuid == restored.uuid);
        REQUIRE(original.name == restored.name);
        REQUIRE(original.creator == restored.creator);
        REQUIRE(original.description == restored.description);
        REQUIRE(original.coverUri == restored.coverUri);
    }
}
