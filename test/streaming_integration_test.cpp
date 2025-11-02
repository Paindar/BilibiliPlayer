#include <catch2/catch_test_macros.hpp>

#include "stream/streaming_audio_buffer.h"
#include <thread>
#include <vector>
#include <chrono>
#include <array>

using namespace std::chrono_literals;

TEST_CASE("Streaming integration: producer writes and stream reads all data", "[streaming][integration]") {
    const size_t total_bytes = 64 * 1024; // 64KB
    const size_t chunk_size = 1024;
    auto buffer = std::make_shared<StreamingAudioBuffer>(32 * 1024); // 32KB circular buffer

    // Producer: writes predictable pattern
    std::thread producer([buffer, total_bytes, chunk_size]() {
        size_t written = 0;
        std::vector<char> chunk(chunk_size);
        while (written < total_bytes) {
            size_t to_write = std::min(chunk_size, total_bytes - written);
            for (size_t i = 0; i < to_write; ++i) {
                chunk[i] = static_cast<char>((written + i) & 0xFF);
            }
            buffer->write(chunk.data(), to_write);
            written += to_write;
            // simulate network jitter
            std::this_thread::sleep_for(2ms);
        }
        buffer->setDownloadComplete();
    });

    // Consumer: read via StreamingInputStream
    StreamingInputStream sstream(buffer);
    std::vector<uint8_t> received;
    received.reserve(total_bytes);

    std::array<char, 4096> read_buf;

    while (sstream.good()) {
        sstream.read(read_buf.data(), static_cast<std::streamsize>(read_buf.size()));
        std::streamsize n = sstream.gcount();
        if (n > 0) {
            received.insert(received.end(), read_buf.data(), read_buf.data() + n);
        } else {
            // If no data was read, but download not complete, wait briefly
            if (!buffer->isDownloadComplete()) std::this_thread::sleep_for(5ms);
            else break;
        }
    }

    producer.join();

    REQUIRE(received.size() == total_bytes);
    // verify content pattern
    for (size_t i = 0; i < total_bytes; ++i) {
        REQUIRE(received[i] == static_cast<uint8_t>(i & 0xFF));
    }
}

TEST_CASE("Streaming partial download: aborted mid-download is handled", "[streaming][partial]") {
    const size_t total_bytes = 16 * 1024; // 16KB
    const size_t chunk_size = 1024;
    auto buffer = std::make_shared<StreamingAudioBuffer>(8 * 1024); // 8KB circular buffer

    // Producer: writes only part of the total, then marks download complete
    std::thread producer([buffer, total_bytes, chunk_size]() {
        size_t written = 0;
        std::vector<char> chunk(chunk_size);
        // write only half the intended bytes, then abort
        size_t to_produce = total_bytes / 2;
        while (written < to_produce) {
            size_t to_write = std::min(chunk_size, to_produce - written);
            for (size_t i = 0; i < to_write; ++i) {
                chunk[i] = static_cast<char>((written + i) & 0xFF);
            }
            buffer->write(chunk.data(), to_write);
            written += to_write;
            std::this_thread::sleep_for(1ms);
        }
        // Simulate aborted download: no more data, but signal completion
        buffer->setDownloadComplete();
    });

    StreamingInputStream sstream(buffer);
    std::vector<uint8_t> received;
    received.reserve(total_bytes / 2);

    std::array<char, 2048> read_buf;
    while (sstream.good()) {
        sstream.read(read_buf.data(), static_cast<std::streamsize>(read_buf.size()));
        std::streamsize n = sstream.gcount();
        if (n > 0) {
            received.insert(received.end(), read_buf.data(), read_buf.data() + n);
        } else {
            if (!buffer->isDownloadComplete()) std::this_thread::sleep_for(2ms);
            else break;
        }
    }

    producer.join();

    REQUIRE(received.size() == total_bytes / 2);
    for (size_t i = 0; i < received.size(); ++i) {
        REQUIRE(received[i] == static_cast<uint8_t>(i & 0xFF));
    }
}

TEST_CASE("Streaming atomic commit (.part) simulation: data visible only after commit", "[streaming][atomic]") {
    const size_t total_bytes = 32 * 1024; // 32KB
    const size_t chunk_size = 1024;
    auto buffer = std::make_shared<StreamingAudioBuffer>(16 * 1024); // 16KB

    // Simulate download into a .part staging buffer that is committed atomically
    std::vector<char> staging;
    std::mutex staging_mutex;
    std::atomic<bool> committed{false};

    // Producer thread writes into staging and then commits once complete
    std::thread producer([&]() {
        size_t written = 0;
        while (written < total_bytes) {
            size_t to_write = std::min(chunk_size, total_bytes - written);
            std::vector<char> chunk(to_write);
            for (size_t i = 0; i < to_write; ++i) chunk[i] = static_cast<char>((written + i) & 0xFF);
            {
                std::lock_guard<std::mutex> lg(staging_mutex);
                staging.insert(staging.end(), chunk.begin(), chunk.end());
            }
            written += to_write;
            std::this_thread::sleep_for(1ms);
        }
        // Commit: transfer staging to streaming buffer in one atomic operation
        {
            std::lock_guard<std::mutex> lg(staging_mutex);
            if (!staging.empty()) {
                buffer->write(staging.data(), staging.size());
                staging.clear();
            }
        }
        committed = true;
        buffer->setDownloadComplete();
    });

    // Consumer: attempt to read before commit should block until commit
    StreamingInputStream sstream(buffer);
    std::vector<uint8_t> received;
    received.reserve(total_bytes);

    std::array<char, 4096> read_buf{};

    // Poll for data: before commit there should be no available data together with committed==true
    // Avoid using logical operators inside Catch2 assertions; decompose the check
    if (buffer->hasEnoughData(1)) {
        REQUIRE(committed.load() == false);
    }

    // After some time the producer will commit; read until EOF
    while (sstream.good()) {
        sstream.read(read_buf.data(), static_cast<std::streamsize>(read_buf.size()));
        std::streamsize n = sstream.gcount();
        if (n > 0) {
            received.insert(received.end(), read_buf.data(), read_buf.data() + n);
        } else {
            if (!buffer->isDownloadComplete()) std::this_thread::sleep_for(2ms);
            else break;
        }
    }

    producer.join();

    REQUIRE(committed == true);
    REQUIRE(received.size() == total_bytes);
    for (size_t i = 0; i < total_bytes; ++i) REQUIRE(received[i] == static_cast<uint8_t>(i & 0xFF));
}
