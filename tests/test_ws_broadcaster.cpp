#include <catch2/catch_test_macros.hpp>
#include "hft/ws/broadcaster.hpp"
#include "hft/core/order_book.hpp"
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

using WsClient = websocketpp::client<websocketpp::config::asio_client>;
using json = nlohmann::json;

TEST_CASE("WebSocket Broadcaster broadcasts snapshots and trades", "[ws]") {
    hft::core::OrderBook book(1000);
    hft::ws::WsBroadcaster broadcaster(book);
    
    // Add some orders
    book.add_order(1, hft::core::Side::BUY, 10000, 100, 1000);
    book.add_order(2, hft::core::Side::BUY, 9999, 50, 1001);
    book.add_order(3, hft::core::Side::SELL, 10005, 200, 1002);
    
    broadcaster.start(9002);
    
    // Start client in background
    WsClient client;
    client.clear_access_channels(websocketpp::log::alevel::all);
    client.clear_error_channels(websocketpp::log::elevel::all);
    client.init_asio();
    
    std::string received_snapshot;
    std::string received_trade;
    bool snapshot_received = false;
    bool trade_received = false;
    
    client.set_message_handler([&](websocketpp::connection_hdl hdl, WsClient::message_ptr msg) {
        auto payload = msg->get_payload();
        auto j = json::parse(payload);
        if (j["type"] == "snapshot" && !snapshot_received) {
            received_snapshot = payload;
            snapshot_received = true;
        } else if (j["type"] == "trade") {
            received_trade = payload;
            trade_received = true;
        }
    });
    
    websocketpp::lib::error_code ec;
    auto con = client.get_connection("ws://127.0.0.1:9002", ec);
    REQUIRE(!ec);
    client.connect(con);
    
    std::thread client_thread([&]() {
        try {
            client.run();
        } catch (...) {
            // Ignored
        }
    });
    
    // Wait for snapshot
    for (int i = 0; i < 40; ++i) {
        if (snapshot_received) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    REQUIRE(snapshot_received);
    
    auto j_snap = json::parse(received_snapshot);
    REQUIRE(j_snap["bids"].size() == 2);
    REQUIRE(j_snap["bids"][0]["price"] == 10000);
    REQUIRE(j_snap["bids"][0]["qty"] == 100);
    REQUIRE(j_snap["bids"][1]["price"] == 9999);
    REQUIRE(j_snap["asks"].size() == 1);
    REQUIRE(j_snap["asks"][0]["price"] == 10005);
    
    // Trigger a trade manually
    auto trades = book.add_order(4, hft::core::Side::SELL, 10000, 50, 1003); // Crosses best bid
    REQUIRE(trades.size() == 1);
    
    broadcaster.broadcast_trades(trades, hft::core::Side::SELL);
    
    // Wait for trade
    for (int i = 0; i < 40; ++i) {
        if (trade_received) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    REQUIRE(trade_received);
    auto j_trade = json::parse(received_trade);
    REQUIRE(j_trade["price"] == 10000);
    REQUIRE(j_trade["qty"] == 50);
    REQUIRE(j_trade["aggressor_side"] == "SELL");
    
    client.stop();
    if (client_thread.joinable()) {
        client_thread.join();
    }
    broadcaster.stop();
}
