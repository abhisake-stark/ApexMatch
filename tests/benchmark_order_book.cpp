#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include "hft/core/order_book.hpp"

using namespace hft::core;

int main() {
    constexpr size_t NUM_ORDERS = 1000000;
    OrderBook book(NUM_ORDERS + 100000);
    
    std::vector<uint64_t> latencies;
    latencies.reserve(NUM_ORDERS);
    
    // Pre-populate some orders to create a thick book
    for (size_t i = 0; i < 50000; ++i) {
        book.add_order(i, Side::BUY, 90, 10, 0);
        book.add_order(i + 50000, Side::SELL, 110, 10, 0);
    }
    
    uint64_t order_id = 100000;
    
    std::cout << "Running benchmark with " << NUM_ORDERS << " orders...\n";
    
    for (size_t i = 0; i < NUM_ORDERS; ++i) {
        Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        int64_t price = 95 + (i % 10); // Price bounces between 95 and 104
        uint32_t qty = 5;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // This will occasionally cross and match, occasionally rest
        auto trades = book.add_order(order_id++, side, price, qty, 0);
        
        auto end = std::chrono::high_resolution_clock::now();
        
        // Prevent trades vector from being optimized out
        if (trades.size() > 100) {
            std::cout << "Too many trades!\n";
        }
        
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        latencies.push_back(duration);
    }
    
    std::sort(latencies.begin(), latencies.end());
    
    auto p50 = latencies[NUM_ORDERS * 0.50];
    auto p99 = latencies[NUM_ORDERS * 0.99];
    auto p999 = latencies[NUM_ORDERS * 0.999];
    
    std::cout << "Latency (ns):\n";
    std::cout << "p50:   " << p50 << " ns\n";
    std::cout << "p99:   " << p99 << " ns\n";
    std::cout << "p99.9: " << p999 << " ns\n";
    
    return 0;
}
