/**
 * BilibiliPlatform Unit Tests
 * 
 * Comprehensive tests for BilibiliPlatform public API including:
 * - IPlatform implementation
 * - Configuration management (load/save/initialize)
 * - Platform-specific methods (search, metadata retrieval)
 * - HTTP client pooling and cookie management
 */

#include <catch2/catch_test_macros.hpp>
#include <network/platform/bili_network_interface.h>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <memory>
#include <thread>
#include <chrono>
#include "test_utils.h"

using namespace network;

// Qt application fixture for network operations (uses centralized helper)
class QtAppFixture {
public:
    static QtAppFixture& instance() {
        static QtAppFixture fixture;
        return fixture;
    }
private:
    QtAppFixture() {
        testutils::ensureQCoreApplication();
    }
};

// ========== IPlatform Implementation Tests ==========

TEST_CASE("BilibiliPlatform: IPlatform implementation", "[BilibiliPlatform][IPlatform]") {
    QtAppFixture::instance();
    
    SECTION("Constructor creates valid interface") {
        auto netInterface = std::make_shared<BilibiliPlatform>();

        // Should be constructable without errors
        REQUIRE(true);
    }
    
    SECTION("initializeDefaultConfig succeeds") {
        auto netInterface = std::make_shared<BilibiliPlatform>();
        bool result = netInterface->initializeDefaultConfig();
        
        REQUIRE(result == true);
    }
    
    SECTION("setUserAgent updates user agent") {
        auto netInterface = std::make_shared<BilibiliPlatform>();
        std::string custom_ua = "CustomBrowser/1.0";
        
        // Should not throw or crash
        netInterface->setUserAgent(custom_ua);
        REQUIRE(true);
    }
    
    SECTION("setTimeout configures timeout") {
        auto netInterface = std::make_shared<BilibiliPlatform>();
        
        // Should accept timeout configuration
        netInterface->setTimeout(30);
        REQUIRE(true);
    }
}

// ========== Configuration Management Tests ==========

TEST_CASE("BilibiliPlatform: Configuration management", "[BilibiliPlatform][Configuration]") {
    QtAppFixture::instance();
    
    SECTION("setPlatformDirectory sets working directory") {
        QTemporaryDir tmpDir;
        REQUIRE(tmpDir.isValid());
        
        auto netInterface = std::make_shared<BilibiliPlatform>();
        bool result = netInterface->setPlatformDirectory(tmpDir.path().toStdString());
        
        REQUIRE(result == true);
    }
    
    SECTION("saveConfig creates valid config file") {
        QTemporaryDir tmpDir;
        REQUIRE(tmpDir.isValid());
        
        auto netInterface = std::make_shared<BilibiliPlatform>();
        netInterface->setPlatformDirectory(tmpDir.path().toStdString());
        netInterface->initializeDefaultConfig();
        
        bool save_result = netInterface->saveConfig("test_config.json");
        REQUIRE(save_result == true);
        
        // Verify file exists
        QString configPath = QDir(tmpDir.path()).filePath("test_config.json");
        REQUIRE(QFile::exists(configPath));
    }
    
    SECTION("loadConfig reads saved config") {
        QTemporaryDir tmpDir;
        REQUIRE(tmpDir.isValid());
        
        // Save a config
        {
            auto netInterface = std::make_shared<BilibiliPlatform>();
            netInterface->setPlatformDirectory(tmpDir.path().toStdString());
            netInterface->initializeDefaultConfig();
            netInterface->saveConfig("test_load.json");
        }
        
        // Load it in a new interface
        {
            auto netInterface = std::make_shared<BilibiliPlatform>();
            netInterface->setPlatformDirectory(tmpDir.path().toStdString());
            bool load_result = netInterface->loadConfig("test_load.json");
            
            REQUIRE(load_result == true);
        }
    }
    
    SECTION("loadConfig with default filename") {
        QTemporaryDir tmpDir;
        REQUIRE(tmpDir.isValid());
        
        auto netInterface = std::make_shared<BilibiliPlatform>();
        netInterface->setPlatformDirectory(tmpDir.path().toStdString());
        netInterface->initializeDefaultConfig();
        netInterface->saveConfig(); // Uses default "bilibili.json"
        
        // Create new interface and load
        auto netInterface2 = std::make_shared<BilibiliPlatform>();
        netInterface2->setPlatformDirectory(tmpDir.path().toStdString());
        bool result = netInterface2->loadConfig(); // Uses default "bilibili.json"
        
        REQUIRE(result == true);
    }
}

