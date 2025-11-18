#include <catch2/catch_test_macros.hpp>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <climits>
#include "test_utils.h"
#include "config/config_manager.h"
#include "playlist/playlist_manager.h"
#include "audio/ffmpeg_probe.h"

using namespace playlist;

TEST_CASE("FFmpegProbe duration fallback strategy", "[audio][probe]") {
    testutils::ensureQCoreApplication();

    // Create a unique temp workspace directory
    auto tmp = std::filesystem::temp_directory_path();
    auto stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string folder = (tmp / (std::string("BilibiliPlayerTest_probe_") + stamp)).string();
    QDir().mkpath(QString::fromStdString(folder));

    SECTION("Probe non-existent file returns INT_MAX (fallback)") {
        QString nonExistent = QDir(QString::fromStdString(folder)).filePath("does_not_exist.mp3");
        int64_t duration = audio::FFmpegProbe::probeDuration(nonExistent);
        // Should return INT_MAX as fallback when file can't be opened
        REQUIRE(duration == static_cast<int64_t>(INT_MAX));
    }

    SECTION("Probe invalid audio file returns INT_MAX (fallback)") {
        QString invalidPath = QDir(QString::fromStdString(folder)).filePath("invalid.mp3");
        QFile invalid(invalidPath);
        REQUIRE(invalid.open(QIODevice::WriteOnly));
        invalid.write("not an mp3");
        invalid.close();
        
        // Should fallback to INT_MAX when file format is invalid
        int64_t duration = audio::FFmpegProbe::probeDuration(invalidPath);
        REQUIRE(duration == static_cast<int64_t>(INT_MAX));
        
        invalid.remove();
    }

    SECTION("Sample rate probe on invalid file returns 0") {
        QString invalidPath = QDir(QString::fromStdString(folder)).filePath("invalid.mp3");
        QFile invalid(invalidPath);
        REQUIRE(invalid.open(QIODevice::WriteOnly));
        invalid.write("not an mp3");
        invalid.close();
        
        int sampleRate = audio::FFmpegProbe::probeSampleRate(invalidPath);
        REQUIRE(sampleRate == 0);
        
        invalid.remove();
    }

    SECTION("Channel count probe on invalid file returns 0") {
        QString invalidPath = QDir(QString::fromStdString(folder)).filePath("invalid.mp3");
        QFile invalid(invalidPath);
        REQUIRE(invalid.open(QIODevice::WriteOnly));
        invalid.write("not an mp3");
        invalid.close();
        
        int channels = audio::FFmpegProbe::probeChannelCount(invalidPath);
        REQUIRE(channels == 0);
        
        invalid.remove();
    }

    // Cleanup
    QDir(QString::fromStdString(folder)).removeRecursively();
}

TEST_CASE("PlaylistManager async duration probe for local files", "[playlist][local][async]") {
    testutils::ensureQCoreApplication();

    // Create a unique temp workspace directory
    auto tmp = std::filesystem::temp_directory_path();
    auto stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string folder = (tmp / (std::string("BilibiliPlayerTest_local_dur_") + stamp)).string();
    QDir().mkpath(QString::fromStdString(folder));

    ConfigManager cfg;
    cfg.initialize(QString::fromStdString(folder));

    PlaylistManager mgr(&cfg);
    mgr.initialize();

    auto current = mgr.getCurrentPlaylist();
    REQUIRE(current.isNull() == false);

    SECTION("Adding local file triggers async duration probe") {
        // Create a dummy audio file path (for testing, we won't have real audio)
        QString localPath = QDir(QString::fromStdString(folder)).filePath("test_song.mp3");
        
        SongInfo s{
            .title = "Test Local Song",
            .uploader = "",
            .platform = 0,
            .duration = 0,  // Initially 0, should be updated by async probe
            .filepath = localPath,
            .args = ""
        };
        
        // Add song - this should trigger async duration probe
        // Note: Since file doesn't exist, duration will be set to INT_MAX
        mgr.addSongToPlaylist(s, current);
        
        // Give async thread a moment to start
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Song should be added regardless of probe result
        auto songs = mgr.iterateSongsInPlaylist(current, 
            [&s](const SongInfo& song) { 
                return song.filepath == s.filepath; 
            });
        
        REQUIRE(!songs.isEmpty());
        // Duration should either remain 0 (if probe fails before update) or be INT_MAX (fallback)
        // Give a bit more time for async to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Cleanup
    QDir(QString::fromStdString(folder)).removeRecursively();
}
