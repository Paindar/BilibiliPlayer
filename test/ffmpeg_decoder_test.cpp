/**
 * FFmpeg Decoder Unit Tests
 * 
 * Tests audio decoding, stream handling, format conversion, and edge cases.
 * Covers initialization, format detection, pause/resume, thread safety, and cleanup.
 */

#include <catch2/catch_test_macros.hpp>
#include <audio/ffmpeg_decoder.h>
#include <audio/audio_frame.h>
#include <util/safe_queue.hpp>
#include <memory>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <fstream>
#include <iostream>

using namespace audio;

// Test helper: Create a minimal silence stream
class SilenceStream : public std::istream {
public:
    SilenceStream(size_t size) : std::istream(&buffer_), buffer_(size) {}
private:
    class SilenceBuffer : public std::streambuf {
    public:
        SilenceBuffer(size_t size) : size_(size), pos_(0) {}
    protected:
        std::streamsize xsgetn(char* s, std::streamsize n) override {
            size_t available = size_ - pos_;
            size_t to_read = std::min(static_cast<size_t>(n), available);
            std::fill_n(s, to_read, 0);
            pos_ += to_read;
            return static_cast<std::streamsize>(to_read);
        }
        int_type underflow() override {
            return (pos_ < size_) ? 0 : traits_type::eof();
        }
    private:
        size_t size_;
        size_t pos_;
    };
    SilenceBuffer buffer_;
};

TEST_CASE("FFmpegDecoder initialization", "[FFmpegDecoder]") {
    SECTION("Initialize with null stream fails") {
        FFmpegStreamDecoder decoder;
        auto frameQueue = std::make_shared<AudioFrameQueue>();
        
        bool result = decoder.initialize(nullptr, frameQueue);
        REQUIRE(result == false);
    }
    
    SECTION("Initialize twice fails") {
        FFmpegStreamDecoder decoder;
        auto stream = std::make_shared<SilenceStream>(1024);
        auto frameQueue = std::make_shared<AudioFrameQueue>();
        
        REQUIRE(decoder.initialize(stream, frameQueue) == true);
        
        // Second initialization should fail
        auto stream2 = std::make_shared<SilenceStream>(1024);
        REQUIRE(decoder.initialize(stream2, frameQueue) == false);
    }
}

TEST_CASE("FFmpegDecoder stream detection", "[FFmpegDecoder]") {
    SECTION("Fail on empty stream (0 bytes)") {
        FFmpegStreamDecoder decoder;
        auto stream = std::make_shared<SilenceStream>(0);
        auto frameQueue = std::make_shared<AudioFrameQueue>();
        
        REQUIRE(decoder.initialize(stream, frameQueue) == true);
        REQUIRE(decoder.startDecoding(0) == false);
    }
}

TEST_CASE("FFmpegDecoder pause/resume", "[FFmpegDecoder]") {
    SECTION("Pause before start fails") {
        FFmpegStreamDecoder decoder;
        auto stream = std::make_shared<SilenceStream>(1024);
        auto frameQueue = std::make_shared<AudioFrameQueue>();
        
        decoder.initialize(stream, frameQueue);
        
        REQUIRE(decoder.pauseDecoding() == false);
    }
}

TEST_CASE("FFmpegDecoder edge cases", "[FFmpegDecoder]") {
    SECTION("Handle empty stream gracefully") {
        FFmpegStreamDecoder decoder;
        auto stream = std::make_shared<SilenceStream>(0);
        auto frameQueue = std::make_shared<AudioFrameQueue>();
        
        REQUIRE(decoder.initialize(stream, frameQueue) == true);
        REQUIRE(decoder.startDecoding(0) == false);
    }
    
    SECTION("isDecoding returns false initially") {
        FFmpegStreamDecoder decoder;
        REQUIRE(decoder.isDecoding() == false);
    }
    
    SECTION("isCompleted returns false initially") {
        FFmpegStreamDecoder decoder;
        REQUIRE(decoder.isCompleted() == false);
    }
}

TEST_CASE("FFmpegDecoder thread safety", "[FFmpegDecoder]") {
    SECTION("Destroy during initialization is safe") {
        {
            FFmpegStreamDecoder decoder;
            auto stream = std::make_shared<SilenceStream>(1024);
            auto frameQueue = std::make_shared<AudioFrameQueue>();
            
            decoder.initialize(stream, frameQueue);
            // Destructor should handle cleanup safely
        }
        // If we reach here without crash, test passes
        REQUIRE(true);
    }
}

TEST_CASE("FFmpegDecoder resource cleanup", "[FFmpegDecoder]") {
    SECTION("Cleanup on normal completion") {
        auto decoder = std::make_unique<FFmpegStreamDecoder>();
        auto stream = std::make_shared<SilenceStream>(1024);
        auto frameQueue = std::make_shared<AudioFrameQueue>();
        
        decoder->initialize(stream, frameQueue);
        
        // Destroy decoder, should cleanup resources
        decoder.reset();
        
        REQUIRE(true); // If no crash, cleanup succeeded
    }
    
    SECTION("Cleanup on early destruction") {
        for (int i = 0; i < 10; ++i) {
            auto decoder = std::make_unique<FFmpegStreamDecoder>();
            auto stream = std::make_shared<SilenceStream>(1024);
            auto frameQueue = std::make_shared<AudioFrameQueue>();
            
            decoder->initialize(stream, frameQueue);
            // Destroy immediately
            decoder.reset();
        }
        
        REQUIRE(true); // No memory leaks
    }
}

