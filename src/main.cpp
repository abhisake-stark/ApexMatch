#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <csignal>
#include <vector>
#include <asio.hpp>

#include "hft/core/order_book.hpp"
#include "hft/core/object_pool.hpp"
#include "hft/net/tcp_server.hpp"
#include "hft/net/spsc_ring_buffer.hpp"
#include "hft/ws/broadcaster.hpp"
#include <algorithm>
#include <cmath>

using namespace hft;

// Simple log-bucketed latency histogram (nanoseconds)
struct LatencyHistogram {
    std::vector<uint64_t> buckets;
    uint64_t total_count = 0;
    
    LatencyHistogram() : buckets(32, 0) {} // Buckets for powers of 2 up to 2^31 ns
    
    void record(uint64_t ns) {
        if (ns == 0) ns = 1;
        uint32_t bucket = static_cast<uint32_t>(std::log2(ns));
        if (bucket >= buckets.size()) bucket = buckets.size() - 1;
        buckets[bucket]++;
        total_count++;
    }
    
    uint64_t get_percentile(double p) {
        if (total_count == 0) return 0;
        uint64_t target = static_cast<uint64_t>(total_count * p);
        uint64_t sum = 0;
        for (size_t i = 0; i < buckets.size(); ++i) {
            sum += buckets[i];
            if (sum >= target) {
                return 1ULL << i;
            }
        }
        return 1ULL << (buckets.size() - 1);
    }
};

// Global stop flag for signal handler
std::atomic<bool> g_running{true};

void signal_handler(int signum) {
    std::cerr << "\n[MAIN] Received signal " << signum << ", initiating graceful shutdown...\n";
    g_running.store(false, std::memory_order_release);
}

