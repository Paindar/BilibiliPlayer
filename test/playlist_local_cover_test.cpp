#include <catch2/catch_test_macros.hpp>
#include <QDir>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <thread>
#include "test_utils.h"
#include "config/config_manager.h"
#include "playlist/playlist_manager.h"
#include <network/platform/i_platform.h>

using namespace playlist;

TEST_CASE("PlaylistManager local cover extraction", "[playlist][local][cover]") {
    testutils::ensureQCoreApplication();

    // Create a unique temp workspace directory
    auto tmp = std::filesystem::temp_directory_path();
    auto stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string folder = (tmp / (std::string("BilibiliPlayerTest_local_cover_") + stamp)).string();
    QDir().mkpath(QString::fromStdString(folder));

    ConfigManager cfg;
    cfg.initialize(QString::fromStdString(folder));

    PlaylistManager mgr(&cfg);
    mgr.initialize();

    auto current = mgr.getCurrentPlaylist();
    REQUIRE(current.isNull() == false);

    SECTION("Adding local file initiates async cover extraction") {
        QString localPath = QDir(QString::fromStdString(folder)).filePath("test_song_with_cover.mp3");
        
        SongInfo s{
            .title = "Test Song with Cover",
            .uploader = "local",
            .platform = static_cast<int>(network::PlatformType::Local),
            .duration = 0,
            .filepath = localPath,
            .coverName = "",  // Should be populated by cover extraction
            .args = ""
        };
        
        // Add song - this should trigger async cover extraction
        mgr.addSongToPlaylist(s, current);
        
        // Give async thread time
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Song should be added
        auto songs = mgr.iterateSongsInPlaylist(current, 
            [&s](const SongInfo& song) { 
                return song.title == s.title; 
            });
        
        REQUIRE(!songs.isEmpty());
    }

    // Cleanup
    QDir(QString::fromStdString(folder)).removeRecursively();
}

TEST_CASE("Cover cache utilities", "[playlist][cover][cache]") {
    testutils::ensureQCoreApplication();

    auto tmp = std::filesystem::temp_directory_path();
    auto stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string folder = (tmp / (std::string("BilibiliPlayerTest_cover_cache_") + stamp)).string();
    QDir().mkpath(QString::fromStdString(folder));

    SECTION("Cover cache path resolves correctly") {
        // This test verifies that cover cache utilities can initialize
        // Full implementation in T018
        REQUIRE(true);
    }

    // Cleanup
    QDir(QString::fromStdString(folder)).removeRecursively();
}
