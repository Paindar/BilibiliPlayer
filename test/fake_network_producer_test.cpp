#include <future>
#include <thread>
#include <chrono>
#include <vector>
#include "stream/streaming_audio_buffer.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Simulated network producer to StreamingAudioBuffer") {
    auto buffer = std::make_shared<StreamingAudioBuffer>(32 * 1024); // 32KB buffer

    // Producer: simulates network chunks arriving every 5ms
    auto producer = std::async(std::launch::async, [buffer]() -> size_t {
        const size_t total = 128 * 1024; // 128KB total
        const size_t chunk = 4096;
        std::vector<uint8_t> data(chunk, 0xAA);
        size_t sent = 0;
        while (sent < total) {
            buffer->write(reinterpret_cast<const char*>(data.data()), data.size());
            sent += data.size();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        buffer->setDownloadComplete();
        return sent;
    });

    // Consumer: read until EOF
    auto consumer = std::async(std::launch::async, [buffer]() -> size_t {
        std::vector<uint8_t> out(16 * 1024);
        size_t total_read = 0;
        while (true) {
            size_t r = buffer->read(out.data(), out.size());
            if (r == 0 && buffer->isDownloadComplete()) break;
            total_read += r;
            // simulate decoding/playing delay
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return total_read;
    });

    size_t sent = producer.get();
    size_t got = consumer.get();

    REQUIRE(got == sent);
}
