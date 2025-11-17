#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <filesystem>
#include <chrono>

#include "config/config_manager.h"
#include "playlist/playlist_manager.h"
#include "playlist/playlist.h"
#include "test_utils.h"

using namespace playlist;

// Ensure a QCoreApplication exists for Qt classes used by ConfigManager/QSettings
struct QtAppGuard {
    QtAppGuard() {
        testutils::ensureQCoreApplication();
    }
    ~QtAppGuard() {}
};
static QtAppGuard _qtGuard;

// RAII temporary workspace helper: creates a unique directory under the system temp
struct TempWorkspace {
    TempWorkspace(const QString& name) {
        auto tmp = std::filesystem::temp_directory_path();
        auto stamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::string folder = (tmp / (std::string("BilibiliPlayerTest_") + name.toStdString() + "_" + stamp)).string();
        path_ = QDir::cleanPath(QString::fromStdString(folder));
        QDir().mkpath(path_);
    }
    ~TempWorkspace() {
        // best-effort cleanup; ignore failure
        QDir d(path_);
        if (d.exists()) d.removeRecursively();
    }
    QString path() const { return path_; }
private:
    QString path_;
};

TEST_CASE("PlaylistManager: category and playlist lifecycle", "[playlist_manager][category]") {
    TempWorkspace tw("pm_cat_test");
    QString ws = tw.path();
    ConfigManager cfg;
    cfg.initialize(ws);

    PlaylistManager mgr(&cfg);
    mgr.initialize();

    // ensure default setup created
    QUuid current = mgr.getCurrentPlaylist();
    REQUIRE(current.isNull() == false);

    // Create a new category
    playlist::CategoryInfo cat;
    cat.name = "Unit Test Category";
    cat.uuid = QUuid::createUuid();
    REQUIRE(mgr.addCategory(cat) == true);

    // Create a playlist in the category
    playlist::PlaylistInfo pl;
    pl.name = "UT Playlist";
    pl.creator = "Tester";
    pl.description = "desc";
    pl.coverUri = "";
    pl.uuid = QUuid::createUuid();

    REQUIRE(mgr.addPlaylist(pl, cat.uuid) == true);

    auto loaded = mgr.getPlaylist(pl.uuid);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->name == pl.name);

    // Remove playlist
    REQUIRE(mgr.removePlaylist(pl.uuid, cat.uuid) == true);
    REQUIRE(mgr.getPlaylist(pl.uuid).has_value() == false);

    // Remove category
    REQUIRE(mgr.removeCategory(cat.uuid) == true);
}

TEST_CASE("PlaylistManager: song operations", "[playlist_manager][songs]") {
    TempWorkspace tw("pm_song_test");
    QString ws = tw.path();
    ConfigManager cfg;
    cfg.initialize(ws);

    PlaylistManager mgr(&cfg);
    mgr.initialize();

    // Use default category/playlist created by ensureDefaultSetup
    QUuid currentPlaylist = mgr.getCurrentPlaylist();
    REQUIRE(currentPlaylist.isNull() == false);

    playlist::SongInfo s;
    s.title = "Song A";
    s.uploader = "Artist";
    s.platform = 1;
    s.duration = 200;
    s.filepath = "file://songA";

    REQUIRE(mgr.addSongToPlaylist(s, currentPlaylist) == true);

    // Iterate songs and verify
    bool found = false;
    mgr.iterateSongsInPlaylist(currentPlaylist, [&](const playlist::SongInfo& song){
        if (song.title == s.title && song.uploader == s.uploader) { found = true; return false; }
        return true;
    });
    REQUIRE(found == true);

    // Remove by value
    REQUIRE(mgr.removeSongFromPlaylist(s, currentPlaylist) == true);

    // Verify removal
    bool any = false;
    mgr.iterateSongsInPlaylist(currentPlaylist, [&](const playlist::SongInfo&){ any = true; return false; });
    REQUIRE(any == false);
}

TEST_CASE("PlaylistManager: save and load JSON roundtrip", "[playlist_manager][json]") {
    TempWorkspace tw("pm_json_test");
    QString ws = tw.path();
    ConfigManager cfg;
    cfg.initialize(ws);

    PlaylistManager mgr(&cfg);
    mgr.initialize();

    // Create category + playlist + song
    playlist::CategoryInfo cat;
    cat.name = "JSON Cat";
    cat.uuid = QUuid::createUuid();
    REQUIRE(mgr.addCategory(cat));

    playlist::PlaylistInfo pl;
    pl.name = "JSON Playlist";
    pl.creator = "Tester";
    pl.description = "desc";
    pl.coverUri = "";
    pl.uuid = QUuid::createUuid();
    REQUIRE(mgr.addPlaylist(pl, cat.uuid));

    playlist::SongInfo s;
    s.title = "JSON Song";
    s.uploader = "Uploader";
    s.platform = 1;
    s.duration = 42;
    s.filepath = "x";
    s.args = "1234";
    REQUIRE(mgr.addSongToPlaylist(s, pl.uuid));

    // Save to JSON (saveAllCategories uses saveCategoriesToJsonFile)
    mgr.saveAllCategories();

    // Create a fresh manager and load from same workspace
    PlaylistManager mgr2(&cfg);
    // don't call initialize() to avoid resetting defaults; directly call loadCategoriesFromFile
    bool ok = mgr2.loadCategoriesFromFile();
    REQUIRE(ok == true);

    // Verify category exists
    auto catOpt = mgr2.getCategory(cat.uuid);
    REQUIRE(catOpt.has_value());

    // Verify playlist exists
    auto plOpt = mgr2.getPlaylist(pl.uuid);
    REQUIRE(plOpt.has_value());

    // Verify song exists
    bool found = false;
    mgr2.iterateSongsInPlaylist(pl.uuid, [&](const playlist::SongInfo& song){ if (song.title == s.title) { found = true; return false; } return true; });
    REQUIRE(found == true);
}

TEST_CASE("PlaylistManager basic CRUD", "[playlist]") {
    int argc = 1;
    char arg0[] = "test";
    char* argv[] = { arg0, nullptr };
    // QCoreApplication app(argc, argv);

    TempWorkspace tw2("basic_crud");
    QString ws = tw2.path();

    ConfigManager cfg;
    cfg.initialize(ws);

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
    s.duration = 201;
    s.args = "1234";
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
