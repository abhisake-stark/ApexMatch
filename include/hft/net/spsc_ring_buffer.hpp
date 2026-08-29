#pragma once
#include <atomic>
#include <array>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <new>

namespace hft::net {

#ifdef __cpp_lib_hardware_interference_size
    inline constexpr size_t cacheline_size = std::hardware_destructive_interference_size;
#else
    inline constexpr size_t cacheline_size = 64;
#endif

template <typename T, size_t Capacity>
class SPSCRingBuffer {
    static_assert((Capacity > 0) && ((Capacity & (Capacity - 1)) == 0), "Capacity must be a power of 2");

public:
    SPSCRingBuffer() : head_(0), tail_(0) {}

    // Disable copy/move
    SPSCRingBuffer(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;

    bool try_push(T&& item) {
        // Read tail with relaxed ordering as it's only modified by the producer (this thread).
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        
        // Read head with acquire ordering to ensure all consumer's previous reads from the 
        // buffer are visible to this thread before we try to overwrite a slot.
        // It synchronizes with the consumer's release-store on head_.
        const size_t current_head = head_.load(std::memory_order_acquire);

        // Check if buffer is full. Because Capacity is a power of 2, 
        // current_tail - current_head == Capacity means it's full.
        if (current_tail - current_head >= Capacity) {
            return false;
        }

        // Write the item into the buffer array.
        buffer_[current_tail & (Capacity - 1)] = std::move(item);

        // Update tail with release ordering. This publishes the written item to the consumer.
        // Any subsequent acquire-load of tail_ by the consumer will see this write.
        tail_.store(current_tail + 1, std::memory_order_release);

        return true;
    }

    bool try_pop(T& item) {
        // Read head with relaxed ordering as it's only modified by the consumer (this thread).
        const size_t current_head = head_.load(std::memory_order_relaxed);
        
        // Read tail with acquire ordering to ensure any writes to the buffer by the producer
        // are visible to this thread before we attempt to read them.
        // It synchronizes with the producer's release-store on tail_.
        const size_t current_tail = tail_.load(std::memory_order_acquire);

        // Check if buffer is empty.
        if (current_head == current_tail) {
            return false;
        }

        // Read the item from the buffer array.
        item = std::move(buffer_[current_head & (Capacity - 1)]);

        // Update head with release ordering. This publishes that the slot is now free
        // and can be safely overwritten by the producer. 
        // Any subsequent acquire-load of head_ by the producer will see this.
        head_.store(current_head + 1, std::memory_order_release);

        return true;
    }

private:
    // Force padding between head and tail to avoid false sharing.
    // The consumer writes to head_, the producer writes to tail_.
    alignas(cacheline_size) std::atomic<size_t> head_;
    alignas(cacheline_size) std::atomic<size_t> tail_;
    
    // The buffer itself
    alignas(cacheline_size) std::array<T, Capacity> buffer_;
};

} // namespace hft::net
