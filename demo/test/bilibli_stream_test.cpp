#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif
#include "../src/ffmpeg_decoder.h"
#include "../src/wasapi_audio_player.h"
#include "../src/bili_network_interface.h"
#include "../src/streaming_audio_buffer.h"

#include <iostream>
#include <chrono>
#include <sstream>
#include <thread>
#include <iomanip>
#include <atomic>
#include <mutex>
#include <condition_variable>

// Global streaming state
WASAPIAudioPlayer* g_audio_player = nullptr;
std::atomic<bool> g_download_complete{false};
std::atomic<bool> g_playback_started{false};
std::mutex g_buffer_mutex;
std::condition_variable g_buffer_cv;

void testAudioCallback(const uint8_t* data, int size, int sample_rate, int channels) {
    static int frame_count = 0;
    static int total_bytes = 0;
    static auto start_time = std::chrono::steady_clock::now();
    
    frame_count++;
    total_bytes += size;
    
    // Validate input data
    if (!data || size <= 0) {
        std::cout << "Warning: Invalid audio data in frame " << frame_count << std::endl;
        return;
    }
    
    // Play audio data in real-time
    if (g_audio_player && g_audio_player->isInitialized()) {
        g_audio_player->playAudioData(data, size);
    } else {
        std::cout << "Warning: Audio player not available for frame " << frame_count << std::endl;
    }
    
    if (frame_count % 500 == 0) { // Print every 500 frames to reduce spam during real-time playback
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        
        std::cout << "♪ Playing audio frame " << frame_count 
                  << " (" << size << " bytes, " 
                  << sample_rate << " Hz, " 
                  << channels << " channels, "
                  << "total: " << (total_bytes / 1024) << "KB, "
                  << "time: " << (duration / 1000.0) << "s)" << std::endl;
    }
}

const char* title = "噓月";

