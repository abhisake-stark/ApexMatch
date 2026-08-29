#pragma once
#include "hft/core/order.hpp"

namespace hft::core {

class PriceLevel {
public:
    Order* head{nullptr};
    Order* tail{nullptr};

    void push_back(Order* order) {
        if (!head) {
            head = tail = order;
            order->prev = nullptr;
            order->next = nullptr;
        } else {
            tail->next = order;
            order->prev = tail;
            order->next = nullptr;
            tail = order;
        }
    }

    void remove(Order* order) {
        if (order->prev) {
            order->prev->next = order->next;
        } else {
            head = order->next;
        }

        if (order->next) {
            order->next->prev = order->prev;
        } else {
            tail = order->prev;
        }
        
        order->prev = nullptr;
        order->next = nullptr;
    }

    bool empty() const {
        return head == nullptr;
    }
};

} // namespace hft::core
