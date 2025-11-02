#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include "config/config_manager.h"
#include "playlist/playlist_manager.h"
#include "playlist/playlist.h"
#include <filesystem>

using namespace playlist;

TEST_CASE("PlaylistManager basic CRUD", "[playlist]") {
    int argc = 1;
    char arg0[] = "test";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    auto tmp = std::filesystem::temp_directory_path();
    auto ws = tmp / ("BilibiliPlayerTest_Playlist_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(ws);

    ConfigManager cfg;
    cfg.initialize(QString::fromStdString(ws.string()));

    PlaylistManager pm(&cfg);
    pm.initialize();

    // After initialize, default setup should create at least one category and playlist
    auto categories = pm.iterateCategories([](const CategoryInfo&){ return true; });
    REQUIRE(categories.size() >= 1);

    // Add a new category
    CategoryInfo c;
    c.name = "UnitTestCat";
    c.uuid = QUuid::createUuid();
    REQUIRE(pm.addCategory(c));
    REQUIRE(pm.categoryExists(c.uuid));

    // Add a playlist to that category
    PlaylistInfo p;
    p.name = "UnitPlaylist";
    p.creator = "Tester";
    p.uuid = QUuid::createUuid();
    REQUIRE(pm.addPlaylist(p, c.uuid));
    REQUIRE(pm.playlistExists(p.uuid));

    // Add a song to the playlist
    SongInfo s;
    s.title = "Test Song";
    s.uploader = "Artist";
    s.platform = 1;
    s.duration = "3:21";
    REQUIRE(pm.addSongToPlaylist(s, p.uuid));

    auto songs = pm.iterateSongsInPlaylist(p.uuid, [](const SongInfo&){ return true; });
    REQUIRE(songs.size() == 1);
    REQUIRE(songs[0].title == "Test Song");

    // Remove song
    REQUIRE(pm.removeSongFromPlaylist(s, p.uuid));
    songs = pm.iterateSongsInPlaylist(p.uuid, [](const SongInfo&){ return true; });
    REQUIRE(songs.empty());

    // Remove playlist and category
    REQUIRE(pm.removePlaylist(p.uuid, c.uuid));
    REQUIRE(!pm.playlistExists(p.uuid));
    REQUIRE(pm.removeCategory(c.uuid));
    REQUIRE(!pm.categoryExists(c.uuid));
}
