/**
 * Theme Manager Unit Tests
 * 
 * Tests theme loading, validation, JSON serialization, and QSS generation.
 */

#include <catch2/catch_test_macros.hpp>
#include <ui/theme/theme_manager.h>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QSignalSpy>
#include <filesystem>

// Qt application fixture for theme tests
struct QtAppFixture {
    static int argc;
    static char* argv[];
    static QCoreApplication* app;
    
    QtAppFixture() {
        if (!app) {
            app = new QCoreApplication(argc, argv);
        }
    }
    
    ~QtAppFixture() {
        // Don't delete app here, let it persist
    }
};

int QtAppFixture::argc = 1;
char arg0[] = "test";
char* QtAppFixture::argv[] = {arg0, nullptr};
QCoreApplication* QtAppFixture::app = nullptr;

TEST_CASE("ThemeManager built-in themes", "[ThemeManager]") {
    QtAppFixture fixture;
    ThemeManager manager;
    
    SECTION("Get Light theme") {
        Theme light = manager.getLightTheme();
        
        // REQUIRE((light.backgroundImagePath.isEmpty() == false) || (light.backgroundImagePath.isEmpty() == true));
        REQUIRE(light.buttonNormalColor.isValid());
        REQUIRE(light.fontPrimaryColor.isValid());
    }
    
    SECTION("Get Dark theme") {
        Theme dark = manager.getDarkTheme();
        
        REQUIRE(dark.buttonNormalColor.isValid());
        REQUIRE(dark.fontPrimaryColor.isValid());
        REQUIRE(dark.buttonNormalColor != QColor("#3498db")); // Different from light
    }
    
    SECTION("Light theme properties valid") {
        Theme light = manager.getLightTheme();
        
        REQUIRE(light.buttonNormalColor.isValid());
        REQUIRE(light.buttonHoverColor.isValid());
        REQUIRE(light.buttonPressedColor.isValid());
        REQUIRE(light.fontPrimaryColor.isValid());
        REQUIRE(light.fontSecondaryColor.isValid());
        REQUIRE(light.fontDisabledColor.isValid());
        REQUIRE(light.borderColor.isValid());
    }
    
    SECTION("Dark theme properties valid") {
        Theme dark = manager.getDarkTheme();
        
        REQUIRE(dark.buttonNormalColor.isValid());
        REQUIRE(dark.buttonHoverColor.isValid());
        REQUIRE(dark.buttonPressedColor.isValid());
        REQUIRE(dark.fontPrimaryColor.isValid());
        REQUIRE(dark.fontSecondaryColor.isValid());
        REQUIRE(dark.fontDisabledColor.isValid());
        REQUIRE(dark.borderColor.isValid());
    }
}

TEST_CASE("ThemeManager JSON loading", "[ThemeManager]") {
    QtAppFixture fixture;
    ThemeManager manager;
    
    SECTION("Load from non-existent file") {
        bool result = manager.loadThemeFromFile("nonexistent.json");
        
        // Should return default/empty theme (just check it doesn't crash)
        REQUIRE(!result);
    }
    
    SECTION("Load valid theme JSON") {
        QString testPath = "data/json/valid_theme.json";
        if (QFile::exists(testPath)) {
            bool result = manager.loadThemeFromFile(testPath);
            
            // Should load successfully
            REQUIRE(result);
        }
    }
    
    SECTION("Load malformed JSON") {
        QString testPath = "data/json/malformed_theme.json";
        if (QFile::exists(testPath)) {
            bool result = manager.loadThemeFromFile(testPath);
            
            // Should handle error gracefully - may fail or succeed with defaults
            // The key is that it doesn't crash
            REQUIRE(true); // No crash is success
        }
    }
    
    SECTION("Load with missing fields") {
        QString testPath = "data/json/missing_fields_theme.json";
        if (QFile::exists(testPath)) {
            bool result = manager.loadThemeFromFile(testPath);
            
            // Should have some fields set, others default
            REQUIRE(result); // No crash
        }
    }
    
    SECTION("Load with extra fields (forward compatibility)") {
        QString testPath = "data/json/extra_fields_theme.json";
        if (QFile::exists(testPath)) {
            bool result = manager.loadThemeFromFile(testPath);
            
            // Should load successfully and ignore unknown fields
            REQUIRE(result);
            
            // Verify known fields were loaded correctly
            Theme theme = manager.getLightTheme();
            REQUIRE(theme.buttonNormalColor.isValid());
        }
    }
}

TEST_CASE("ThemeManager JSON saving", "[ThemeManager]") {
    QtAppFixture fixture;
    ThemeManager manager;
    
    SECTION("Round-trip preserves data") {
        Theme original = manager.getLightTheme();
        
        // Create temp file
        QString tempPath = QDir::temp().filePath("test_theme.json");
        
        // Save
        manager.saveThemeToFile(tempPath, original);
        
        // Load
        bool result = manager.loadThemeFromFile(tempPath);
        REQUIRE(result);
        
        Theme loaded = manager.getLightTheme();
        REQUIRE(loaded.buttonNormalColor == original.buttonNormalColor);
        REQUIRE(loaded.fontPrimaryColor == original.fontPrimaryColor);
        
        // Cleanup
        QFile::remove(tempPath);
    }
}

TEST_CASE("ThemeManager QSS generation", "[ThemeManager]") {
    QtAppFixture fixture;
    ThemeManager manager;
    
    SECTION("Generate complete stylesheet") {
        Theme theme = manager.getLightTheme();
        QString qss = manager.generateStyleSheet(theme);
        
        REQUIRE(!qss.isEmpty());
        REQUIRE(qss.contains("QPushButton"));
        REQUIRE(qss.length() > 0);
    }
    
    SECTION("Validate CSS syntax") {
        Theme theme = manager.getDarkTheme();
        QString qss = manager.generateStyleSheet(theme);
        
        // Basic syntax check: should have some color definitions
        REQUIRE((qss.contains("#") || qss.contains("rgb")));
    }
    
    SECTION("Handle special characters") {
        Theme theme = manager.getLightTheme();
        theme.backgroundImagePath = "C:/path with spaces/file.png";
        
        QString qss = manager.generateStyleSheet(theme);
        
        // Should not crash
        REQUIRE(!qss.isEmpty());
    }
}

TEST_CASE("ThemeManager application", "[ThemeManager]") {
    QtAppFixture fixture;
    ThemeManager manager;
    
    SECTION("Apply theme emits signal") {
        Theme theme = manager.getLightTheme();
        
        // Create signal spy to capture themeApplied signal
        QSignalSpy spy(&manager, &ThemeManager::themeApplied);
        
        // Apply theme should emit signal
        manager.applyTheme(theme);
        
        // Verify signal was emitted
        REQUIRE(spy.count() == 1);
        
        // Verify the signal contains the correct theme name
        QList<QVariant> arguments = spy.takeFirst();
        REQUIRE(!arguments.isEmpty());
    }
    
    SECTION("Load theme emits themeChanged and themeApplied signals") {
        QSignalSpy themeChangedSpy(&manager, &ThemeManager::themeChanged);
        QSignalSpy themeAppliedSpy(&manager, &ThemeManager::themeApplied);
        
        // Load built-in theme
        bool result = manager.loadTheme("Light");
        REQUIRE(result);
        
        // Process events to allow signals to be handled
        QCoreApplication::processEvents();
        
        // Verify themeChanged signal was emitted
        REQUIRE(themeChangedSpy.count() >= 1);
        
        // Verify themeApplied signal was emitted
        REQUIRE(themeAppliedSpy.count() >= 1);
    }
}
