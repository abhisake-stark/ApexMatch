#pragma once
#include <cstdint>

namespace hft::core {

enum class Side {
    BUY,
    SELL
};

struct Order {
    uint64_t order_id;
    Side side;
    int64_t price;
    uint32_t quantity;
    uint64_t timestamp;

    Order* prev{nullptr};
    Order* next{nullptr};
};

} // namespace hft::core