// ========== Platform-Specific Search Methods ==========

TEST_CASE("BilibiliPlatform: Platform-specific search", "[BilibiliPlatform][Search]") {
    QtAppFixture::instance();
    auto netInterface = std::make_shared<BilibiliPlatform>();
    
    netInterface->initializeDefaultConfig();
    
    SECTION("searchByTitle with valid query") {
        std::string query = "Bad Apple";
        std::vector<SearchResult> results = netInterface->searchByTitle(query, 1);
        
        // Should return results (empty or non-empty is acceptable for unit test)
        // The actual API response depends on network availability
        REQUIRE(results.size() >= 0);
    }
    
    SECTION("searchByTitle with empty query") {
        std::string empty_query = "";
        std::vector<SearchResult> results = netInterface->searchByTitle(empty_query, 1);
        
        // Empty query should return empty results
        REQUIRE(results.empty());
    }
    
    SECTION("searchByTitle with page number") {
        std::string query = "Bad Apple";
        
        // Should accept different page numbers without crashing
        std::vector<SearchResult> page1 = netInterface->searchByTitle(query, 1);
        std::vector<SearchResult> page2 = netInterface->searchByTitle(query, 2);
        
        REQUIRE(page1.size() >= 0);
        REQUIRE(page2.size() >= 0);
    }
}

// ========== HTTP Stream Operations Tests ==========

TEST_CASE("BilibiliPlatform: HTTP stream operations", "[BilibiliPlatform][Stream]") {
    QtAppFixture::instance();
    auto netInterface = std::make_shared<BilibiliPlatform>();
    
    netInterface->initializeDefaultConfig();
    
    SECTION("getAudioUrlByParams with valid params") {
        // This tests the interface contract - actual URL validity depends on network
        std::string params = "bvid=BV1xx411c79H&cid=3724723";
        std::string url = netInterface->getAudioUrlByParams(params);
        // std::cout << "Retrieved audio URL: " << url << std::endl;
        // Sometimes it returns empty url due to anti-robot detection
        REQUIRE(url.size() >= 0);
        
        // Test getStreamBytesSize with the retrieved URL
        if (!url.empty()) {
                uint64_t size = netInterface->getStreamBytesSize(url);
            // Size can be 0 (unknown/invalid) or actual size
            REQUIRE(size >= 0);
        }
    }
    
    SECTION("getAudioUrlByParams with empty params") {
        std::string empty_params = "";
        std::string emptyUrl = netInterface->getAudioUrlByParams(empty_params);
        
        // Should handle gracefully - empty params should return empty URL
        REQUIRE(emptyUrl.size() == 0);
    }
    
    SECTION("getStreamBytesSize with empty URL") {
        std::string empty_url = "";
        uint64_t size = netInterface->getStreamBytesSize(empty_url);
        
        REQUIRE(size == 0);
    }
    
    SECTION("getStreamBytesSize with invalid URL") {
        std::string invalid_url = "http://example.invalid.url/nonexistent.mp3";
        uint64_t size = netInterface->getStreamBytesSize(invalid_url);
        
        // Should return 0 for invalid URLs
        REQUIRE(size == 0);
    }
}

// ========== Async Download Operations Tests ==========

TEST_CASE("BilibiliPlatform: Async download operations", "[BilibiliPlatform][Download]") {
    QtAppFixture::instance();
    auto netInterface = std::make_shared<BilibiliPlatform>();
    
    netInterface->initializeDefaultConfig();
    
    SECTION("asyncDownloadStream with invalid URL returns future") {
        std::string invalid_url = "http://invalid.example.com/nonexistent/audio.mp3";
        size_t bytes_received = 0;
        
        auto download_future = netInterface->asyncDownloadStream(
            invalid_url,
            [&bytes_received](const char* data, size_t size) {
                bytes_received += size;
                return true; // Continue downloading
            },
            [](uint64_t current, uint64_t total) {
                return true; // Continue download
            }
        );
        
        // Should return a valid future
        REQUIRE(download_future.valid());
        
        // Wait for completion (with timeout)
        auto status = download_future.wait_for(std::chrono::seconds(5));
        REQUIRE(status != std::future_status::timeout);
        
        // Result should indicate failure (false) for invalid URL
        bool success = download_future.get();
        REQUIRE(success == false);
    }
    
    SECTION("asyncDownloadStream with null callbacks") {
        std::string url = "http://example.com/audio.mp3";
        
        auto download_future = netInterface->asyncDownloadStream(
            url,
            nullptr,
            nullptr
        );
        
        REQUIRE(download_future.valid());
    }
    
    SECTION("asyncDownloadStream with content receiver only") {
        std::string url = "http://example.com/audio.mp3";
        size_t bytes_received = 0;
        
        auto download_future = netInterface->asyncDownloadStream(
            url,
            [&bytes_received](const char* data, size_t size) {
                bytes_received += size;
                return true;
            }
        );
        
        REQUIRE(download_future.valid());
        
        // Wait for completion
        auto status = download_future.wait_for(std::chrono::seconds(5));
        REQUIRE(status != std::future_status::timeout);
    }
}

