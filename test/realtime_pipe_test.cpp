#include <thread>
#include <future>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <stream/realtime_pipe.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>

TEST_CASE("RealtimePipe basic write/read", "[RealtimePipe]") {
    RealtimePipe pipe(1024);
    auto in = pipe.getInputStream();
    auto out = pipe.getOutputStream();

    const std::string message = "Hello from RealtimePipe!";
    std::promise<void> started;
    std::atomic<bool> done = false;

    std::thread reader([&] {
        started.set_value();
        char buffer[64];
        std::string received;
        std::cerr << "[TEST] Reader started, waiting for data...\n";
        
        while (!in->eof()) {
            in->read(buffer, sizeof(buffer));
            std::streamsize bytesRead = in->gcount();
            std::cerr << "[TEST] Read " << bytesRead << " bytes, EOF: " << in->eof() << "\n";
            
            if (bytesRead > 0) {
                received.append(buffer, bytesRead);
                std::cerr << "[TEST] Received so far: \"" << received << "\"\n";
            }
            
            // Prevent infinite loop if stream is stuck
            if (bytesRead == 0 && !in->eof()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        
        std::cerr << "[TEST] Reader finished. Final received: \"" << received << "\"\n";
        CHECK(received == message);
        done = true;
    });

    started.get_future().wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::cerr << "[TEST] Writing message: \"" << message << "\"\n";
    out->write(message.data(), static_cast<std::streamsize>(message.size()));
    out->flush();
    std::cerr << "[TEST] Message written and flushed\n";

    // Give a moment for data to be available
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Let OutputStream go out of scope to trigger EOF
    std::cerr << "[TEST] Closing output stream to trigger EOF\n";
    out.reset();

    reader.join();
    CHECK(done);
}

TEST_CASE("RealtimePipe unblocks on close") {
    // Create a RealtimePipe with 8 KB capacity
    RealtimePipe pipe(8 * 1024);

    auto in = pipe.getInputStream();
    auto out = pipe.getOutputStream();

    // Pre-fill a small amount so read will drain and then block waiting for more
    std::vector<char> pre(1024, 0x5A);
    out->write(pre.data(), static_cast<std::streamsize>(pre.size()));
    out->flush();

    const size_t request = 8 * 1024; // larger than currently available
    std::vector<char> readBuffer(request);

    std::promise<std::streamsize> p;
    auto fut = p.get_future();

    std::thread reader([&]() {
        std::cerr << "[TEST] reader starting read()\n";
        in->read(readBuffer.data(), static_cast<std::streamsize>(request));
        std::streamsize got = in->gcount();
        std::cerr << "[TEST] reader finished, got=" << got << "\n";
        p.set_value(got);
    });

    // allow reader to consume the prefilled bytes and then block
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Now close the pipe which should wake the blocked read
    std::cerr << "[TEST] closing pipe to wake reader\n";
    pipe.close();

    reader.join();
    auto got = fut.get();

    // Expect that we read at least the prefilled bytes, but less than requested
    CHECK(got >= static_cast<std::streamsize>(pre.size()));
    CHECK(got < static_cast<std::streamsize>(request));
}

TEST_CASE("RealtimePipe write blocks when full and unblocks when data is consumed") {
    const size_t capacity = 1024; // small capacity to trigger blocking
    RealtimePipe pipe(capacity);

    auto in = pipe.getInputStream();
    auto out = pipe.getOutputStream();

    // Prepare data larger than capacity with deterministic pattern
    std::vector<char> data(capacity * 2);
    // Fill with simple, predictable pattern that's easy to verify
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<char>(i & 0xFF); // Simple incrementing pattern with wrap-around
    }

    std::promise<std::streamsize> writeDone;
    auto fut = writeDone.get_future();

    // Writer thread: attempts to write 2x capacity, will block on second half
    std::thread writer([&]() {
        std::cerr << "[TEST] writer starting\n";
        out->write(data.data(), static_cast<std::streamsize>(data.size()));
        out->flush();
        std::cerr << "[TEST] writer finished\n";
        writeDone.set_value(static_cast<std::streamsize>(data.size()));
    });

    // Allow writer to fill the pipe and block
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Reader thread: consumes all data to unblock writer
    std::vector<char> readData(capacity * 2);
    std::thread reader([&]() {
        std::cerr << "[TEST] reader starting\n";
        std::streamsize totalRead = 0;
        
        while (totalRead < static_cast<std::streamsize>(data.size()) && !in->eof()) {
            std::streamsize toRead = std::min(
                static_cast<std::streamsize>(readData.size() - totalRead),
                static_cast<std::streamsize>(capacity / 2) // Read in chunks
            );
            
            in->read(readData.data() + totalRead, toRead);
            std::streamsize bytesRead = in->gcount();
            totalRead += bytesRead;
            
            std::cerr << "[TEST] reader read " << bytesRead << " bytes, total: " << totalRead << "\n";
            
            // Small delay to allow writer to make progress
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        std::cerr << "[TEST] reader finished, total read: " << totalRead << "\n";
    });

    reader.join();
    writer.join();

    auto written = fut.get();

    // Writer should have written the full data eventually
    CHECK(written == static_cast<std::streamsize>(data.size()));
    
    // Verify data integrity - check if all data was transmitted correctly
    bool dataMatches = std::equal(data.begin(), data.end(), readData.begin());
    if (!dataMatches) {
        // Find first mismatch for debugging
        auto mismatch = std::mismatch(data.begin(), data.end(), readData.begin());
        if (mismatch.first != data.end()) {
            size_t pos = std::distance(data.begin(), mismatch.first);
            std::cerr << "[TEST] Data mismatch at position " << pos 
                      << ": expected " << static_cast<int>(*mismatch.first)
                      << ", got " << static_cast<int>(*mismatch.second) << "\n";
        }
    }
    CHECK(dataMatches);
}

TEST_CASE("RealtimePipe with random data patterns", "[RealtimePipe]") {
    const size_t capacity = 2048;
    RealtimePipe pipe(capacity);

    auto in = pipe.getInputStream();
    auto out = pipe.getOutputStream();

    // Use seeded random generator for reproducible "randomness"
    std::srand(42); // Fixed seed for reproducibility
    
    // Generate random data larger than capacity
    std::vector<char> data(capacity * 3);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<char>(std::rand() & 0xFF);
    }

    std::promise<std::streamsize> writeDone;
    auto fut = writeDone.get_future();
    std::vector<char> readData(data.size());
    std::atomic<std::streamsize> totalBytesRead{0};

    // Writer thread
    std::thread writer([&]() {
        std::cerr << "[RANDOM TEST] writer starting with " << data.size() << " bytes\n";
        out->write(data.data(), static_cast<std::streamsize>(data.size()));
        out->flush();
        std::cerr << "[RANDOM TEST] writer finished\n";
        writeDone.set_value(static_cast<std::streamsize>(data.size()));
    });

    // Reader thread with random read sizes
    std::thread reader([&]() {
        std::cerr << "[RANDOM TEST] reader starting\n";
        std::streamsize totalRead = 0;
        
        while (totalRead < static_cast<std::streamsize>(data.size()) && !in->eof()) {
            // Random read size between 1 and 512 bytes
            std::streamsize readSize = 1 + (std::rand() % 512);
            readSize = std::min(readSize, static_cast<std::streamsize>(data.size() - totalRead));
            
            in->read(readData.data() + totalRead, readSize);
            std::streamsize bytesRead = in->gcount();
            totalRead += bytesRead;
            
            // Random delay to create varied timing
            if (std::rand() % 10 == 0) { // 10% chance of delay
                std::this_thread::sleep_for(std::chrono::milliseconds(1 + (std::rand() % 20)));
            }
        }
        
        totalBytesRead.store(totalRead);
        std::cerr << "[RANDOM TEST] reader finished, total read: " << totalRead << "\n";
    });

    reader.join();
    writer.join();

    auto written = fut.get();
    auto read = totalBytesRead.load();

    // Verify all data was written and read
    CHECK(written == static_cast<std::streamsize>(data.size()));
    CHECK(read == static_cast<std::streamsize>(data.size()));
    
    // Verify data integrity with random patterns
    bool dataMatches = std::equal(data.begin(), data.end(), readData.begin());
    if (!dataMatches) {
        auto mismatch = std::mismatch(data.begin(), data.end(), readData.begin());
        if (mismatch.first != data.end()) {
            size_t pos = std::distance(data.begin(), mismatch.first);
            std::cerr << "[RANDOM TEST] Data mismatch at position " << pos 
                      << ": expected 0x" << std::hex << static_cast<unsigned char>(*mismatch.first)
                      << ", got 0x" << std::hex << static_cast<unsigned char>(*mismatch.second) << "\n";
        }
    }
    CHECK(dataMatches);
}