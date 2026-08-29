#include <catch2/catch_test_macros.hpp>
#include "hft/core/order_book.hpp"

using namespace hft::core;

TEST_CASE("ObjectPool exhaustion and reuse", "[ObjectPool]") {
    ObjectPool<Order> pool(2);
    
    Order* o1 = pool.acquire();
    Order* o2 = pool.acquire();
    
    REQUIRE(o1 != nullptr);
    REQUIRE(o2 != nullptr);
    REQUIRE(pool.is_exhausted() == true);
    
    pool.release(o1);
    REQUIRE(pool.is_exhausted() == false);
    
    Order* o3 = pool.acquire();
    REQUIRE(o3 == o1);
    REQUIRE(pool.is_exhausted() == true);
}

TEST_CASE("O(1) cancel removes correct node without corruption", "[PriceLevel]") {
    OrderBook book(10);
    
    book.add_order(1, Side::BUY, 100, 10, 1000);
    book.add_order(2, Side::BUY, 100, 20, 1001);
    book.add_order(3, Side::BUY, 100, 30, 1002);
    
    auto& bids = book.get_bids();
    REQUIRE(bids.count(100) == 1);
    const PriceLevel& level = bids.at(100);
    
    // Cancel middle node (o2)
    book.cancel_order(2);
    
    Order* head = level.head;
    Order* tail = level.tail;
    
    REQUIRE(head->order_id == 1);
    REQUIRE(tail->order_id == 3);
    REQUIRE(head->next == tail);
    REQUIRE(tail->prev == head);
    
    // Cancel head (o1)
    book.cancel_order(1);
    REQUIRE(level.head->order_id == 3);
    REQUIRE(level.tail->order_id == 3);
    REQUIRE(level.tail->prev == nullptr);
    
    // Cancel tail (o3)
    book.cancel_order(3);
    REQUIRE(bids.count(100) == 0);
}

TEST_CASE("Price levels stay sorted after random inserts/cancels", "[OrderBook]") {
    OrderBook book(20);
    
    // Use wide spreads so they don't match
    book.add_order(1, Side::BUY, 90, 10, 1000);
    book.add_order(2, Side::BUY, 89, 10, 1001);
    book.add_order(3, Side::BUY, 91, 10, 1002);
    
    book.add_order(4, Side::SELL, 102, 10, 1003);
    book.add_order(5, Side::SELL, 105, 10, 1004);
    book.add_order(6, Side::SELL, 103, 10, 1005);
    
    auto& bids = book.get_bids();
    auto bit = bids.begin();
    REQUIRE(bit->first == 91);
    ++bit;
    REQUIRE(bit->first == 90);
    ++bit;
    REQUIRE(bit->first == 89);
    
    auto& asks = book.get_asks();
    auto ait = asks.begin();
    REQUIRE(ait->first == 102);
    ++ait;
    REQUIRE(ait->first == 103);
    ++ait;
    REQUIRE(ait->first == 105);
}

TEST_CASE("Matching Engine: Full fill", "[OrderBook]") {
    OrderBook book(10);
    book.add_order(1, Side::SELL, 100, 10, 1000); // Resting ASK
    
    auto trades = book.add_order(2, Side::BUY, 100, 10, 1001); // Aggressive BUY crosses
    
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].maker_order_id == 1);
    REQUIRE(trades[0].taker_order_id == 2);
    REQUIRE(trades[0].price == 100);
    REQUIRE(trades[0].quantity == 10);
    
    REQUIRE(book.get_asks().empty()); // ASK consumed
    REQUIRE(book.get_bids().empty()); // BUY consumed
}

TEST_CASE("Matching Engine: Partial fill", "[OrderBook]") {
    OrderBook book(10);
    book.add_order(1, Side::BUY, 100, 20, 1000); // Resting BID qty 20
    
    auto trades = book.add_order(2, Side::SELL, 100, 5, 1001); // Aggressive SELL qty 5
    
    REQUIRE(trades.size() == 1);
    REQUIRE(trades[0].maker_order_id == 1);
    REQUIRE(trades[0].taker_order_id == 2);
    REQUIRE(trades[0].quantity == 5);
    
    // Check book state
    REQUIRE(book.get_asks().empty());
    REQUIRE(book.get_bids().size() == 1);
    REQUIRE(book.get_bids().at(100).head->quantity == 15);
}

TEST_CASE("Matching Engine: Multiple price levels consumed", "[OrderBook]") {
    OrderBook book(10);
    book.add_order(1, Side::SELL, 101, 10, 1000);
    book.add_order(2, Side::SELL, 102, 10, 1001);
    book.add_order(3, Side::SELL, 103, 10, 1002);
    
    // Aggressive BUY sweeps 101 and 102
    auto trades = book.add_order(4, Side::BUY, 102, 25, 1003);
    
    REQUIRE(trades.size() == 2);
    REQUIRE(trades[0].price == 101);
    REQUIRE(trades[0].quantity == 10);
    REQUIRE(trades[1].price == 102);
    REQUIRE(trades[1].quantity == 10);
    
    REQUIRE(book.get_asks().size() == 1);
    REQUIRE(book.get_asks().count(103) == 1);
    
    REQUIRE(book.get_bids().size() == 1);
    REQUIRE(book.get_bids().at(102).head->quantity == 5); // Remainder rests at 102
}

TEST_CASE("Matching Engine: FIFO priority within same price level", "[OrderBook]") {
    OrderBook book(10);
    book.add_order(1, Side::BUY, 100, 10, 1000); // First
    book.add_order(2, Side::BUY, 100, 10, 1001); // Second
    
    auto trades = book.add_order(3, Side::SELL, 100, 15, 1002);
    
    REQUIRE(trades.size() == 2);
    // FIFO applies
    REQUIRE(trades[0].maker_order_id == 1);
    REQUIRE(trades[0].quantity == 10);
    REQUIRE(trades[1].maker_order_id == 2);
    REQUIRE(trades[1].quantity == 5);
    
    // Order 2 should have 5 qty left
    REQUIRE(book.get_bids().at(100).head->order_id == 2);
    REQUIRE(book.get_bids().at(100).head->quantity == 5);
}