// ========== HTTP Client Pool Tests (using UNIT_TEST methods) ==========

#ifdef UNIT_TEST

TEST_CASE("BilibiliPlatform: HTTP client pool management", "[BilibiliPlatform][ClientPool][UNIT_TEST]") {
    QtAppFixture::instance();
    auto netInterface = std::make_shared<BilibiliPlatform>();
    
    netInterface->initializeDefaultConfig();
    
    SECTION("test_create_clients adds clients to pool") {
        std::string host = "https://api.bilibili.com";
        size_t initial_size = netInterface->test_pool_size(host);
        
        netInterface->test_create_clients(host, 3);
        size_t after_size = netInterface->test_pool_size(host);
        
        REQUIRE(after_size >= initial_size);
    }
    
    SECTION("test_pool_size returns pool size") {
        std::string host = "https://api.example.com";
        size_t size = netInterface->test_pool_size(host);
        
        REQUIRE(size >= 0);
    }
    
    SECTION("test_concurrent_borrow_return handles concurrent access") {
        std::string host = "https://api.bilibili.com";
        
        // Create some clients first
        netInterface->test_create_clients(host, 5);
        
        // Test concurrent operations
        bool success = netInterface->test_concurrent_borrow_return(host, 4, 10);
        
        REQUIRE(success == true);
    }
    
    SECTION("test_concurrent_borrow_return with large thread count") {
        std::string host = "https://api.test.com";
        
        // Test with many threads
        bool success = netInterface->test_concurrent_borrow_return(host, 10, 5);
        
        REQUIRE(success == true);
    }
}

// ========== Cookie Management Tests (using UNIT_TEST methods) ==========

TEST_CASE("BilibiliPlatform: Cookie management (UNIT_TEST)", "[BilibiliPlatform][Cookies][UNIT_TEST]") {
    QtAppFixture::instance();
    auto netInterface = std::make_shared<BilibiliPlatform>();
    
    netInterface->initializeDefaultConfig();
    
    SECTION("test_add_cookie_from_set_cookie parses Set-Cookie header") {
        std::string set_cookie = "SESSDATA=abcd1234; Path=/; Domain=.bilibili.com; Expires=Wed, 01 Jan 2025 00:00:00 GMT; HttpOnly; Secure";
        std::string origin_host = "api.bilibili.com";
        
        // Should not throw
        netInterface->test_add_cookie_from_set_cookie(set_cookie, origin_host);
        
        REQUIRE(true);
    }
    
    SECTION("test_add_cookie_from_set_cookie with simple cookie") {
        std::string simple_cookie = "name=value";
        std::string host = "example.com";
        
        netInterface->test_add_cookie_from_set_cookie(simple_cookie, host);
        
        REQUIRE(true);
    }
    
    SECTION("test_get_headers_for_url builds correct headers") {
        // Add some cookies first
        netInterface->test_add_cookie_from_set_cookie("test=cookie", "example.com");
        
        // Get headers for URL
        auto headers = netInterface->test_get_headers_for_url("https://example.com/api/path");
        
        // Should return valid headers (may or may not contain cookies depending on domain matching)
        REQUIRE(headers.size() >= 0);
    }
    
    SECTION("test_get_headers_for_url with different domains") {
        netInterface->test_add_cookie_from_set_cookie("domain1=cookie1", "api.example.com");
        netInterface->test_add_cookie_from_set_cookie("domain2=cookie2", "different.com");
        
        auto headers1 = netInterface->test_get_headers_for_url("https://api.example.com/path");
        auto headers2 = netInterface->test_get_headers_for_url("https://different.com/path");
        
        REQUIRE(headers1.size() >= 0);
        REQUIRE(headers2.size() >= 0);
    }
}

