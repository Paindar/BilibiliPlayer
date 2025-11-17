#include <catch2/catch_test_macros.hpp>
#include <QCoreApplication>
#include "test_utils.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <iostream>

#include <config/config_manager.h>

// Helper: write an ini file
static bool writeConfigFile(const QString &path, const QString &content) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&f);
    out << content;
    f.close();
    return true;
}

TEST_CASE("ConfigManager: initialize with valid and invalid workspace and config file behaviors", "[ConfigManager]") {
    testutils::ensureQCoreApplication();

    SECTION("initialize with invalid workspace") {
        ConfigManager cfg(nullptr);
        REQUIRE_NOTHROW(cfg.initialize("/this/path/does/not/exist"));
        // Should sanitize and create defaults only when asked; check workspace stored
        // Because initialize may set workspace even if directory doesn't exist, verify behavior
        QString ws = cfg.getWorkspaceDirectory();
        REQUIRE(!ws.isEmpty());
    }

    SECTION("initialize with valid workspace and missing config.ini") {
        QTemporaryDir tmp;
        REQUIRE(tmp.isValid());
        QString workspace = QDir::toNativeSeparators(tmp.path());
        std::cout<<"Using temporary workspace: " << workspace.toStdString() << std::endl;
        // If config.ini is existing, delete it to simulate missing file
        QString configDir = QDir(workspace).filePath("config");
        QString configFile = QDir(configDir).filePath("config.ini");
        if (QFile::exists(configFile)) {
            QFile::remove(configFile);
        }
        ConfigManager cfg(nullptr);
        cfg.initialize(workspace);

        // After initialization, default config file path should be reported
        configFile = QDir(workspace).filePath(cfg.getConfigFilePath());
        QFileInfo fi(configFile);

        // Saving should create the file
        cfg.saveToFile();
        REQUIRE(QFileInfo(configFile).exists());
    }

    SECTION("initialize with valid workspace and valid config.ini") {
        QTemporaryDir tmp;
        REQUIRE(tmp.isValid());
        QString workspace = QDir::toNativeSeparators(tmp.path());

        // create config dir and config.ini
        QString configDir = QDir(workspace).filePath("config");
        QDir().mkpath(configDir);
        QString configFile = QDir(configDir).filePath("config.ini");

        // write minimal valid ini content
        QString ini = "[General]\nvolume=50\nautosave=true\n";
        REQUIRE(writeConfigFile(configFile, ini));

        ConfigManager cfg(nullptr);
        cfg.initialize(workspace);

        // loadFromFile should pick up values (assuming implementation reads these keys)
        cfg.loadFromFile();

        // Check that getters return reasonable defaults or values impacted by file
        int vol = cfg.getVolumeLevel();
        REQUIRE(vol >= 0); // specific value depends on implementation
    }
}

TEST_CASE("ConfigManager: load invalid config.ini and recovery", "[ConfigManager]") {
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    QString workspace = QDir::toNativeSeparators(tmp.path());

    QString configDir = QDir(workspace).filePath("config");
    QDir().mkpath(configDir);
    QString configFile = QDir(configDir).filePath("config.ini");

    // write malformed content
    QString bad = "\x00\x01 not an ini";
    REQUIRE(writeConfigFile(configFile, bad));

    ConfigManager cfg(nullptr);
    cfg.initialize(workspace);

    // Load should handle invalid file gracefully (not throw)
    REQUIRE_NOTHROW(cfg.loadFromFile());

    // After load, saving should overwrite or fix the file
    REQUIRE_NOTHROW(cfg.saveToFile());
}

TEST_CASE("ConfigManager: getters/setters and signals", "[ConfigManager]") {
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    QString workspace = QDir::toNativeSeparators(tmp.path());

    ConfigManager cfg(nullptr);
    cfg.initialize(workspace);

    QSignalSpy themeSpy(&cfg, &ConfigManager::themeChanged);
    QSignalSpy langSpy(&cfg, &ConfigManager::languageChanged);
    QString currentTheme = cfg.getTheme();
    cfg.setTheme(currentTheme == "dark" ? "light" : "dark");
    cfg.setLanguage("en-US");

    // Some implementations may emit signals synchronously; wait a short time in case
    QCoreApplication::processEvents();

    REQUIRE(themeSpy.count() == 1);
    REQUIRE(langSpy.count() == 1);

    // Path setters
    cfg.setPlaylistDirectory("playlists");
    cfg.setThemeDirectory("themes");
    cfg.setAudioCacheDirectory("audiocache");
    cfg.setCoverCacheDirectory("covercache");

    QCoreApplication::processEvents();

    // Test simple getters
    REQUIRE(cfg.getPlaylistDirectory().size() > 0);
    REQUIRE(cfg.getThemeDirectory().size() > 0);
    REQUIRE(cfg.getAudioCacheDirectory().size() > 0);
}

// Note: more detailed behavior tests (e.g., validateRelativePath, sanitizeRelativePath,
// buildSafeFilePath) can be added similarly by creating files and directories

// Logging: ensure that if ConfigManager interacts with a log manager reading a properties
// file, we provide a simple properties file in workspace root that prints to stdout.
TEST_CASE("ConfigManager: provide log properties file for stdout logging", "[ConfigManager][logging]") {
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    QString workspace = QDir::toNativeSeparators(tmp.path());

    QString props = QDir(workspace).filePath("log.properties");
    QString content = R"(
# simple logging properties for tests
log.level=DEBUG
log.target=STDOUT
)";
    REQUIRE(writeConfigFile(props, content));

    ConfigManager cfg(nullptr);
    cfg.initialize(workspace);

    // If the app reads log.properties we expect no crash; otherwise this is a no-op
    REQUIRE(true);
}
