#pragma once

#include <cstdint>
#include <vector>
#include <set>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>

// ASIO standalone macro must be defined before websocketpp includes
#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "hft/core/order_book.hpp"
#include "hft/core/trade.hpp"

namespace hft::ws {

using WsServer = websocketpp::server<websocketpp::config::asio>;
using ConnectionHdl = websocketpp::connection_hdl;

class WsBroadcaster {
public:
    explicit WsBroadcaster(hft::core::OrderBook& order_book);
    ~WsBroadcaster();

    // Start the WebSocket server and the background snapshot thread
    void start(uint16_t port);
    
    // Stop the server and background threads
    void stop();

    // Immediately broadcasts trades triggered by an incoming aggressive order
    void broadcast_trades(const std::vector<hft::core::Trade>& trades, hft::core::Side aggressor_side);

private:
    void on_open(ConnectionHdl hdl);
    void on_close(ConnectionHdl hdl);
    
    void snapshot_loop();
    void broadcast(const std::string& message);

    hft::core::OrderBook& order_book_;
    
    WsServer server_;
    std::thread server_thread_;
    
    std::set<ConnectionHdl, std::owner_less<ConnectionHdl>> connections_;
    std::mutex connections_mutex_;
    
    std::thread snapshot_thread_;
    std::atomic<bool> running_{false};
};

} // namespace hft::ws
