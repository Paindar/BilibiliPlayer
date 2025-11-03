#include <thread>
#include <future>
#include <vector>
#include "stream/streaming_audio_buffer.h"
#include <catch2/catch_test_macros.hpp>
#include <iostream>

TEST_CASE("StreamingInputStream unblocks on destroyBuffer") {
    auto buffer = std::make_shared<StreamingAudioBuffer>(8 * 1024);

    // Pre-fill a small amount so read will drain and then block waiting for more
    std::vector<uint8_t> pre(1024, 0x5A);
    buffer->write(reinterpret_cast<const char*>(pre.data()), pre.size());

    StreamingInputStream sis(buffer);

    const size_t request = 8 * 1024; // larger than currently available
    std::vector<char> out(request);

    std::promise<std::streamsize> p;
    auto fut = p.get_future();

    std::thread reader([&]() {
        std::cerr << "[TEST] reader starting read()\n";
        sis.read(out.data(), static_cast<std::streamsize>(request));
        std::streamsize got = sis.gcount();
        std::cerr << "[TEST] reader finished, got=" << got << "\n";
        p.set_value(got);
    });

    // allow reader to consume the prefilled bytes and then block
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Now destroy the buffer which should wake the underflow() and cause read to return
    std::cerr << "[TEST] destroying buffer to wake reader\n";
    sis.destroyBuffer();

    reader.join();
    auto got = fut.get();

    // Expect that we read at least the prefilled bytes, but less than requested
    CHECK(got >= static_cast<std::streamsize>(pre.size()));
    CHECK(got < static_cast<std::streamsize>(request));
}