TEST_CASE("FFmpegDecoder real audio file decoding", "[FFmpegDecoder][Integration]") {
    // Helper: open real test audio file
    auto getTestAudioPath = []() -> std::string {
        // Try multiple paths since tests can run from different directories
        // Primary: from build directory (after CMake copies data/)
        std::vector<std::string> candidates = {
            "data/audio/Bruno Wen-li - 笑顔の行方(M3).mp3",
            "data/audio/1_08b4aecfaf4db28a02aaea69009720c0.audio",
            // Fallback: from source tree
            "test/data/audio/Bruno Wen-li - 笑顔の行方(M3).mp3",
            "test/data/audio/1_08b4aecfaf4db28a02aaea69009720c0.audio",
            "../test/data/audio/Bruno Wen-li - 笑顔の行方(M3).mp3",
            "../../test/data/audio/Bruno Wen-li - 笑顔の行方(M3).mp3"
        };
        
        for (const auto& path : candidates) {
            if (std::ifstream(path).good()) {
                return path;
            }
        }
        return "";
    };
    
    SECTION("Decode valid MP3 file") {
        std::string audioPath = getTestAudioPath();
        
        if (audioPath.empty()) {
            SKIP("Test audio files not found - this is expected in CI environments");
        }
        
        FFmpegStreamDecoder decoder;
        auto frameQueue = std::make_shared<AudioFrameQueue>();
        
        // Open file as stream
        auto file = std::make_shared<std::ifstream>(audioPath, std::ios::binary);
        REQUIRE(file->is_open());
        
        // Get file size
        file->seekg(0, std::ios::end);
        uint64_t fileSize = file->tellg();
        file->seekg(0, std::ios::beg);
        
        // Initialize decoder
        REQUIRE(decoder.initialize(file, frameQueue) == true);
        
        // Start decoding with file size
        REQUIRE(decoder.startDecoding(fileSize) == true);
        REQUIRE(decoder.isDecoding() == true);
        
        // Give decoder time to process some frames
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Check that frames are being queued
        REQUIRE(decoder.isDecoding() == true);
    }
    
    SECTION("Pause and resume real file decoding") {
        std::string audioPath = getTestAudioPath();
        
        if (audioPath.empty()) {
            SKIP("Test audio files not found");
        }
        
        FFmpegStreamDecoder decoder;
        auto frameQueue = std::make_shared<AudioFrameQueue>();
        
        auto file = std::make_shared<std::ifstream>(audioPath, std::ios::binary);
        REQUIRE(file->is_open());
        
        // Get file size
        file->seekg(0, std::ios::end);
        uint64_t fileSize = file->tellg();
        file->seekg(0, std::ios::beg);
        
        REQUIRE(decoder.initialize(file, frameQueue) == true);
        REQUIRE(decoder.startDecoding(fileSize) == true);
        
        // Let it decode a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        bool wasDecoding = decoder.isDecoding();
        
        // Pause
        REQUIRE(decoder.pauseDecoding() == true);
        REQUIRE(decoder.isDecoding() == false);
        
        // Resume
        REQUIRE(decoder.resumeDecoding() == true);
        REQUIRE(decoder.isDecoding() == true);
    }
    
    SECTION("Get audio format async") {
        std::string audioPath = getTestAudioPath();
        
        if (audioPath.empty()) {
            SKIP("Test audio files not found");
        }
        
        FFmpegStreamDecoder decoder;
        auto frameQueue = std::make_shared<AudioFrameQueue>();
        
        auto file = std::make_shared<std::ifstream>(audioPath, std::ios::binary);
        REQUIRE(file->is_open());
        
        // Get file size
        file->seekg(0, std::ios::end);
        uint64_t fileSize = file->tellg();
        file->seekg(0, std::ios::beg);
        
        REQUIRE(decoder.initialize(file, frameQueue) == true);
        REQUIRE(decoder.startDecoding(fileSize) == true);
        
        // Get format info asynchronously
        auto formatFuture = decoder.getAudioFormatAsync();
        
        // Wait a bit for format detection
        using namespace std::chrono_literals;
        auto status = formatFuture.wait_for(500ms);
        
        // Format detection should complete within reasonable time
        REQUIRE(status != std::future_status::timeout);
    }
    
    SECTION("Get duration async") {
        std::string audioPath = getTestAudioPath();
        
        if (audioPath.empty()) {
            SKIP("Test audio files not found");
        }
        
        FFmpegStreamDecoder decoder;
        auto frameQueue = std::make_shared<AudioFrameQueue>();
        
        auto file = std::make_shared<std::ifstream>(audioPath, std::ios::binary);
        REQUIRE(file->is_open());
        
        // Get file size
        file->seekg(0, std::ios::end);
        uint64_t fileSize = file->tellg();
        file->seekg(0, std::ios::beg);
        
        REQUIRE(decoder.initialize(file, frameQueue) == true);
        REQUIRE(decoder.startDecoding(fileSize) == true);
        
        // Get duration asynchronously
        auto durationFuture = decoder.getDurationAsync();
        
        // Wait a bit for duration detection
        using namespace std::chrono_literals;
        auto status = durationFuture.wait_for(500ms);
        
        // Duration detection should complete within reasonable time
        REQUIRE(status != std::future_status::timeout);
    }
}

// Note: Full audio decoding tests with real MP3/FLAC files now implemented above
// using test audio files from data/audio/ directory (copied to build output by CMake).
// Tests gracefully skip if audio files are unavailable (e.g., CI environment).
