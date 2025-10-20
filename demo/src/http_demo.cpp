#include <httplib.h>
#include <iostream>
#include <string>

int main() {
    std::cout << "=== cpp-httplib Demo ===" << std::endl;
    
    // Simple HTTP GET request
    std::cout << "\n1. Simple GET request:" << std::endl;
    httplib::Client cli("http://httpbin.org");
    
    auto res = cli.Get("/get");
    if (res && res->status == 200) {
        std::cout << "Status: " << res->status << std::endl;
        std::cout << "Response: " << res->body.substr(0, 200) << "..." << std::endl;
    } else {
        std::cout << "Failed to connect or bad status" << std::endl;
    }
    
    // GET with parameters
    std::cout << "\n2. GET with query parameters:" << std::endl;
    auto res2 = cli.Get("/get?name=BilibiliPlayer&version=1.0");
    if (res2 && res2->status == 200) {
        std::cout << "Status: " << res2->status << std::endl;
        std::cout << "Response contains our params: " << std::endl;
        std::cout << res2->body.substr(0, 300) << "..." << std::endl;
    }
    
    // POST request with JSON
    std::cout << "\n3. POST request with JSON:" << std::endl;
    httplib::Headers headers = {
        {"Content-Type", "application/json"}
    };
    
    std::string json_data = R"({
        "player": "BilibiliPlayer",
        "action": "test_connection",
        "timestamp": "2025-10-19"
    })";
    
    auto res3 = cli.Post("/post", headers, json_data, "application/json");
    if (res3 && res3->status == 200) {
        std::cout << "Status: " << res3->status << std::endl;
        std::cout << "POST Response: " << res3->body.substr(0, 200) << "..." << std::endl;
    }
    
    // Download example (for streaming later)
    std::cout << "\n4. Download with progress (simulation for video streaming):" << std::endl;
    auto res4 = cli.Get("/bytes/1024", [](uint64_t len, uint64_t total) {
        if (total > 0) {
            double progress = (double)len / total * 100;
            std::cout << "\rDownload progress: " << std::fixed << progress << "%" << std::flush;
        }
        return true; // Continue download
    });
    
    if (res4 && res4->status == 200) {
        std::cout << "\nDownload completed! Received " << res4->body.size() << " bytes" << std::endl;
    }
    
    // Test connection timeout and error handling
    std::cout << "\n5. Testing connection timeout:" << std::endl;
    httplib::Client slow_cli("http://httpbin.org");
    slow_cli.set_connection_timeout(2, 0); // 2 seconds timeout
    
    auto res5 = slow_cli.Get("/delay/5"); // This will timeout
    if (res5) {
        std::cout << "Unexpected success!" << std::endl;
    } else {
        std::cout << "Connection timed out as expected (good for error handling)" << std::endl;
    }
    
    std::cout << "\n=== Demo completed! ===" << std::endl;
    return 0;
}