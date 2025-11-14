/**
 * CircularBuffer Utility Unit Tests
 * 
 * Tests circular buffer operations, state management, and data integrity.
 */

#include <catch2/catch_test_macros.hpp>
#include <util/circular_buffer.hpp>
#include <vector>
#include <stdexcept>

using namespace util;

TEST_CASE("CircularBuffer construction", "[CircularBuffer]") {
    SECTION("Create with valid capacity") {
        CircularBuffer<int> buffer(10);
        
        REQUIRE(buffer.capacity() == 10);
        REQUIRE(buffer.empty() == true);
        REQUIRE(buffer.size() == 0);
    }
    
    SECTION("Create with capacity 1") {
        CircularBuffer<int> buffer(1);
        
        REQUIRE(buffer.capacity() == 1);
        REQUIRE(buffer.empty() == true);
    }
    
    SECTION("Create with capacity 0 throws") {
        REQUIRE_THROWS_AS(CircularBuffer<int>(0), std::invalid_argument);
    }
}

TEST_CASE("CircularBuffer push/pop", "[CircularBuffer]") {
    SECTION("Push single element") {
        CircularBuffer<int> buffer(5);
        
        buffer.push(42);
        
        REQUIRE(buffer.size() == 1);
        REQUIRE(buffer.empty() == false);
    }
    
    SECTION("Pop single element") {
        CircularBuffer<int> buffer(5);
        buffer.push(42);
        
        int value = buffer.pop();
        
        REQUIRE(value == 42);
        REQUIRE(buffer.empty() == true);
    }
    
    SECTION("Push to full overwrites") {
        CircularBuffer<int> buffer(3);
        
        buffer.push(1);
        buffer.push(2);
        buffer.push(3);
        REQUIRE(buffer.full() == true);
        
        buffer.push(4); // Overwrites oldest (1)
        
        REQUIRE(buffer.size() == 3);
        REQUIRE(buffer.pop() == 2); // 1 was overwritten
    }
    
    SECTION("Pop from empty throws") {
        CircularBuffer<int> buffer(5);
        
        REQUIRE_THROWS_AS(buffer.pop(), std::runtime_error);
    }
    
    SECTION("FIFO order preserved") {
        CircularBuffer<int> buffer(5);
        
        buffer.push(1);
        buffer.push(2);
        buffer.push(3);
        
        REQUIRE(buffer.pop() == 1);
        REQUIRE(buffer.pop() == 2);
        REQUIRE(buffer.pop() == 3);
    }
}

TEST_CASE("CircularBuffer write/read", "[CircularBuffer]") {
    SECTION("Write multiple elements") {
        CircularBuffer<uint8_t> buffer(10);
        
        uint8_t data[] = {1, 2, 3, 4, 5};
        buffer.write(data, 5);
        
        REQUIRE(buffer.size() == 5);
    }
    
    SECTION("Read multiple elements") {
        CircularBuffer<uint8_t> buffer(10);
        
        uint8_t write_data[] = {1, 2, 3, 4, 5};
        buffer.write(write_data, 5);
        
        uint8_t read_data[5];
        size_t read = buffer.read(read_data, 5);
        
        REQUIRE(read == 5);
        REQUIRE(read_data[0] == 1);
        REQUIRE(read_data[4] == 5);
    }
    
    SECTION("Write more than capacity") {
        CircularBuffer<uint8_t> buffer(3);
        
        uint8_t data[] = {1, 2, 3, 4, 5};
        buffer.write(data, 5);
        
        // Should overwrite oldest data
        REQUIRE(buffer.size() == 3);
        
        uint8_t read_data[3];
        buffer.read(read_data, 3);
        
        // Should have last 3 elements
        REQUIRE(read_data[0] == 3);
        REQUIRE(read_data[1] == 4);
        REQUIRE(read_data[2] == 5);
    }
    
    SECTION("Read more than available") {
        CircularBuffer<uint8_t> buffer(10);
        
        uint8_t write_data[] = {1, 2, 3};
        buffer.write(write_data, 3);
        
        uint8_t read_data[10];
        size_t read = buffer.read(read_data, 10);
        
        REQUIRE(read == 3); // Only 3 available
    }
    
    SECTION("Write 0 bytes") {
        CircularBuffer<uint8_t> buffer(10);
        
        uint8_t data[] = {1};
        buffer.write(data, 0);
        
        REQUIRE(buffer.size() == 0);
    }
    
    SECTION("Read 0 bytes") {
        CircularBuffer<uint8_t> buffer(10);
        
        uint8_t write_data[] = {1, 2, 3};
        buffer.write(write_data, 3);
        
        uint8_t read_data[10];
        size_t read = buffer.read(read_data, 0);
        
        REQUIRE(read == 0);
        REQUIRE(buffer.size() == 3); // Unchanged
    }
}