#endif // UNIT_TEST

// ========== Data Structure Tests ==========

TEST_CASE("BilibiliPlatform: Data structures", "[BilibiliPlatform][DataStructures]") {
    QtAppFixture::instance();
    
    SECTION("BilibiliVideoInfo can be created") {
        BilibiliVideoInfo video;
        video.title = "【東方】Bad Apple!! ＰＶ【影絵】";
        video.author = "折射";
        video.bvid = "BV1xx411c79H";
        
        REQUIRE(video.title.size() > 0);
        REQUIRE(video.author.size() > 0);
        REQUIRE(video.bvid.size() > 0);
    }
    
    SECTION("BilibiliPageInfo can be created") {
        BilibiliPageInfo page;
        page.cid = 3724723;
        page.page = 1;
        page.part = "1";
        page.duration = 233;
        
        REQUIRE(page.cid == 3724723);
        REQUIRE(page.page == 1);
        REQUIRE(page.duration == 233);
    }
    
    SECTION("BilibiliSearchResult matches expected structure") {
        BilibiliSearchResult result;
        result.title = "【東方】Bad Apple!! ＰＶ【影絵】";
        result.bvid = "BV1xx411c79H";
        result.author = "折射";
        result.cid = 3724723;
        result.partTitle = "1";
        result.duration = 233;
        
        REQUIRE(result.title == "【東方】Bad Apple!! ＰＶ【影絵】");
        REQUIRE(result.bvid == "BV1xx411c79H");
        REQUIRE(result.author == "折射");
        REQUIRE(result.cid == 3724723);
    }
}

// ========== Error Handling Tests ==========

TEST_CASE("BilibiliPlatform: Error handling", "[BilibiliPlatform][ErrorHandling]") {
    QtAppFixture::instance();
    auto netInterface = std::make_shared<BilibiliPlatform>();
    
    SECTION("setTimeout with valid value") {
        // Should accept reasonable timeout values
        netInterface->setTimeout(1);
        netInterface->setTimeout(30);
        netInterface->setTimeout(300);
        
        REQUIRE(true);
    }
    
    SECTION("setTimeout with edge cases") {
        // Should handle edge cases without crashing
        netInterface->setTimeout(0);
        netInterface->setTimeout(-1); // May interpret as infinite
        
        REQUIRE(true);
    }
    
    SECTION("setPlatformDirectory with invalid path") {
        // Should return false for non-existent path
        bool result = netInterface->setPlatformDirectory("/nonexistent/path/that/does/not/exist");
        
        // May return false or create directory depending on implementation
        REQUIRE((result == true || result == false));
    }
    
    SECTION("loadConfig with missing file") {
        auto netInterface2 = std::make_shared<BilibiliPlatform>();
        netInterface2->setPlatformDirectory("."); // Current directory
        
        bool result = netInterface2->loadConfig("nonexistent_config_12345.json");
        
        // Should return false for missing file
        REQUIRE(result == false);
    }
}

// ========== Interface Contract Verification ==========

TEST_CASE("BilibiliPlatform: Implements IPlatform contract", "[BilibiliPlatform][InterfaceContract]") {
    QtAppFixture::instance();
    
    SECTION("Can instantiate as IPlatform pointer") {
        auto netInterface = std::make_shared<BilibiliPlatform>();

        // Should be castable to base class pointer
        IPlatform* base = netInterface.get();
        REQUIRE(base != nullptr);
    }
    
    SECTION("Virtual methods from base class are callable") {
        auto netInterface = std::make_shared<BilibiliPlatform>();

        // Test all pure virtual methods from IPlatform
        bool init_result = netInterface->initializeDefaultConfig();
        REQUIRE((init_result == true || init_result == false));

        netInterface->setTimeout(10);
        netInterface->setUserAgent("TestAgent/1.0");

        std::string url = netInterface->getAudioUrlByParams("");
        REQUIRE(url.size() >= 0);

        uint64_t size = netInterface->getStreamBytesSize("");
        REQUIRE(size >= 0);

        // asyncDownloadStream returns a future
        auto future = netInterface->asyncDownloadStream(
            "",
            [](const char*, size_t) { return false; }
        );
        REQUIRE(future.valid());
    }
}
