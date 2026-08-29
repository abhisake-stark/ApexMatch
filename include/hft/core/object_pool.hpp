#pragma once
#include <vector>
#include <cassert>
#include <mutex>

namespace hft::core {

template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(size_t capacity) {
        pool_.resize(capacity);
        if (capacity > 0) {
            for (size_t i = 0; i < capacity - 1; ++i) {
                pool_[i].next = &pool_[i + 1];
            }
            pool_.back().next = nullptr;
            free_list_ = &pool_[0];
        } else {
            free_list_ = nullptr;
        }
    }

    // No copy/move allowed
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) = delete;
    ObjectPool& operator=(ObjectPool&&) = delete;

    T* acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (free_list_ == nullptr) return nullptr;
        
        T* obj = free_list_;
        free_list_ = obj->next;
        
        // Clear intrusive pointers before returning
        obj->prev = nullptr;
        obj->next = nullptr;
        return obj;
    }

    void release(T* obj) {
        if (!obj) return;
        std::lock_guard<std::mutex> lock(mutex_);
        
        obj->next = free_list_;
        free_list_ = obj;
    }

    // Mostly for testing capacity limits
    bool is_exhausted() const {
        return free_list_ == nullptr;
    }

private:
    std::vector<T> pool_;
    T* free_list_{nullptr};
    std::mutex mutex_;
};

} // namespace hft::core
