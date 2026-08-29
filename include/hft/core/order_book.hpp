#pragma once
#include <map>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <vector>
#include <algorithm>
#include <shared_mutex>
#include "hft/core/order.hpp"
#include "hft/core/price_level.hpp"
#include "hft/core/object_pool.hpp"
#include "hft/core/trade.hpp"

namespace hft::core {

class OrderBook {
public:
    explicit OrderBook(size_t max_orders) : order_pool_(max_orders), next_trade_id_(1) {}

    std::vector<Trade> add_order(uint64_t order_id, Side side, int64_t price, uint32_t quantity, uint64_t timestamp) {
        std::vector<Trade> trades;
        
        if (side == Side::BUY) {
            auto it = asks_.begin();
            while (it != asks_.end() && quantity > 0 && it->first <= price) {
                PriceLevel& level = it->second;
                Order* resting = level.head;
                
                while (resting != nullptr && quantity > 0) {
                    uint32_t fill_qty = std::min(quantity, resting->quantity);
                    
                    trades.push_back(Trade{
                        next_trade_id_++,
                        resting->order_id,
                        order_id,
                        resting->price,
                        fill_qty,
                        timestamp
                    });
                    
                    quantity -= fill_qty;
                    resting->quantity -= fill_qty;
                    
                    Order* next_resting = resting->next;
                    
                    if (resting->quantity == 0) {
                        level.remove(resting);
                        order_index_.erase(resting->order_id);
                        order_pool_.release(resting);
                    }
                    
                    resting = next_resting;
                }
                
                if (level.empty()) {
                    it = asks_.erase(it);
                } else {
                    ++it;
                }
            }
        } else { // SELL
            auto it = bids_.begin();
            while (it != bids_.end() && quantity > 0 && it->first >= price) {
                PriceLevel& level = it->second;
                Order* resting = level.head;
                
                while (resting != nullptr && quantity > 0) {
                    uint32_t fill_qty = std::min(quantity, resting->quantity);
                    
                    trades.push_back(Trade{
                        next_trade_id_++,
                        resting->order_id,
                        order_id,
                        resting->price,
                        fill_qty,
                        timestamp
                    });
                    
                    quantity -= fill_qty;
                    resting->quantity -= fill_qty;
                    
                    Order* next_resting = resting->next;
                    
                    if (resting->quantity == 0) {
                        level.remove(resting);
                        order_index_.erase(resting->order_id);
                        order_pool_.release(resting);
                    }
                    
                    resting = next_resting;
                }
                
                if (level.empty()) {
                    it = bids_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        if (quantity > 0) {
            Order* order = order_pool_.acquire();
            order->order_id = order_id;
            order->side = side;
            order->price = price;
            order->quantity = quantity;
            order->timestamp = timestamp;
            
            order_index_[order_id] = order;

            if (side == Side::BUY) {
                bids_[price].push_back(order);
            } else {
                asks_[price].push_back(order);
            }
        }
        
        return trades;
    }

    void cancel_order(uint64_t order_id) {
        auto it = order_index_.find(order_id);
        if (it == order_index_.end()) return;

        Order* order = it->second;
        order_index_.erase(it);

        if (order->side == Side::BUY) {
            auto& level = bids_[order->price];
            level.remove(order);
            if (level.empty()) {
                bids_.erase(order->price);
            }
        } else {
            auto& level = asks_[order->price];
            level.remove(order);
            if (level.empty()) {
                asks_.erase(order->price);
            }
        }

        order_pool_.release(order);
    }
    
    // Test & Broadcaster accessors
    const std::map<int64_t, PriceLevel, std::greater<int64_t>>& get_bids() const { return bids_; }
    const std::map<int64_t, PriceLevel>& get_asks() const { return asks_; }
    
    std::shared_mutex& get_lock() const { return rw_lock_; }

private:
    mutable std::shared_mutex rw_lock_;
    std::map<int64_t, PriceLevel, std::greater<int64_t>> bids_;
    std::map<int64_t, PriceLevel> asks_;
    
    std::unordered_map<uint64_t, Order*> order_index_;
    ObjectPool<Order> order_pool_;
    uint64_t next_trade_id_;
};

} // namespace hft::core
