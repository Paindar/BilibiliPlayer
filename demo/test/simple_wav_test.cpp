#include "../src/ffmpeg_decoder.h"
#include "../src/wasapi_audio_player.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <filesystem>

// Global audio player pointer for callback
// Note: In production code, consider using a proper callback system instead of global
WASAPIAudioPlayer* g_audio_player = nullptr;

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

int main() {
    std::cout << "=== FFmpeg Audio Decoder & WASAPI Player Test ===" << std::endl;
    
    // Available audio files
    std::vector<std::string> audio_files;
    // get all resource under ./wav
    for (const auto& entry : std::filesystem::directory_iterator("./wav")) {
        if (entry.is_regular_file()) {
            audio_files.push_back(entry.path().filename().string());
        }
    }
    if (audio_files.empty()) {
        std::cerr << "No audio files found in ./wav directory!" << std::endl;
        return 1;
    }
    // Display file selection menu
    std::cout << "\nAvailable audio files:" << std::endl;
    for (size_t i = 0; i < audio_files.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << audio_files[i] << std::endl;
    }
    
    // Get user selection
    std::cout << "\nSelect a file to play (1-" << audio_files.size() << "): ";
    int selection;
    std::cin >> selection;
    
    if (selection < 1 || selection > static_cast<int>(audio_files.size())) {
        std::cerr << "Invalid selection!" << std::endl;
        return 1;
    }
    
    std::string selected_file = audio_files[selection - 1];
    std::string wav_file_path = "../resource/wav/" + selected_file;
    
    std::cout << "\n=== Selected: " << selected_file << " ===" << std::endl;
    
    // Create decoder instance
    FFmpegStreamDecoder decoder;
    decoder.setAudioCallback(testAudioCallback);
    
    // Create file stream
    auto file_stream = std::make_shared<std::ifstream>(wav_file_path, std::ios::binary);
    
    if (!file_stream->is_open()) {
        std::cerr << "Error: Could not open selected audio file" << std::endl;
        return 1;
    }
    
    std::cout << "File opened successfully!" << std::endl;
    
    // Use the unified openStream interface
    if (decoder.openStream(file_stream)) {
        std::cout << "Stream opened successfully!" << std::endl;
        std::cout << "Duration: " << decoder.getDuration() << " seconds" << std::endl;
        std::cout << "Codec: " << decoder.getCodecName() << std::endl;
        
        // Get the actual audio format from the decoder
        AudioFormat audio_format = decoder.getAudioFormat();
        std::cout << "Detected Audio Format:" << std::endl;
        std::cout << "  Sample Rate: " << audio_format.sample_rate << " Hz" << std::endl;
        std::cout << "  Channels: " << audio_format.channels << std::endl;
        std::cout << "  Bits per sample: " << audio_format.bits_per_sample << std::endl;
        
        // Create WASAPI audio player with the actual detected format
        WASAPIAudioPlayer audio_player(audio_format.sample_rate, audio_format.channels, audio_format.bits_per_sample);
        g_audio_player = &audio_player;
        
        if (!audio_player.isInitialized()) {
            std::cerr << "Failed to initialize Windows audio system!" << std::endl;
            return 1;
        }
        
        // Start the audio player
        if (!audio_player.start()) {
            std::cerr << "Failed to start WASAPI audio player!" << std::endl;
            return 1;
        }
        
        std::cout << "✅ WASAPI audio player started successfully!" << std::endl;
        
        // Start playback
        if (decoder.play()) {
            std::cout << "Playback started!" << std::endl;
            
            // Play for 10 seconds
            for (int i = 0; i < 10; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::cout << "Playing... " << (i + 1) << "/10 seconds" << std::endl;
            }
            
            // Test volume control
            std::cout << "Testing volume control..." << std::endl;
            decoder.setVolume(0.5f);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            decoder.setVolume(0.2f);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            decoder.setVolume(1.0f);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            // Stop playback
            decoder.stop();
            std::cout << "Playback stopped." << std::endl;
        } else {
            std::cerr << "Error: Failed to start playback" << std::endl;
            return 1;
        }
    } else {
        std::cerr << "Error: Failed to open stream" << std::endl;
        return 1;
    }
    
    std::cout << "Test completed successfully!" << std::endl;
    std::cout << "Audio file \"" << selected_file << "\" played successfully!" << std::endl;
    
    g_audio_player = nullptr; // Reset global pointer
    
    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.ignore(); // Clear the newline from the selection input
    std::cin.get();
    
    return 0;
}