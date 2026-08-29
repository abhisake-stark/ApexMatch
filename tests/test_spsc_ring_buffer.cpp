#include <catch2/catch_test_macros.hpp>
#include "hft/net/spsc_ring_buffer.hpp"
#include "hft/core/order.hpp"
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <memory>

using namespace hft::net;
using namespace hft::core;

TEST_CASE("SPSCRingBuffer Basic Functionality", "[SPSCRingBuffer]") {
    SPSCRingBuffer<Order, 4> buffer;
    Order o1; o1.order_id = 1;
    Order o2; o2.order_id = 2;
    Order o3; o3.order_id = 3;
    Order o4; o4.order_id = 4;
    Order o5; o5.order_id = 5;

    REQUIRE(buffer.try_push(std::move(o1)) == true);
    REQUIRE(buffer.try_push(std::move(o2)) == true);
    REQUIRE(buffer.try_push(std::move(o3)) == true);
    REQUIRE(buffer.try_push(std::move(o4)) == true);
    
    // Buffer is full (Capacity is 4, but tail - head can be up to 4)
    REQUIRE(buffer.try_push(std::move(o5)) == false);
    
    Order out;
    REQUIRE(buffer.try_pop(out) == true);
    REQUIRE(out.order_id == 1);
    
    REQUIRE(buffer.try_push(std::move(o5)) == true); // Should succeed now
    
    REQUIRE(buffer.try_pop(out) == true);
    REQUIRE(out.order_id == 2);
    REQUIRE(buffer.try_pop(out) == true);
    REQUIRE(out.order_id == 3);
    REQUIRE(buffer.try_pop(out) == true);
    REQUIRE(out.order_id == 4);
    REQUIRE(buffer.try_pop(out) == true);
    REQUIRE(out.order_id == 5);
    
    // Buffer should be empty
    REQUIRE(buffer.try_pop(out) == false);
}

TEST_CASE("SPSCRingBuffer Concurrency Stress Test", "[SPSCRingBuffer][Stress]") {
    constexpr size_t CAPACITY = 1024 * 64; // 64K
    constexpr uint64_t NUM_MESSAGES = 10'000'000;
    
    auto buffer = std::make_unique<SPSCRingBuffer<Order, CAPACITY>>();
    
    // Expected sum of order IDs: Sum_{i=1}^{NUM_MESSAGES} i
    // n(n+1)/2
    uint64_t expected_sum = (NUM_MESSAGES * (NUM_MESSAGES + 1)) / 2;
    uint64_t actual_sum = 0;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::jthread consumer([&buffer, &actual_sum, NUM_MESSAGES]() {
        uint64_t received = 0;
        Order order;
        while (received < NUM_MESSAGES) {
            if (buffer->try_pop(order)) {
                actual_sum += order.order_id;
                received++;
            }
        }
    });
    
    std::jthread producer([&buffer, NUM_MESSAGES]() {
        for (uint64_t i = 1; i <= NUM_MESSAGES; ++i) {
            Order order;
            order.order_id = i;
            
            // Spin until push succeeds
            while (!buffer->try_push(std::move(order))) {
                // busy wait
            }
        }
    });
    
    // join is automatic due to std::jthread, but we can wait explicitly if we want to measure time cleanly
    producer.join();
    consumer.join();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    REQUIRE(actual_sum == expected_sum);
    
    double seconds = duration.count() / 1000.0;
    std::cout << "--- SPSCRingBuffer Stress Test ---\n";
    std::cout << "Processed " << NUM_MESSAGES << " messages in " << seconds << " seconds.\n";
    std::cout << "Throughput: " << (NUM_MESSAGES / seconds) << " orders/sec\n";
    std::cout << "Data race check: passed. Expected sum == Actual sum.\n";
}
