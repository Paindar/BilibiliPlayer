#include <thread>
#include <vector>
#include <memory>
#include <future>
#include "stream/streaming_audio_buffer.h"
#include <catch2/catch_test_macros.hpp>
#include <iostream>

TEST_CASE("StreamingAudioBuffer writer cancels mid-write") {
    const size_t total_size = 1024 * 1024; // 1MB
    auto buffer = std::make_shared<StreamingAudioBuffer>(8 * 1024); // 8KB capacity to force blocking

    std::vector<uint8_t> data(total_size, 0xAB);

    std::promise<size_t> p;
    auto fut = p.get_future();

    std::thread writer([&](){
        std::cout << "[TEST] writer started" << std::endl;
        size_t written = 0;
        // This write will block when the buffer is full; we cancel from the main thread and
        // call notifyAll() to wake the writer.
        buffer->write(reinterpret_cast<const char*>(data.data()), data.size());
        // After write returns, we consider total written as total_size or as much as could be written
        // (write() is void in this API; we approximate by total_size or 0 depending on state).
        // For testing purposes we just set a non-zero sentinel if any bytes were consumed from input.
        // The buffer reports totalWritten() which we can use instead.
        written = buffer->totalWritten();
        std::cout << "[TEST] writer finished, totalWritten=" << written << std::endl;
        p.set_value(written);
    });

    // Let writer run and block in the buffer
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    // Ask it to cancel
    std::cout << "[TEST] setting cancel for writer" << std::endl;
    buffer->destroy();
    writer.join();
    size_t written = fut.get();

    CHECK(written < total_size);
    CHECK(written > 0);
}

TEST_CASE("StreamingAudioBuffer reader cancels while waiting") {
    auto buffer = std::make_shared<StreamingAudioBuffer>(4 * 1024); // 4KB

    // Pre-fill some data less than requested
    std::vector<uint8_t> pre(1024, 0x11);
    buffer->write(reinterpret_cast<const char*>(pre.data()), pre.size());

    std::vector<uint8_t> out(8 * 1024);

    std::promise<size_t> p;
    auto fut = p.get_future();

    std::thread reader([&](){
        std::cout << "[TEST] reader started" << std::endl;
        size_t r = buffer->read(out.data(), out.size());
        std::cout << "[TEST] reader finished, got=" << r << std::endl;
        p.set_value(r);
    });

    // Let reader drain available data and then block
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    // Cancel the reader
    std::cout << "[TEST] setting cancel for reader" << std::endl;
    buffer->destroy();

    reader.join();
    size_t got = fut.get();

    CHECK(got >= pre.size());
    CHECK(got < out.size());
}
