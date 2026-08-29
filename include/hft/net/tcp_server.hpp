#pragma once

#include <asio.hpp>
#include <memory>
#include <array>
#include <atomic>
#include <cstdint>
#include <iostream>

#include "hft/core/order.hpp"
#include "hft/core/object_pool.hpp"
#include "hft/net/spsc_ring_buffer.hpp"

namespace hft::net {

using namespace hft::core;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(asio::ip::tcp::socket socket, 
                  ObjectPool<Order>& order_pool, 
                  SPSCRingBuffer<Order*, 65536>& ring_buffer,
                  std::atomic<uint64_t>& dropped_counter);

    void start();

private:
    void do_read();
    void parse_and_push();

    asio::ip::tcp::socket socket_;
    ObjectPool<Order>& order_pool_;
    SPSCRingBuffer<Order*, 65536>& ring_buffer_;
    std::atomic<uint64_t>& dropped_counter_;

    std::array<uint8_t, 22> buffer_;
};

class TcpServer {
public:
    TcpServer(asio::io_context& io_context, short port, 
              ObjectPool<Order>& order_pool,
              SPSCRingBuffer<Order*, 65536>& ring_buffer);

    uint64_t get_dropped_count() const { return dropped_counter_.load(std::memory_order_relaxed); }
    unsigned short port() const { return acceptor_.local_endpoint().port(); }

private:
    void do_accept();

    asio::ip::tcp::acceptor acceptor_;
    ObjectPool<Order>& order_pool_;
    SPSCRingBuffer<Order*, 65536>& ring_buffer_;
    std::atomic<uint64_t> dropped_counter_{0};
};

} // namespace hft::net
