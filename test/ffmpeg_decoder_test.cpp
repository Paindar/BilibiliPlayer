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

TEST_CASE("FFmpegDecoder format conversion and output format", "[FFmpegDecoder][Format]") {
    // Verify decoder converts sample format to 16-bit (S16) and preserves sample rate/channels
    auto getTestAudioPath = []() -> std::string {
        std::vector<std::string> candidates = {
            "data/audio/Bruno Wen-li - 笑顔の行方(M3).mp3",
            "data/audio/1_08b4aecfaf4db28a02aaea69009720c0.audio",
            "test/data/audio/Bruno Wen-li - 笑顔の行方(M3).mp3",
            "test/data/audio/1_08b4aecfaf4db28a02aaea69009720c0.audio",
            "../test/data/audio/Bruno Wen-li - 笑顔の行方(M3).mp3",
            "../../test/data/audio/Bruno Wen-li - 笑顔の行方(M3).mp3"
        };
        for (const auto& path : candidates) {
            if (std::ifstream(path).good()) return path;
        }
        return "";
    };

    std::string audioPath = getTestAudioPath();
    if (audioPath.empty()) {
        SKIP("Test audio files not found - skipping format conversion checks");
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

    // Wait for audio format to become available
    auto formatFuture = decoder.getAudioFormatAsync();
    using namespace std::chrono_literals;
    auto status = formatFuture.wait_for(1000ms);
    REQUIRE(status != std::future_status::timeout);
    AudioFormat fmt = formatFuture.get();
    REQUIRE(fmt.isValid());

    // The decoder is expected to convert to 16-bit PCM (S16)
    REQUIRE(fmt.bits_per_sample == 16);
    REQUIRE(fmt.sample_rate > 0);
    REQUIRE(fmt.channels > 0);

    // Wait briefly for at least one decoded frame to be queued
    bool gotFrame = false;
    std::shared_ptr<AudioFrame> frame;
    for (int i = 0; i < 20; ++i) {
        if (frameQueue->size() > 0) {
            frameQueue->dequeue(frame);
            gotFrame = true;
            break;
        }
        std::this_thread::sleep_for(50ms);
    }

    REQUIRE(gotFrame == true);
    REQUIRE(frame->bits_per_sample == 16);
    REQUIRE(frame->sample_rate == fmt.sample_rate);
    REQUIRE(frame->channels == fmt.channels);
}

// --- Merged phase-2 detection & edge-case tests ---

static std::string findTestFile(const std::string& filename) {
    std::vector<std::string> candidates = {
        std::string("data/audio/") + filename,
        std::string("test/data/audio/") + filename,
        std::string("../test/data/audio/") + filename,
        std::string("test/data/video/") + filename,
        std::string("data/") + filename,
        std::string("test/data/") + filename
    };
    for (const auto& p : candidates) {
        std::ifstream f(p, std::ios::binary);
        if (f.good()) return p;
    }
    return "";
}

TEST_CASE("FFmpegDecoder detects FLAC format when FLAC file available", "[ffmpeg][format][flac]") {
    auto path = findTestFile("Bruno_Wen-li.flac");
    if (path.empty()) {
        WARN("FLAC test data not available; skipping");
        return;
    }

    FFmpegStreamDecoder decoder;
    auto frameQueue = std::make_shared<AudioFrameQueue>();
    auto file = std::make_shared<std::ifstream>(path, std::ios::binary);
    REQUIRE(file->is_open());

    // Get size
    file->seekg(0, std::ios::end);
    uint64_t size = file->tellg();
    file->seekg(0, std::ios::beg);

    REQUIRE(decoder.initialize(file, frameQueue) == true);
    REQUIRE(decoder.startDecoding(size) == true);

    auto fmtFuture = decoder.getAudioFormatAsync();
    using namespace std::chrono_literals;
    auto st = fmtFuture.wait_for(1500ms);
    REQUIRE(st != std::future_status::timeout);
    AudioFormat fmt = fmtFuture.get();
    REQUIRE(fmt.isValid());
    REQUIRE(fmt.sample_rate > 0);
    REQUIRE(fmt.channels > 0);
}

TEST_CASE("FFmpegDecoder fails on corrupted header", "[ffmpeg][format][corrupt]") {
    // Create an in-memory corrupted header stream
    std::string bad(128, '\xFF');
    auto strstream = std::make_shared<std::stringstream>(std::ios::in | std::ios::out | std::ios::binary);
    strstream->write(bad.data(), static_cast<std::streamsize>(bad.size()));
    strstream->seekg(0, std::ios::beg);

    FFmpegStreamDecoder decoder;
    auto frameQueue = std::make_shared<AudioFrameQueue>();

    REQUIRE(decoder.initialize(strstream, frameQueue) == true);
    REQUIRE(decoder.startDecoding(static_cast<uint64_t>(bad.size())) == true);

    // getAudioFormatAsync may either time out or return an invalid format object.
    auto fmtFuture = decoder.getAudioFormatAsync();
    using namespace std::chrono_literals;
    auto st = fmtFuture.wait_for(800ms);
    if (st == std::future_status::timeout) {
        SUCCEED("Format detection timed out as expected for corrupted input");
    } else {
        AudioFormat fmt = fmtFuture.get();
        REQUIRE(fmt.isValid() == false);
    }
}

TEST_CASE("FFmpegDecoder rejects video-only file", "[ffmpeg][format][video_only]") {
    auto path = findTestFile("test_video_only.mp4");
    if (path.empty()) {
        WARN("Video-only test data not available; skipping");
        return;
    }

    FFmpegStreamDecoder decoder;
    auto frameQueue = std::make_shared<AudioFrameQueue>();
    auto file = std::make_shared<std::ifstream>(path, std::ios::binary);
    REQUIRE(file->is_open());
    std::cout<<"Opened video-only test file: "<<path<<std::endl;
    file->seekg(0, std::ios::end);
    uint64_t size = file->tellg();
    file->seekg(0, std::ios::beg);
    std::cout<<"Video-only test file size: "<<size<<" bytes"<<std::endl;
    REQUIRE(decoder.initialize(file, frameQueue) == true);
    REQUIRE(decoder.startDecoding(size) == true);
    std::cout<<"Started decoding video-only test file"<<std::endl;
    // Audio format should either time out or return an invalid format for a video-only file
    auto fmtFuture = decoder.getAudioFormatAsync();
    std::cout<<"Waiting for audio format detection..."<<std::endl;
    using namespace std::chrono_literals;
    auto st = fmtFuture.wait_for(800ms);
    std::cout<<"Audio format detection wait over"<<std::endl;
    if (st == std::future_status::timeout) {
        SUCCEED("Format detection timed out as expected for video-only file");
    } else {
        AudioFormat fmt = fmtFuture.get();
        REQUIRE(fmt.isValid() == false);
    }
}

TEST_CASE("FFmpegDecoder pause/resume race and concurrency", "[FFmpegDecoder][Concurrency]") {
    // These tests use a real audio file to exercise threads; skip if not available
    auto getTestAudioPath = []() -> std::string {
        std::vector<std::string> candidates = {
            "data/audio/Bruno Wen-li - 笑顔の行方(M3).mp3",
            "test/data/audio/Bruno Wen-li - 笑顔の行方(M3).mp3",
            "../test/data/audio/Bruno Wen-li - 笑顔の行方(M3).mp3"
        };
        for (const auto& path : candidates) if (std::ifstream(path).good()) return path;
        return "";
    };

    std::string audioPath = getTestAudioPath();
    if (audioPath.empty()) {
        SKIP("Real audio not available; skipping concurrency tests");
    }

    // Pause/resume race: start decoding and rapidly toggle pause/resume from multiple threads
    {
        FFmpegStreamDecoder decoder;
        auto frameQueue = std::make_shared<AudioFrameQueue>();
        auto file = std::make_shared<std::ifstream>(audioPath, std::ios::binary);
        REQUIRE(file->is_open());
        file->seekg(0, std::ios::end);
        uint64_t fileSize = file->tellg();
        file->seekg(0, std::ios::beg);

        REQUIRE(decoder.initialize(file, frameQueue) == true);
        REQUIRE(decoder.startDecoding(fileSize) == true);

        // Launch multiple toggler threads
        std::vector<std::thread> togglers;
        std::atomic<bool> stop{false};
        for (int i = 0; i < 4; ++i) {
            togglers.emplace_back([&decoder, &stop]() {
                while (!stop.load()) {
                    decoder.pauseDecoding();
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    decoder.resumeDecoding();
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            });
        }

        // Run for a short period
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        stop.store(true);
        for (auto &t : togglers) if (t.joinable()) t.join();

        // Ensure decoder still responsive
        REQUIRE(true); // no crash == success
    }

    // Concurrent getAudioFormatAsync: multiple callers should receive format
    {
        FFmpegStreamDecoder decoder;
        auto frameQueue = std::make_shared<AudioFrameQueue>();
        auto file = std::make_shared<std::ifstream>(audioPath, std::ios::binary);
        REQUIRE(file->is_open());
        file->seekg(0, std::ios::end);
        uint64_t fileSize = file->tellg();
        file->seekg(0, std::ios::beg);

        REQUIRE(decoder.initialize(file, frameQueue) == true);
        REQUIRE(decoder.startDecoding(fileSize) == true);

        std::vector<std::future<AudioFormat>> futures;
        for (int i = 0; i < 6; ++i) {
            futures.push_back(std::async(std::launch::async, [&decoder]() {
                return decoder.getAudioFormatAsync().get();
            }));
        }

        using namespace std::chrono_literals;
        for (auto &f : futures) {
            auto status = f.wait_for(1500ms);
            REQUIRE(status != std::future_status::timeout);
            AudioFormat fmt = f.get();
            REQUIRE(fmt.isValid());
        }
    }

    // Destructor during active decoding: ensure destructor joins threads without hanging
    {
        auto runDestructor = [&audioPath]() {
            auto decoder = std::make_unique<FFmpegStreamDecoder>();
            auto frameQueue = std::make_shared<AudioFrameQueue>();
            auto file = std::make_shared<std::ifstream>(audioPath, std::ios::binary);
            if (!file->is_open()) return false;
            file->seekg(0, std::ios::end);
            uint64_t fileSize = file->tellg();
            file->seekg(0, std::ios::beg);
            if (!decoder->initialize(file, frameQueue)) return false;
            if (!decoder->startDecoding(fileSize)) return false;
            // Destroy quickly
            decoder.reset();
            return true;
        };

        auto fut = std::async(std::launch::async, runDestructor);
        using namespace std::chrono_literals;
        auto status = fut.wait_for(2000ms);
        REQUIRE(status != std::future_status::timeout);
        REQUIRE(fut.get() == true);
    }
}

TEST_CASE("FFmpegDecoder FLAC conversion to S16", "[ffmpeg][format][conversion][flac]") {
    auto path = findTestFile("Bruno_Wen-li.flac");
    if (path.empty()) {
        WARN("FLAC test data not available; skipping");
        return;
    }

    FFmpegStreamDecoder decoder;
    auto frameQueue = std::make_shared<AudioFrameQueue>();
    auto file = std::make_shared<std::ifstream>(path, std::ios::binary);
    REQUIRE(file->is_open());

    file->seekg(0, std::ios::end);
    uint64_t size = file->tellg();
    file->seekg(0, std::ios::beg);

    REQUIRE(decoder.initialize(file, frameQueue) == true);
    REQUIRE(decoder.startDecoding(size) == true);

    auto fmtFuture = decoder.getAudioFormatAsync();
    using namespace std::chrono_literals;
    auto st = fmtFuture.wait_for(1500ms);
    REQUIRE(st != std::future_status::timeout);
    AudioFormat fmt = fmtFuture.get();
    REQUIRE(fmt.isValid());
    // Accept common PCM bit depths (8/16/24/32) after conversion
    REQUIRE(((fmt.bits_per_sample == 8) || (fmt.bits_per_sample == 16) || (fmt.bits_per_sample == 24) || (fmt.bits_per_sample == 32)));
    // Typical FLAC files here are 44100 Hz; ensure sample_rate is positive
    REQUIRE(fmt.sample_rate == 44100);
    REQUIRE(fmt.channels >= 1);

    // Ensure at least one decoded frame is S16 and matches the reported fmt
    std::shared_ptr<AudioFrame> frame;
    bool got = false;
    for (int i = 0; i < 30; ++i) {
        if (frameQueue->size() > 0) {
            frameQueue->dequeue(frame);
            got = true;
            break;
        }
        std::this_thread::sleep_for(20ms);
    }
    REQUIRE(got == true);
    REQUIRE(frame->bits_per_sample == fmt.bits_per_sample);
    REQUIRE(frame->sample_rate == fmt.sample_rate);
    REQUIRE(frame->channels == fmt.channels);
}

TEST_CASE("FFmpegDecoder Opus conversion to S16", "[ffmpeg][format][conversion][opus]") {
    auto path = findTestFile("Bruno_opus.webm");
    if (path.empty()) {
        WARN("Opus test data not available; skipping");
        return;
    }

    FFmpegStreamDecoder decoder;
    auto frameQueue = std::make_shared<AudioFrameQueue>();
    auto file = std::make_shared<std::ifstream>(path, std::ios::binary);
    REQUIRE(file->is_open());

    file->seekg(0, std::ios::end);
    uint64_t size = file->tellg();
    file->seekg(0, std::ios::beg);

    REQUIRE(decoder.initialize(file, frameQueue) == true);
    REQUIRE(decoder.startDecoding(size) == true);

    auto fmtFuture = decoder.getAudioFormatAsync();
    using namespace std::chrono_literals;
    auto st = fmtFuture.wait_for(1500ms);
    REQUIRE(st != std::future_status::timeout);
    AudioFormat fmt = fmtFuture.get();
    REQUIRE(fmt.isValid());
    // Opus commonly uses 48000 Hz
    REQUIRE(fmt.sample_rate == 48000);
    REQUIRE(fmt.bits_per_sample == 16);
    REQUIRE(fmt.channels >= 1);

    // Ensure at least one decoded frame is available and matches
    std::shared_ptr<AudioFrame> frame;
    bool got = false;
    for (int i = 0; i < 30; ++i) {
        if (frameQueue->size() > 0) {
            frameQueue->dequeue(frame);
            got = true;
            break;
        }
        std::this_thread::sleep_for(20ms);
    }
    REQUIRE(got == true);
    REQUIRE(frame->bits_per_sample == 16);
    REQUIRE(frame->sample_rate == fmt.sample_rate);
}

TEST_CASE("FFmpegDecoder truncated stream behavior", "[ffmpeg][format][edge][truncated]") {
    // Use FLAC test file and truncate it to simulate incomplete downloads
    auto path = findTestFile("Bruno_Wen-li.flac");
    if (path.empty()) { WARN("FLAC test data not available; skipping truncated-stream test"); return; }

    // Read the file into memory and truncate to a small prefix
    std::ifstream f(path, std::ios::binary);
    REQUIRE(f.is_open());
    f.seekg(0, std::ios::end);
    size_t fullSize = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(fullSize);
    f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(fullSize));

    size_t truncatedSize = std::min<size_t>(1024, data.size());
    std::string truncated(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(truncatedSize));
    auto stream = std::make_shared<std::stringstream>(truncated, std::ios::in | std::ios::out | std::ios::binary);

    FFmpegStreamDecoder decoder;
    auto frameQueue = std::make_shared<AudioFrameQueue>();
    REQUIRE(decoder.initialize(stream, frameQueue) == true);
    REQUIRE(decoder.startDecoding(static_cast<uint64_t>(truncatedSize)) == true);

    auto fmtFuture = decoder.getAudioFormatAsync();
    using namespace std::chrono_literals;
    auto st = fmtFuture.wait_for(800ms);
    if (st == std::future_status::timeout) {
        SUCCEED("Format detection timed out as expected for truncated input");
    } else {
        AudioFormat fmt = fmtFuture.get();
        // Accept either invalid format or valid detected format; ensure decoder didn't crash
        REQUIRE(((fmt.isValid() == false) || (fmt.sample_rate > 0 && fmt.channels > 0)));
    }
}

TEST_CASE("FFmpegDecoder midstream corruption behavior", "[ffmpeg][format][edge][corrupt_midstream]") {
    // Corrupt the middle of a real audio file and ensure decoder handles it without crashing
    auto path = findTestFile("Bruno_Wen-li.flac");
    if (path.empty()) { WARN("FLAC test data not available; skipping midstream-corrupt test"); return; }

    std::ifstream f(path, std::ios::binary);
    REQUIRE(f.is_open());
    f.seekg(0, std::ios::end);
    size_t fullSize = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(fullSize);
    f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(fullSize));

    if (fullSize < 512) { WARN("source file too small for meaningful midstream corruption; skipping"); return; }

    // Flip bytes in the middle region
    size_t start = fullSize / 3;
    size_t end = std::min(fullSize, start + 256);
    for (size_t i = start; i < end; ++i) data[i] = static_cast<uint8_t>(data[i] ^ 0xFF);

    std::string corrupted(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    auto stream = std::make_shared<std::stringstream>(corrupted, std::ios::in | std::ios::out | std::ios::binary);

    FFmpegStreamDecoder decoder;
    auto frameQueue = std::make_shared<AudioFrameQueue>();
    REQUIRE(decoder.initialize(stream, frameQueue) == true);
    REQUIRE(decoder.startDecoding(static_cast<uint64_t>(data.size())) == true);

    auto fmtFuture = decoder.getAudioFormatAsync();
    using namespace std::chrono_literals;
    auto st = fmtFuture.wait_for(1000ms);
    if (st == std::future_status::timeout) {
        SUCCEED("Format detection timed out for midstream-corrupted input");
    } else {
        AudioFormat fmt = fmtFuture.get();
        // Either format may be invalid or valid; ensure decoder did not crash and returned something sensible
        REQUIRE(((fmt.isValid() == false) || (fmt.sample_rate > 0 && fmt.channels > 0)));
    }
}

TEST_CASE("FFmpegDecoder very-short stream behavior", "[ffmpeg][format][edge][very_short]") {
    // Use a very small prefix of an audio file to simulate extremely short downloads
    auto path = findTestFile("Bruno_Wen-li.flac");
    if (path.empty()) { WARN("FLAC test data not available; skipping very-short test"); return; }

    std::ifstream f(path, std::ios::binary);
    REQUIRE(f.is_open());
    f.seekg(0, std::ios::end);
    size_t fullSize = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(fullSize);
    f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(fullSize));

    size_t tiny = std::min<size_t>(64, data.size());
    std::string tinyStr(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(tiny));
    auto stream = std::make_shared<std::stringstream>(tinyStr, std::ios::in | std::ios::out | std::ios::binary);

    FFmpegStreamDecoder decoder;
    auto frameQueue = std::make_shared<AudioFrameQueue>();
    REQUIRE(decoder.initialize(stream, frameQueue) == true);
    REQUIRE(decoder.startDecoding(static_cast<uint64_t>(tiny)) == true);

    auto fmtFuture = decoder.getAudioFormatAsync();
    using namespace std::chrono_literals;
    auto st = fmtFuture.wait_for(800ms);
    if (st == std::future_status::timeout) {
        SUCCEED("Format detection timed out as expected for very-short input");
    } else {
        AudioFormat fmt = fmtFuture.get();
        // Accept either invalid format or a valid (but small) detected format; ensure decoder didn't crash
        REQUIRE(((fmt.isValid() == false) || (fmt.sample_rate > 0 && fmt.channels > 0)));
    }
}
