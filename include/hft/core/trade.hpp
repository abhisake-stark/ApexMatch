#pragma once
#include <cstdint>

namespace hft::core {

struct Trade {
    uint64_t trade_id;
    uint64_t maker_order_id;
    uint64_t taker_order_id;
    int64_t price;
    uint32_t quantity;
    uint64_t timestamp;
};

} // namespace hft::core
