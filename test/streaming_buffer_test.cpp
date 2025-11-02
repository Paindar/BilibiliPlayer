// Catch2-based concurrent producer/consumer test for StreamingAudioBuffer
#include <thread>
#include <vector>
#include <memory>
#include "stream/streaming_audio_buffer.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("StreamingAudioBuffer producer/consumer transfers data correctly") {
    const size_t total_size = 256 * 1024; // 256 KB
    const size_t chunk = 4096;
    auto buffer = std::make_shared<StreamingAudioBuffer>(64 * 1024); // 64 KB capacity

    std::vector<uint8_t> produced;
    produced.reserve(total_size);
    for (size_t i = 0; i < total_size; ++i) produced.push_back(static_cast<uint8_t>(i & 0xFF));

    std::vector<uint8_t> consumed;
    consumed.reserve(total_size);

    // Producer thread
    auto producer = std::thread([&]() {
        size_t offset = 0;
        while (offset < total_size) {
            size_t sz = std::min(chunk, total_size - offset);
            buffer->write(reinterpret_cast<const char*>(produced.data() + offset), sz);
            offset += sz;
            // simulate network jitter
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        buffer->setDownloadComplete();
    });

    // Consumer thread
    auto consumer = std::thread([&]() {
        std::vector<uint8_t> tmp(1024);
        while (true) {
            size_t r = buffer->read(tmp.data(), tmp.size());
            if (r > 0) {
                consumed.insert(consumed.end(), tmp.begin(), tmp.begin() + r);
            } else {
                // No data returned
                if (buffer->isDownloadComplete()) break;
                // else, retry
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });

    producer.join();
    consumer.join();

    CHECK(consumed.size() == produced.size());
    if (consumed.size() == produced.size()) {
        for (size_t i = 0; i < produced.size(); ++i) {
            REQUIRE(produced[i] == consumed[i]);
        }
    }
}
