#define UNIT_TEST
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <chrono>
#include "network/bili_network_interface.h"

using namespace network;

TEST_CASE("HTTP client pool borrow/return basic", "[http_pool]") {
    BilibiliNetworkInterface iface;
    const std::string host = "https://example.com";

    REQUIRE(iface.test_pool_size(host) == 0);

    iface.test_create_clients(host, 3);
    REQUIRE(iface.test_pool_size(host) >= 3);

    bool ok = iface.test_concurrent_borrow_return(host, 8, 200);
    REQUIRE(ok == true);

    // pool size should remain stable
    REQUIRE(iface.test_pool_size(host) >= 3);
}
