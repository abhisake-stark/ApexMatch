#include "hft/ws/broadcaster.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <iostream>

namespace hft::ws {

using json = nlohmann::json;

WsBroadcaster::WsBroadcaster(hft::core::OrderBook& order_book)
    : order_book_(order_book) {
    
    server_.clear_access_channels(websocketpp::log::alevel::all);
    server_.clear_error_channels(websocketpp::log::elevel::all);
    
    server_.init_asio();
    server_.set_reuse_addr(true);
    
    server_.set_open_handler([this](ConnectionHdl hdl) { on_open(hdl); });
    server_.set_close_handler([this](ConnectionHdl hdl) { on_close(hdl); });
}

WsBroadcaster::~WsBroadcaster() {
    stop();
}

void WsBroadcaster::start(uint16_t port) {
    if (running_) return;
    running_ = true;

    server_.listen(port);
    server_.start_accept();

    server_thread_ = std::thread([this]() {
        try {
            server_.run();
        } catch (const std::exception& e) {
            std::cerr << "WsBroadcaster server thread error: " << e.what() << "\n";
        }
    });

    snapshot_thread_ = std::thread([this]() {
        snapshot_loop();
    });
}

void WsBroadcaster::stop() {
    if (!running_) return;
    running_ = false;

    server_.stop_listening();
    
    // Close all active connections
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto hdl : connections_) {
            websocketpp::lib::error_code ec;
            server_.close(hdl, websocketpp::close::status::going_away, "Server shutting down", ec);
        }
    }
    
    server_.stop();
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    if (snapshot_thread_.joinable()) {
        snapshot_thread_.join();
    }
}

void WsBroadcaster::on_open(ConnectionHdl hdl) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.insert(hdl);
}

void WsBroadcaster::on_close(ConnectionHdl hdl) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(hdl);
}

void WsBroadcaster::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto hdl : connections_) {
        websocketpp::lib::error_code ec;
        server_.send(hdl, message, websocketpp::frame::opcode::text, ec);
    }
}

void WsBroadcaster::broadcast_trades(const std::vector<hft::core::Trade>& trades, hft::core::Side aggressor_side) {
    if (trades.empty()) return;
    
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    const std::string side_str = (aggressor_side == hft::core::Side::BUY) ? "BUY" : "SELL";

    for (const auto& trade : trades) {
        json j;
        j["type"] = "trade";
        j["timestamp"] = now_ms;
        j["price"] = trade.price;
        j["qty"] = trade.quantity;
        j["aggressor_side"] = side_str;
        
        broadcast(j.dump());
    }
}

void WsBroadcaster::snapshot_loop() {
    while (running_) {
        json j;
        j["type"] = "snapshot";
        
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        json bids_array = json::array();
        json asks_array = json::array();

        // Scope the lock to minimize contention with the matching engine
        {
            std::shared_lock<std::shared_mutex> lock(order_book_.get_lock());
            
            const auto& bids = order_book_.get_bids();
            int count = 0;
            for (auto it = bids.begin(); it != bids.end() && count < 15; ++it, ++count) {
                const auto& level = it->second;
                uint32_t qty = 0;
                uint32_t orders = 0;
                
                hft::core::Order* curr = level.head;
                while (curr) {
                    qty += curr->quantity;
                    orders++;
                    curr = curr->next;
                }
                
                bids_array.push_back({
                    {"price", it->first},
                    {"qty", qty},
                    {"orders", orders}
                });
            }
            
            const auto& asks = order_book_.get_asks();
            count = 0;
            for (auto it = asks.begin(); it != asks.end() && count < 15; ++it, ++count) {
                const auto& level = it->second;
                uint32_t qty = 0;
                uint32_t orders = 0;
                
                hft::core::Order* curr = level.head;
                while (curr) {
                    qty += curr->quantity;
                    orders++;
                    curr = curr->next;
                }
                
                asks_array.push_back({
                    {"price", it->first},
                    {"qty", qty},
                    {"orders", orders}
                });
            }
        } // Lock released

        j["bids"] = bids_array;
        j["asks"] = asks_array;
        
        broadcast(j.dump());
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace hft::ws
