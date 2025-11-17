#include <catch2/catch_test_macros.hpp>
#include <QDir>
#include "test_utils.h"
#include <filesystem>
#include <iostream>
#include <QUrl>
#include <QFile>
#include <QIODevice>

#include "config/config_manager.h"
#include "playlist/playlist_manager.h"

using namespace playlist;

TEST_CASE("PlaylistManager adds local media to playlist", "[playlist][local]") {
    testutils::ensureQCoreApplication();

    // Create a unique temp workspace directory
    auto tmp = std::filesystem::temp_directory_path();
    auto stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string folder = (tmp / (std::string("BilibiliPlayerTest_local_") + stamp)).string();
    QDir().mkpath(QString::fromStdString(folder));

    ConfigManager cfg;
    cfg.initialize(QString::fromStdString(folder));

    PlaylistManager mgr(&cfg);
    mgr.initialize();

    auto current = mgr.getCurrentPlaylist();
    REQUIRE(current.isNull() == false);

    // Create test path
    QString networkUrl = "http://example.com/song.mp3";
    QString localAbsolutePath = QDir(QString::fromStdString(folder)).filePath("song_local.mp3");
    QString localRelativePath = "./song_local.mp3";
    SECTION("Test network url as filepath") {
        // No args, should fail
        SongInfo s {
            .title = "Network Song",
            .uploader = "Artist",
            .platform = 0,
            .duration = 200,
            .filepath = networkUrl,
            .args = ""
        };
        REQUIRE(mgr.addSongToPlaylist(s, current) == false);

        SongInfo s1 {
            .title = "Network Song",
            .uploader = "Artist",
            .platform = 0,
            .duration = 200,
            .filepath = networkUrl,
            .args = "some_args"
        };
        REQUIRE(mgr.addSongToPlaylist(s1, current) == true);
    }

    SECTION("Test local absolute path as filepath") {
        // Create dummy file
        QFile file(localAbsolutePath);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("dummy data");
        file.close();

        SongInfo s {
            .title = "Local Song",
            .uploader = "Artist",
            .platform = 0,
            .duration = 180,
            .filepath = localAbsolutePath,
            .args = ""
        };
        REQUIRE(mgr.addSongToPlaylist(s, current) == true);
    }

    SECTION("Test local relative path as filepath") {
        // Create dummy file
        QFile file(QDir(QString::fromStdString(folder)).filePath("song_local.mp3"));
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("dummy data");
        file.close();

        SongInfo s {
            .title = "Local Relative Song",
            .uploader = "Artist",
            .platform = 0,
            .duration = 150,
            .filepath = localRelativePath,
            .args = ""
        };
        REQUIRE(mgr.addSongToPlaylist(s, current) == false);

        SongInfo s1{
            .title = "Local Relative Song",
            .uploader = "Artist",
            .platform = 0,
            .duration = 150,
            .filepath = localRelativePath,
            .args = "some_args"
        };
        REQUIRE(mgr.addSongToPlaylist(s1, current) == true);
    }

    // cleanup
    QDir d(QString::fromStdString(folder));
    d.removeRecursively();
}
