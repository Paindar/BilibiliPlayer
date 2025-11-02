#define UNIT_TEST
#include <catch2/catch_test_macros.hpp>
#include "network/bili_network_interface.h"

using namespace network;

TEST_CASE("Cookie jar parsing and selection", "[cookie_jar]") {
    BilibiliNetworkInterface iface;

    // Host-only cookie from origin https://www.bilibili.com
    iface.test_add_cookie_from_set_cookie("SID=abc123; Path=/; HttpOnly", "https://www.bilibili.com");
    // Domain cookie with leading dot
    iface.test_add_cookie_from_set_cookie("DID=xyz; Domain=.bilibili.com; Path=/; Max-Age=3600", "https://www.bilibili.com");
    // Secure cookie
    iface.test_add_cookie_from_set_cookie("SEC=1; Secure; Domain=bilibili.com; Path=/", "https://www.bilibili.com");

    // Request to www.bilibili.com should include SID, DID, SEC (https)
    auto h1 = iface.test_get_headers_for_url("https://www.bilibili.com/");
    REQUIRE(h1.find("Cookie") != h1.end());
    auto it1 = h1.find("Cookie");
    REQUIRE(it1 != h1.end());
    std::string c1 = it1->second;
    REQUIRE(c1.find("SID=abc123") != std::string::npos);
    REQUIRE(c1.find("DID=xyz") != std::string::npos);
    REQUIRE(c1.find("SEC=1") != std::string::npos);

    // Request to api.bilibili.com should include DID and SEC but not SID (host-only)
    auto h2 = iface.test_get_headers_for_url("https://api.bilibili.com/resource");
    REQUIRE(h2.find("Cookie") != h2.end());
    auto it2 = h2.find("Cookie");
    REQUIRE(it2 != h2.end());
    std::string c2 = it2->second;
    REQUIRE(c2.find("DID=xyz") != std::string::npos);
    REQUIRE(c2.find("SEC=1") != std::string::npos);
    REQUIRE(c2.find("SID=abc123") == std::string::npos);

    // HTTP request to api.bilibili.com should not include SEC (secure cookie)
    auto h3 = iface.test_get_headers_for_url("http://api.bilibili.com/resource");
    auto it3 = h3.find("Cookie");
    if (it3 != h3.end()) {
        std::string c3 = it3->second;
        REQUIRE(c3.find("SEC=1") == std::string::npos);
    }
}
