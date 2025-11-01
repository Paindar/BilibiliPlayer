#include "../src/bili_network_interface.h"
#include <iostream>
#include <iomanip>

// Print the search results for a given keyword
void printSearchResults(const std::string& keyword) {
    std::cout << "=== Bilibili Network Interface Test ===" << std::endl;
    
    BilibiliNetworkInterface bili;
    
    // Load configuration if available
    bili.loadConfig("config.json");
    
    std::cout << "Attempting to connect to Bilibili API..." << std::endl;
    bool connected = bili.connect();
    if (!connected) {
        std::cout << "❌ Failed to connect to Bilibili API" << std::endl;
        std::cout << "📝 This is likely due to HTTPS/SSL support not being available in httplib." << std::endl;
        std::cout << "💡 Possible solutions:" << std::endl;
        std::cout << "   1. Ensure httplib is compiled with OpenSSL support" << std::endl;
        std::cout << "   2. Check if HTTPLIB_OPENSSL_SUPPORT is defined" << std::endl;
        std::cout << "   3. Verify OpenSSL libraries are properly linked" << std::endl;
        return;
    }
    
    std::cout << "✅ Connected to Bilibili API successfully!" << std::endl;
    std::cout << std::endl;

    // Search for videos
    std::cout << "🔍 Searching for: \"" << keyword << "\"..." << std::endl;
    auto videos = bili.searchByTitle(keyword);
    
    if (videos.empty()) {
        std::cout << "❌ No videos found for keyword: " << keyword << std::endl;
        return;
    }
    
    std::cout << "✅ Found " << videos.size() << " videos:" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    for (size_t i = 0; i < videos.size() && i < 10; ++i) { // Show first 10 results
        std::cout << std::setw(3) << (i + 1) << ". Title: " << videos[i].title << std::endl;
        std::cout << "     BVID: " << videos[i].bvid << std::endl;
        std::cout << "     Author: " << videos[i].author << std::endl;
        if (!videos[i].description.empty()) {
            // Truncate description to avoid too much output
            std::string desc = videos[i].description;
            if (desc.length() > 100) {
                desc = desc.substr(0, 97) + "...";
            }
            std::cout << "     Description: " << desc << std::endl;
        }
        std::cout << std::endl;
    }
    
    // Test getting pages for the first video
    if (!videos.empty()) {
        std::cout << std::string(80, '=') << std::endl;
        std::cout << "🎬 Testing page information for first video..." << std::endl;
        std::cout << "Selected video: " << videos[0].title << std::endl;
        std::cout << "BVID: " << videos[0].bvid << std::endl;
        
        auto pages = bili.getPagesCid(videos[0].bvid);
        if (pages.empty()) {
            std::cout << "❌ No pages found for video: " << videos[0].title << std::endl;
        } else {
            std::cout << "✅ Found " << pages.size() << " page(s):" << std::endl;
            for (size_t i = 0; i < pages.size(); ++i) {
                std::cout << "  Page " << pages[i].page << ": " << pages[i].part 
                         << " (CID: " << pages[i].cid << ", Duration: " << pages[i].duration << "s)" << std::endl;
            }
            
            // Test getting audio link for the first page
            std::cout << std::endl;
            std::cout << "🎵 Testing audio link retrieval..." << std::endl;
            std::string audio_url = bili.getAudioLink(videos[0].bvid, pages[0].cid);
            if (audio_url.empty()) {
                std::cout << "❌ Failed to get audio link" << std::endl;
            } else {
                std::cout << "✅ Audio URL retrieved successfully!" << std::endl;
                std::cout << "Audio URL: " << audio_url.substr(0, 80) << "..." << std::endl;
                
                // Test getting stream size
                std::cout << std::endl;
                std::cout << "📊 Testing stream size retrieval..." << std::endl;
                uint64_t stream_size = bili.getStreamBytesSize(audio_url);
                if (stream_size > 0) {
                    std::cout << "✅ Stream size: " << (stream_size / 1024) << " KB" << std::endl;
                } else {
                    std::cout << "❌ Failed to get stream size or stream is empty" << std::endl;
                }
            }
        }
    }
    
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "✅ Bilibili Network Interface Test completed!" << std::endl;
}

int main() {
    std::string keyword;
    
    // Use default keyword if no input provided
    std::cout << "Enter search keyword (or press Enter for default '噓月'): ";
    std::getline(std::cin, keyword);
    
    if (keyword.empty()) {
        keyword = "噓月";
    }
    
    printSearchResults(keyword);
    
    return 0;
}