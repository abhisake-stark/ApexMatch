#include <catch2/catch_test_macros.hpp>
#include <asio.hpp>
#include <thread>
#include <chrono>
#include <vector>

#include "hft/net/tcp_server.hpp"
#include "hft/core/object_pool.hpp"
#include "hft/net/spsc_ring_buffer.hpp"

using namespace hft::net;
using namespace hft::core;

void encode_u64(uint8_t* buf, uint64_t val) {
    buf[0] = (val >> 56) & 0xFF;
    buf[1] = (val >> 48) & 0xFF;
    buf[2] = (val >> 40) & 0xFF;
    buf[3] = (val >> 32) & 0xFF;
    buf[4] = (val >> 24) & 0xFF;
    buf[5] = (val >> 16) & 0xFF;
    buf[6] = (val >> 8) & 0xFF;
    buf[7] = val & 0xFF;
}

void encode_u32(uint8_t* buf, uint32_t val) {
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

std::array<uint8_t, 22> create_wire_message(uint8_t type, uint64_t order_id, uint8_t side, uint64_t price, uint32_t quantity) {
    std::array<uint8_t, 22> msg;
    msg[0] = type;
    encode_u64(&msg[1], order_id);
    msg[9] = side;
    encode_u64(&msg[10], price);
    encode_u32(&msg[18], quantity);
    return msg;
}

TEST_CASE("TcpServer Integration Tests") {
    ObjectPool<Order> pool(10000);
    auto ring_buffer = std::make_unique<SPSCRingBuffer<Order*, 65536>>();

    asio::io_context server_io;
    TcpServer server(server_io, 0, pool, *ring_buffer);
    unsigned short port = server.port();

    std::thread server_thread([&server_io]() {
        server_io.run();
    });

    SECTION("Bulk sequential orders") {
        asio::io_context client_io;
        asio::ip::tcp::socket socket(client_io);
        socket.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), port));

        std::vector<uint8_t> send_buffer;
        send_buffer.reserve(1000 * 22);

        for (uint64_t i = 1; i <= 1000; ++i) {
            auto msg = create_wire_message(0, i, 0, 10000, 100);
            send_buffer.insert(send_buffer.end(), msg.begin(), msg.end());
        }

        asio::write(socket, asio::buffer(send_buffer));

        // Allow some time for the server to process all messages
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        uint64_t expected_id = 1;
        Order* order;
        while (ring_buffer->try_pop(order)) {
            REQUIRE(order->order_id == expected_id);
            REQUIRE(order->side == Side::BUY);
            REQUIRE(order->price == 10000);
            REQUIRE(order->quantity == 100);
            pool.release(order);
            expected_id++;
        }
        
        REQUIRE(expected_id == 1001); // Confirms all 1000 were processed
    }

    SECTION("Partial read / message framing edge case") {
        asio::io_context client_io;
        asio::ip::tcp::socket socket(client_io);
        socket.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("127.0.0.1"), port));

        auto msg = create_wire_message(0, 9999, 1, 5555, 50);

        // Write first 10 bytes
        asio::write(socket, asio::buffer(msg.data(), 10));
        
        // Wait to simulate network fragmentation
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Verify buffer is empty so far
        Order* order;
        REQUIRE(ring_buffer->try_pop(order) == false);

        // Write remaining 12 bytes
        asio::write(socket, asio::buffer(msg.data() + 10, 12));
        
        // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        REQUIRE(ring_buffer->try_pop(order) == true);
        REQUIRE(order->order_id == 9999);
        REQUIRE(order->side == Side::SELL);
        REQUIRE(order->price == 5555);
        REQUIRE(order->quantity == 50);
        
        pool.release(order);
    }

    server_io.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}
