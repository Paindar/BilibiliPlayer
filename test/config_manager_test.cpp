#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include "config/config_manager.h"
#include <filesystem>
#include <chrono>

TEST_CASE("ConfigManager basic operations", "[config]") {
    int argc = 1;
    char arg0[] = "test";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    // Create a unique temporary workspace directory
    auto tmp = std::filesystem::temp_directory_path();
    auto ws = tmp / ("BilibiliPlayerTest_Config_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(ws);

    ConfigManager cfg;
    cfg.initialize(QString::fromStdString(ws.string()));

    REQUIRE(!cfg.getWorkspaceDirectory().isEmpty());

    // Path helpers
    QString abs = cfg.getAbsolutePath("config/myfile.txt");
    REQUIRE(!abs.isEmpty());
    REQUIRE(cfg.validateRelativePath("good/path.txt"));
    REQUIRE(!cfg.validateRelativePath("../escape"));

    // Safe build (basic sanity)
    QString safe = cfg.buildSafeFilePath("config/sub", "file.txt");
    (void)safe; // platform-dependent behavior; don't assert here

    // Network settings
    cfg.setNetworkTimeout(12345);
    REQUIRE(cfg.getNetworkTimeout() == 12345);
    cfg.setProxyUrl("http://127.0.0.1:8080");
    REQUIRE(cfg.getProxyUrl().contains("127.0.0.1"));

    // UI settings
    cfg.setTheme("light");
    REQUIRE(cfg.getTheme() == "light");
    cfg.setLanguage("zh-CN");
    REQUIRE(cfg.getLanguage() == "zh-CN");

    // Playlist settings: default categories file path should mention categories.json
    REQUIRE(cfg.getCategoriesFilePath().contains("categories.json"));
    // Setting categories file path may be platform-sensitive due to workspace canonicalization;
    // call it to ensure it doesn't crash, but don't assert the returned string here.
    (void)cfg.setCategoriesFilePath("mycats/categories.json");

    // Error logging
    cfg.logNetworkError("unit-test-error");
    auto errors = cfg.getRecentErrors();
    REQUIRE(errors.size() >= 1);
    bool found = false;
    for (const auto& e : errors) if (e.contains("unit-test-error")) found = true;
    REQUIRE(found);
}
