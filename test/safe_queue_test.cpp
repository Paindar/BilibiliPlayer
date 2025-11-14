/**
 * SafeQueue Utility Unit Tests
 * 
 * Tests thread-safe queue operations and concurrent access.
 */

#include <catch2/catch_test_macros.hpp>
#include <util/safe_queue.hpp>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

TEST_CASE("SafeQueue basic operations", "[SafeQueue]") {
    SECTION("Enqueue and dequeue single item") {
        SafeQueue<int> queue;
        
        queue.enqueue(42);
        
        int value;
        bool result = queue.dequeue(value);
        
        REQUIRE(result == true);
        REQUIRE(value == 42);
    }
    
    SECTION("dequeue on empty queue") {
        SafeQueue<int> queue;
        
        int value;
        bool result = queue.dequeue(value);
        
        REQUIRE(result == false);
    }
    
    SECTION("Multiple enqueue/dequeue") {
        SafeQueue<int> queue;
        
        queue.enqueue(1);
        queue.enqueue(2);
        queue.enqueue(3);
        
        int v1, v2, v3;
        REQUIRE(queue.dequeue(v1) == true);
        REQUIRE(queue.dequeue(v2) == true);
        REQUIRE(queue.dequeue(v3) == true);
        
        REQUIRE(v1 == 1);
        REQUIRE(v2 == 2);
        REQUIRE(v3 == 3);
    }
    
    SECTION("FIFO order") {
        SafeQueue<int> queue;
        
        for (int i = 0; i < 10; ++i) {
            queue.enqueue(i);
        }
        
        for (int i = 0; i < 10; ++i) {
            int value;
            REQUIRE(queue.dequeue(value) == true);
            REQUIRE(value == i);
        }
    }
}

TEST_CASE("SafeQueue blocking operations", "[SafeQueue]") {
    SECTION("waitAndDequeue blocks until data available") {
        SafeQueue<int> queue;
        
        // Producer thread
        std::thread producer([&queue]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            queue.enqueue(42);
        });
        
        // Consumer waits
        int value;
        queue.waitAndDequeue(value);
        
        REQUIRE(value == 42);
        
        producer.join();
    }
}

TEST_CASE("SafeQueue state queries", "[SafeQueue]") {
    SECTION("empty() on new queue") {
        SafeQueue<int> queue;
        
        REQUIRE(queue.empty() == true);
    }
    
    SECTION("empty() after enqueue") {
        SafeQueue<int> queue;
        
        queue.enqueue(1);
        
        REQUIRE(queue.empty() == false);
    }
    
    SECTION("empty() after dequeue") {
        SafeQueue<int> queue;
        
        queue.enqueue(1);
        int value;
        queue.dequeue(value);
        
        REQUIRE(queue.empty() == true);
    }
    
    SECTION("size() accuracy") {
        SafeQueue<int> queue;
        
        REQUIRE(queue.size() == 0);
        
        queue.enqueue(1);
        REQUIRE(queue.size() == 1);
        
        queue.enqueue(2);
        queue.enqueue(3);
        REQUIRE(queue.size() == 3);
        
        int value;
        queue.dequeue(value);
        REQUIRE(queue.size() == 2);
    }
}

TEST_CASE("SafeQueue clean operation", "[SafeQueue]") {
    SECTION("Clean empty queue") {
        SafeQueue<int> queue;
        
        queue.clean();
        
        REQUIRE(queue.empty() == true);
    }
    
    SECTION("Clean non-empty queue") {
        SafeQueue<int> queue;
        
        queue.enqueue(1);
        queue.enqueue(2);
        queue.enqueue(3);
        
        queue.clean();
        
        REQUIRE(queue.empty() == true);
        REQUIRE(queue.size() == 0);
    }
}

TEST_CASE("SafeQueue concurrent access", "[SafeQueue]") {
    SECTION("Multiple producers") {
        SafeQueue<int> queue;
        constexpr int num_threads = 4;
        constexpr int items_per_thread = 100;
        
        std::vector<std::thread> producers;
        for (int t = 0; t < num_threads; ++t) {
            producers.emplace_back([&queue, t]() {
                for (int i = 0; i < items_per_thread; ++i) {
                    queue.enqueue(t * items_per_thread + i);
                }
            });
        }
        
        for (auto& t : producers) {
            t.join();
        }
        
        REQUIRE(queue.size() == num_threads * items_per_thread);
    }
    
    SECTION("Multiple consumers") {
        SafeQueue<int> queue;
        constexpr int total_items = 400;
        
        // Fill queue
        for (int i = 0; i < total_items; ++i) {
            queue.enqueue(i);
        }
        
        std::atomic<int> consumed_count{0};
        std::vector<std::thread> consumers;
        
        for (int t = 0; t < 4; ++t) {
            consumers.emplace_back([&queue, &consumed_count]() {
                int value;
                while (queue.dequeue(value)) {
                    consumed_count++;
                }
            });
        }
        
        for (auto& t : consumers) {
            t.join();
        }
        
        REQUIRE(consumed_count == total_items);
        REQUIRE(queue.empty() == true);
    }
    
    SECTION("Producer-consumer pattern") {
        SafeQueue<int> queue;
        constexpr int total_items = 1000;
        std::atomic<bool> done{false};
        
        // Producer thread
        std::thread producer([&queue, &done]() {
            for (int i = 0; i < total_items; ++i) {
                queue.enqueue(i);
            }
            done = true;
        });
        
        // Consumer thread
        std::vector<int> consumed;
        std::thread consumer([&queue, &done, &consumed]() {
            while (!done || !queue.empty()) {
                int value;
                if (queue.dequeue(value)) {
                    consumed.push_back(value);
                } else {
                    std::this_thread::yield();
                }
            }
        });
        
        producer.join();
        consumer.join();
        
        REQUIRE(consumed.size() == total_items);
        REQUIRE(queue.empty() == true);
    }
    
    SECTION("Concurrent enqueue and dequeue") {
        SafeQueue<int> queue;
        constexpr int operations = 1000;
        
        std::thread pusher([&queue]() {
            for (int i = 0; i < operations; ++i) {
                queue.enqueue(i);
            }
        });
        
        std::thread popper([&queue]() {
            for (int i = 0; i < operations; ++i) {
                int value;
                while (!queue.dequeue(value)) {
                    std::this_thread::yield();
                }
            }
        });
        
        pusher.join();
        popper.join();
        
        REQUIRE(queue.empty() == true);
    }
}

TEST_CASE("SafeQueue with custom types", "[SafeQueue]") {
    struct TestData {
        int id;
        std::string name;
        
        bool operator==(const TestData& other) const {
            return id == other.id && name == other.name;
        }
    };
    
    SECTION("Enqueue/dequeue custom type") {
        SafeQueue<TestData> queue;
        
        TestData data{42, "test"};
        queue.enqueue(data);
        
        TestData retrieved;
        bool result = queue.dequeue(retrieved);
        
        REQUIRE(result == true);
        REQUIRE(retrieved == data);
    }
}
