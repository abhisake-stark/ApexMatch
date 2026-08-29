#include "hft/net/tcp_server.hpp"
#include <chrono>
#include <iostream>

namespace hft::net {

inline uint64_t parse_u64(const uint8_t* buf) {
    return (static_cast<uint64_t>(buf[0]) << 56) |
           (static_cast<uint64_t>(buf[1]) << 48) |
           (static_cast<uint64_t>(buf[2]) << 40) |
           (static_cast<uint64_t>(buf[3]) << 32) |
           (static_cast<uint64_t>(buf[4]) << 24) |
           (static_cast<uint64_t>(buf[5]) << 16) |
           (static_cast<uint64_t>(buf[6]) << 8) |
           (static_cast<uint64_t>(buf[7]));
}

inline uint32_t parse_u32(const uint8_t* buf) {
    return (static_cast<uint32_t>(buf[0]) << 24) |
           (static_cast<uint32_t>(buf[1]) << 16) |
           (static_cast<uint32_t>(buf[2]) << 8) |
           (static_cast<uint32_t>(buf[3]));
}

TcpConnection::TcpConnection(asio::ip::tcp::socket socket, 
                             ObjectPool<Order>& order_pool, 
                             SPSCRingBuffer<Order*, 65536>& ring_buffer,
                             std::atomic<uint64_t>& dropped_counter)
    : socket_(std::move(socket)),
      order_pool_(order_pool),
      ring_buffer_(ring_buffer),
      dropped_counter_(dropped_counter) {
}

void TcpConnection::start() {
    do_read();
}

void TcpConnection::do_read() {
    auto self(shared_from_this());
    // async_read guarantees we read exactly 22 bytes before calling the handler
    asio::async_read(socket_, asio::buffer(buffer_),
        [this, self](std::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                parse_and_push();
                do_read();
            } else {
                // Connection closed or error
            }
        });
}

void TcpConnection::parse_and_push() {
    Order* order = order_pool_.acquire();
    
    uint8_t type = buffer_[0]; // 0 = NEW, 1 = CANCEL
    order->order_id = parse_u64(&buffer_[1]);
    order->side = (buffer_[9] == 0) ? Side::BUY : Side::SELL;
    order->price = parse_u64(&buffer_[10]);
    
    if (type == 1) {
        order->quantity = 0; // Use quantity = 0 as a signal for CANCEL
    } else {
        order->quantity = parse_u32(&buffer_[18]);
    }
    
    order->timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::system_clock::now().time_since_epoch()).count();
                           
    if (!ring_buffer_.try_push(std::move(order))) {
        order_pool_.release(order);
        uint64_t dropped = dropped_counter_.fetch_add(1, std::memory_order_relaxed) + 1;
        if (dropped % 100 == 0) {
            std::cerr << "WARNING: Dropped " << dropped << " messages due to full ring buffer.\n";
        }
    }
}

TcpServer::TcpServer(asio::io_context& io_context, short port, 
                     ObjectPool<Order>& order_pool,
                     SPSCRingBuffer<Order*, 65536>& ring_buffer)
    : acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
      order_pool_(order_pool),
      ring_buffer_(ring_buffer) {
    do_accept();
}

void TcpServer::do_accept() {
    acceptor_.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                std::make_shared<TcpConnection>(std::move(socket), order_pool_, ring_buffer_, dropped_counter_)->start();
            }
            do_accept();
        });
}

} // namespace hft::net