int main()
{
    std::cout << "=== Bilibili Network Stream Test ===" << std::endl;
    
    BilibiliNetworkInterface bili;
    bili.loadConfig("config.json");
    std::cout << "Attempting to connect to Bilibili API..." << std::endl;
    bool ret = bili.connect();
    if (!ret) {
        std::cout << "❌ Failed to connect to Bilibili API" << std::endl;
        std::cout << "📝 This is likely due to HTTPS/SSL support not being available in httplib." << std::endl;
        std::cout << "💡 Possible solutions:" << std::endl;
        std::cout << "   1. Ensure httplib is compiled with OpenSSL support" << std::endl;
        std::cout << "   2. Check if HTTPLIB_OPENSSL_SUPPORT is defined" << std::endl;
        std::cout << "   3. Verify OpenSSL libraries are properly linked" << std::endl;
        return -1;
    }
    
    std::cout << "Connected to Bilibili API successfully!" << std::endl;

    auto videos = bili.searchByTitle(title);
    if (videos.empty()) {
        std::cout << "No videos found for title: " << title << std::endl;
        return -1;
    }
    
    std::cout << "Found " << videos.size() << " videos:" << std::endl;
    for (size_t i = 0; i < videos.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << videos[i].title 
                  << " (BVID: " << videos[i].bvid << ")" << std::endl;
    }
    
    std::cout << "Select a video (1-" << videos.size() << "): ";
    int page_index = 0;
    std::cin >> page_index;
    page_index--; // Convert to 0-based index
    
    if (page_index < 0 || page_index >= static_cast<int>(videos.size())) {
        std::cout << "Invalid selection: " << (page_index + 1) << std::endl;
        return -1;
    }
    
    std::cout << "Selected: " << videos[page_index].title << std::endl;
    
    auto pages = bili.getPagesCid(videos[page_index].bvid);
    if (pages.empty()) {
        std::cout << "No pages found for video: " << videos[page_index].title << std::endl;
        return -1;
    }
    
    std::cout << "Getting audio link..." << std::endl;
    std::string audio_url = bili.getAudioLink(videos[page_index].bvid, pages[0].cid);
    if (audio_url.empty()) {
        std::cout << "Failed to get audio link for video: " << videos[page_index].title << std::endl;
        return -1;
    }  
    
    std::cout << "Audio URL: " << audio_url << std::endl;
    
    // Create streaming buffer (5MB buffer for smooth streaming)
    const size_t buffer_size = 5 * 1024 * 1024;
    const size_t min_buffer_for_playback = 256 * 1024; // Start playback with 256KB buffered
    auto streaming_buffer = std::make_shared<StreamingAudioBuffer>(buffer_size);
    
    // Download monitoring variables
    size_t total_downloaded = 0;
    auto download_start_time = std::chrono::steady_clock::now();
    const size_t rate_limit_bytes_per_sec = 100 * 1024; // 100 KB/s rate limit
    auto last_chunk_time = std::chrono::steady_clock::now();
    
    std::cout << "🚀 Starting real-time streaming with " << (rate_limit_bytes_per_sec / 1024) 
              << " KB/s rate limit..." << std::endl;
    std::cout << "📊 Buffer size: " << (buffer_size / 1024) << " KB, Start threshold: " 
              << (min_buffer_for_playback / 1024) << " KB" << std::endl;
    
    uint64_t ui64ExpectSize = bili.getStreamBytesSize(audio_url);
    std::cout << "🔍 Expected audio stream size: " << (ui64ExpectSize / 1024) << " KB" << std::endl;
    // Start download in background thread
    auto download_future = bili.asyncDownloadStream(audio_url, 
        [&](const char* data, size_t size) -> bool {
            // Rate limiting: delay if downloading too fast
            auto now = std::chrono::steady_clock::now();
            auto time_since_last_chunk = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_chunk_time).count();
            
            double expected_time_ms = (double(size) / rate_limit_bytes_per_sec) * 1000.0;
            if (time_since_last_chunk < expected_time_ms) {
                int delay_ms = int(expected_time_ms - time_since_last_chunk);
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }
            
            // Write to streaming buffer
            streaming_buffer->write(data, size);
            total_downloaded += size;
            last_chunk_time = std::chrono::steady_clock::now();
            
            // Calculate current download speed
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(last_chunk_time - download_start_time).count();
            if (elapsed > 0) {
                double speed_kbps = (double(total_downloaded) / 1024.0) / (elapsed / 1000.0);
                
                // Print download progress every second
                static auto last_print_time = std::chrono::steady_clock::now();
                auto now_print = std::chrono::steady_clock::now();
                auto print_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now_print - last_print_time).count();
                if (print_elapsed >= 1000) {
                    std::cout << "📥 Downloaded: " << (total_downloaded / 1024) << " KB"
                            << " | Speed: " << std::fixed << std::setprecision(1) << speed_kbps << " KB/s"
                            << " | Chunk: " << size << " bytes" << std::endl;
                    last_print_time = now_print;
                }
            }
            
            return true;
        }
    );
    
    std::cout << "⏳ Buffering audio data..." << std::endl;
    
    // Wait for minimum buffer before starting playback
    while (!streaming_buffer->hasEnoughData(min_buffer_for_playback) && 
           download_future.wait_for(std::chrono::milliseconds(1000)) != std::future_status::ready) {
        std::cout << " ⏳ Waiting for enough buffer: "
                  << (streaming_buffer->available() / 1024) 
                  << "/" << (min_buffer_for_playback / 1024) << " KB" << std::endl;
    }
    
    if (streaming_buffer->hasEnoughData(min_buffer_for_playback)) {
        std::cout << "✅ Sufficient buffer available! Starting real-time playback..." << std::endl;
        
        // Create streaming input for FFmpeg decoder
        auto streaming_input = std::make_shared<StreamingInputStream>(streaming_buffer.get());
        
        // Create decoder outside the thread so it stays alive
        auto decoder = std::make_shared<FFmpegStreamDecoder>();
        decoder->setAudioCallback(testAudioCallback);
        decoder->setStreamExpectedSize(ui64ExpectSize);
        
        std::cout << "🎵 Opening streaming audio for decoding..." << std::endl;
        if (!decoder->openStream(streaming_input)) {
            std::cout << "❌ Failed to open streaming audio for decoding" << std::endl;
            return 1;
        }
        
        // Get the actual audio format from the decoder
        AudioFormat audio_format = decoder->getAudioFormat();
        std::cout << "🎶 Detected Audio Format:" << std::endl;
        std::cout << "  Sample Rate: " << audio_format.sample_rate << " Hz" << std::endl;
        std::cout << "  Channels: " << audio_format.channels << std::endl;
        std::cout << "  Bits per sample: " << audio_format.bits_per_sample << std::endl;
        std::cout << "  Codec: " << decoder->getCodecName() << std::endl;

        // Initialize WASAPI audio player with detected format
        auto audio_player = std::make_shared<WASAPIAudioPlayer>(audio_format.sample_rate, audio_format.channels, audio_format.bits_per_sample);
        g_audio_player = audio_player.get();
        
        if (!audio_player->isInitialized()) {
            std::cout << "❌ Failed to initialize WASAPI audio player" << std::endl;
            return 1;
        }
        
        std::cout << "✅ WASAPI audio player started successfully!" << std::endl;

        // Start playback in a separate thread
        std::thread playback_thread([decoder, audio_player]() {
            std::cout << "🎵 Starting real-time streaming playback..." << std::endl;
            g_playback_started = true;
            
            if (!decoder->play()) {
                std::cout << "❌ Failed to start streaming playback" << std::endl;
                g_playback_started = false;
                return;
            }
            
            std::cout << "✅ Streaming playback started successfully!" << std::endl;
            
            // Keep the decoder and audio player alive until stopped
            while (g_playback_started) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            std::cout << "✅ Streaming playback done!" << std::endl;
            
            decoder->stop();
        });
        
        // Wait for playback to start
        while (!g_playback_started && playback_thread.joinable()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (g_playback_started) {
            std::cout << "🔄 Streaming in progress..." << std::endl;
            std::cout << "📊 Download continues while playing. Press Enter to stop..." << std::endl;
            
            // Show streaming status while playing
            std::thread status_thread([&]() {
                while (g_playback_started) {  // Continue while playback is active, regardless of download status
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    if (g_playback_started) {
                        std::string status = g_download_complete ? "✅ Complete" : "⬇️ Downloading";
                        std::cout << "📊 " << status << " | Buffer: " << (streaming_buffer->available() / 1024) << " KB"
                                 << " | Total: " << (streaming_buffer->totalWritten() / 1024) << " KB" << std::endl;
                    }
                }
            });
            
            // Wait for download to complete in background while playing
            std::thread download_monitor([&]() {
                bool download_success = download_future.get();
                streaming_buffer->setDownloadComplete();
                g_download_complete = true;
                if (download_success) {
                    std::cout << "📥 Download completed in background!" << std::endl;
                } else {
                    std::cout << "❌ Download failed!" << std::endl;
                }
            });
            
            std::cin.ignore(); // Clear any remaining input
            while (g_playback_started) {
                char command = std::cin.get();    // Wait for user input
                std::cout << "Command received: " << command << std::endl;

                // Handle user commands
                switch (command) {
                    case 'b':
                        decoder->play();
                        g_audio_player->start();
                        std::cout << "▶️ Resumed playback" << std::endl;
                        break;
                    case 'p':
                        decoder->pause();
                        g_audio_player->pause();
                        std::cout << "⏸ Paused playback" << std::endl;
                        break;
                    case 's':
                        decoder->stop();
                        g_audio_player->stop();
                        g_playback_started = false;
                        std::cout << "⏹ Stopped playback" << std::endl;
                        break;
                    case 'e':
                        decoder->stop();
                        g_audio_player->stop(); 
                        g_playback_started = false;
                        std::cout << "⏹ Stopped playback" << std::endl;
                        break;
                }
            }
            g_playback_started = false;
            if (status_thread.joinable()) status_thread.join();
            if (download_monitor.joinable()) download_monitor.join();
        }
        
        if (playback_thread.joinable()) {
            playback_thread.join();
        }
    } else {
        // If we couldn't start playback, still wait for download to complete
        bool download_success = download_future.get();
        streaming_buffer->setDownloadComplete();
        g_download_complete = true;
    }
    
    std::cout << "✅ Streaming session completed!" << std::endl;
    std::cout << "📊 Final stats - Total downloaded: " << (streaming_buffer->totalWritten() / 1024) << " KB" << std::endl;
    
    g_audio_player = nullptr;

    return 0;
}