TEST_CASE("CircularBuffer state management", "[CircularBuffer]") {
    SECTION("empty() after creation") {
        CircularBuffer<int> buffer(5);
        
        REQUIRE(buffer.empty() == true);
        REQUIRE(buffer.full() == false);
    }
    
    SECTION("full() after fill") {
        CircularBuffer<int> buffer(3);
        
        buffer.push(1);
        buffer.push(2);
        buffer.push(3);
        
        REQUIRE(buffer.full() == true);
        REQUIRE(buffer.empty() == false);
    }
    
    SECTION("size() accuracy") {
        CircularBuffer<int> buffer(10);
        
        REQUIRE(buffer.size() == 0);
        
        buffer.push(1);
        REQUIRE(buffer.size() == 1);
        
        buffer.push(2);
        buffer.push(3);
        REQUIRE(buffer.size() == 3);
        
        buffer.pop();
        REQUIRE(buffer.size() == 2);
    }
    
    SECTION("Wrap-around behavior") {
        CircularBuffer<int> buffer(3);
        
        // Fill buffer
        buffer.push(1);
        buffer.push(2);
        buffer.push(3);
        
        // Pop one
        buffer.pop();
        
        // Push one (wraps around)
        buffer.push(4);
        
        REQUIRE(buffer.size() == 3);
        REQUIRE(buffer.pop() == 2);
        REQUIRE(buffer.pop() == 3);
        REQUIRE(buffer.pop() == 4);
    }
}

TEST_CASE("CircularBuffer data integrity", "[CircularBuffer]") {
    SECTION("Data preserved across operations") {
        CircularBuffer<uint8_t> buffer(100);
        
        std::vector<uint8_t> original(50);
        for (size_t i = 0; i < 50; ++i) {
            original[i] = static_cast<uint8_t>(i);
        }
        
        buffer.write(original.data(), original.size());
        
        std::vector<uint8_t> retrieved(50);
        buffer.read(retrieved.data(), retrieved.size());
        
        REQUIRE(retrieved == original);
    }
    
    SECTION("No corruption on overwrite") {
        CircularBuffer<uint8_t> buffer(5);
        
        // Fill buffer
        uint8_t data1[] = {1, 2, 3, 4, 5};
        buffer.write(data1, 5);
        
        // Overwrite with new data
        uint8_t data2[] = {6, 7, 8};
        buffer.write(data2, 3);
        
        // Should have last 5 bytes: 4, 5, 6, 7, 8
        uint8_t result[5];
        buffer.read(result, 5);
        
        REQUIRE(result[0] == 4);
        REQUIRE(result[1] == 5);
        REQUIRE(result[2] == 6);
        REQUIRE(result[3] == 7);
        REQUIRE(result[4] == 8);
    }
    
    SECTION("Partial read/write") {
        CircularBuffer<uint8_t> buffer(10);
        
        uint8_t data[] = {1, 2, 3, 4, 5};
        buffer.write(data, 5);
        
        uint8_t partial[3];
        buffer.read(partial, 3);
        
        REQUIRE(partial[0] == 1);
        REQUIRE(partial[1] == 2);
        REQUIRE(partial[2] == 3);
        REQUIRE(buffer.size() == 2); // 2 remaining
    }
}