int main() {
    std::cout << "ApexMatch v0.1 - Starting up...\n";

    // Register signal handler
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 1. Initialize Components
    constexpr size_t ORDER_BOOK_CAPACITY = 100000;
    constexpr size_t RING_BUFFER_CAPACITY = 65536;
    
    // Core engine
    core::OrderBook order_book(ORDER_BOOK_CAPACITY);
    
    // Ring buffer for IPC (TCP Ingestion -> Matching Engine)
    net::SPSCRingBuffer<core::Order*, RING_BUFFER_CAPACITY> ring_buffer;
    
    // ObjectPool specifically for messages pushed into the ring buffer
    core::ObjectPool<core::Order> ring_buffer_pool(ORDER_BOOK_CAPACITY);
    
    // WebSocket Broadcaster (Phase 5)
    ws::WsBroadcaster broadcaster(order_book);

    // TCP Ingestion Server (Phase 4)
    asio::io_context io_context;
    net::TcpServer tcp_server(io_context, 9001, ring_buffer_pool, ring_buffer);

    // 2. Start Threads
    
    // Thread 1: TCP Ingestion (Boost.Asio event loop)
    std::jthread tcp_thread([&io_context](std::stop_token st) {
        std::cerr << "[TCP] Ingestion thread started on port 9001.\n";
        // Run until the context is stopped
        while (g_running.load(std::memory_order_acquire)) {
            io_context.run_for(std::chrono::milliseconds(100));
        }
        io_context.stop();
        std::cerr << "[TCP] Ingestion thread stopping.\n";
    });

    // Thread 3: WS Broadcaster (manages its own internal threads)
    broadcaster.start(9002);
    std::cerr << "[WS] Broadcaster started on port 9002.\n";

    // Metrics for Thread 2
    std::atomic<uint64_t> orders_processed{0};
    std::atomic<uint64_t> trades_executed{0};

    // Thread 2: Core Matching Engine
    LatencyHistogram matching_histogram;
    std::jthread matching_thread([&](std::stop_token st) {
        std::cerr << "[MATCHING] Core thread started.\n";
        
        uint32_t spin_count = 0;
        constexpr uint32_t MAX_SPINS = 1000;

        while (g_running.load(std::memory_order_acquire)) {
            core::Order* order = nullptr;
            
            if (ring_buffer.try_pop(order)) {
                // Successfully popped an order from the ingestion thread
                spin_count = 0; 
                
                auto t_start = std::chrono::high_resolution_clock::now();
                
                std::vector<core::Trade> trades;
                
                // Exclusive lock on the OrderBook for writing
                {
                    std::unique_lock<std::shared_mutex> lock(order_book.get_lock());
                    
                    if (order->quantity == 0) {
                        // Protocol uses quantity = 0 to signify a CANCEL order
                        order_book.cancel_order(order->order_id);
                    } else {
                        // NEW order
                        trades = order_book.add_order(
                            order->order_id, 
                            order->side, 
                            order->price, 
                            order->quantity, 
                            order->timestamp
                        );
                    }
                } // Lock released

                auto t_end = std::chrono::high_resolution_clock::now();
                matching_histogram.record(std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count());

                // Release the popped order back to the IPC pool since the OrderBook 
                // internally copies its properties into its own managed pool.
                ring_buffer_pool.release(order);
                
                orders_processed.fetch_add(1, std::memory_order_relaxed);

                if (!trades.empty()) {
                    trades_executed.fetch_add(trades.size(), std::memory_order_relaxed);
                    
                    // We directly invoke the broadcaster's broadcast_trades method from the matching thread.
                    // Justification: It's the simplest approach that avoids allocating a secondary queue 
                    // just for trades. WsBroadcaster::broadcast_trades internally takes a std::mutex over its
                    // connection list and pushes to the websocketpp ASIO loop asynchronously. Since string serialization 
                    // and a single mutex lock are relatively cheap, it is acceptable for Phase 6. In a stricter HFT 
                    // environment, we would use another SPSC ring buffer to hand off trades to avoid ANY locking on 
                    // the critical path.
                    broadcaster.broadcast_trades(trades, order->side);
                }
                
            } else {
                // Backoff strategy:
                // We use a brief spin-then-yield backoff.
                // Spinning for a short burst (e.g. 1000 iterations) keeps the core hot and handles micro-bursts of traffic with low latency.
                // If it remains empty, yielding avoids pegging the CPU at 100% indefinitely, which would starve other threads (like TCP/WS) 
                // on a machine with fewer cores than threads. A short sleep is used if it's completely idle.
                spin_count++;
                if (spin_count < MAX_SPINS) {
                    // CPU pause instruction (busy wait)
                    #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
                    #ifdef _MSC_VER
                        _mm_pause();
                    #else
                        __builtin_ia32_pause();
                    #endif
                    #endif
                } else if (spin_count < MAX_SPINS * 2) {
                    std::this_thread::yield();
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            }
        }
        std::cerr << "[MATCHING] Core thread stopping.\n";
    });

    // Main thread acts as the metrics logging loop
    auto last_log_time = std::chrono::steady_clock::now();
    uint64_t last_orders = 0;
    uint64_t last_trades = 0;
    
    while (g_running.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        auto now = std::chrono::steady_clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_log_time).count();
        
        if (dt >= 1000) {
            uint64_t current_orders = orders_processed.load(std::memory_order_relaxed);
            uint64_t current_trades = trades_executed.load(std::memory_order_relaxed);
            uint64_t dropped = tcp_server.get_dropped_count();
            
            double orders_per_sec = (current_orders - last_orders) * 1000.0 / dt;
            double trades_per_sec = (current_trades - last_trades) * 1000.0 / dt;
            
            std::cerr << "[METRICS] " 
                      << "Orders: " << static_cast<uint64_t>(orders_per_sec) << "/s | "
                      << "Trades: " << static_cast<uint64_t>(trades_per_sec) << "/s | "
                      << "Dropped: " << dropped 
                      << " || Matching p50: " << matching_histogram.get_percentile(0.5) / 1000.0 << "us, "
                      << "p99: " << matching_histogram.get_percentile(0.99) / 1000.0 << "us\n";
                      
            last_orders = current_orders;
            last_trades = current_trades;
            last_log_time = now;
        }
    }

    // Graceful shutdown sequence
    std::cerr << "[MAIN] Shutting down WS Broadcaster...\n";
    broadcaster.stop();
    
    std::cerr << "[MAIN] Waiting for threads to join...\n";
    // jthreads will automatically request stop and join on destruction

    std::cout << "ApexMatch shutdown complete.\n";
    return 0;
}
