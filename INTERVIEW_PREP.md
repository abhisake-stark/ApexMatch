# ApexMatch Interview Prep & Architecture Guide

This document breaks down the *actual* implementation of the ApexMatch engine in the current workspace, providing a brutally honest guide to surviving a technical deep-dive into the codebase.

---

## 1. Core Data Structures (`include/hft/core/order.hpp`)

**1. Engineering Goal**
Minimize memory overhead and cache misses when moving orders around the system. Avoid allocating memory on the critical path by ensuring all core objects can live inside pre-allocated arrays and form relationships via raw pointers rather than heavy container classes.

**2. Execution Logic**
The `Order` struct represents a single order. Instead of storing orders in a `std::list` (which dynamically allocates nodes), the struct natively includes `Order* prev` and `Order* next` pointers. This turns the struct into an intrusive doubly-linked list node. We use fixed-point integers (`int64_t`) for price to avoid floating-point precision loss and rounding errors.

**3. Interview Grid**
*   **Q:** *Why did you use intrusive linked list pointers (`prev`/`next`) directly in the `Order` struct instead of a `std::list<Order>`?*
    **A:** A standard `std::list` dynamically allocates a node wrapper (containing the `prev`/`next` pointers and the payload) every time an item is added. By embedding the pointers directly in the `Order` struct, we completely eliminate allocation. When an order is pulled from the `ObjectPool`, it is already a node, allowing us to achieve O(1) insertion and deletion at any price level without ever calling `new` or `delete`.
*   **Q:** *Why is `price` stored as an `int64_t` instead of a `double`?*
    **A:** Floating-point math is non-deterministic and subject to precision loss (e.g., `0.1 + 0.2 != 0.3`). In finance, this can lead to catastrophic matching errors. We use fixed-point arithmetic (ticks) so that all prices are exact integers. It's also significantly faster for CPU ALUs to compare integers than floats.
*   **Q:** *What would break if you changed the `side` enum to a boolean?*
    **A:** Functionally, nothing would break since there are only two states (BUY/SELL). However, using a strongly typed `enum class` makes the code significantly more readable and prevents accidental implicit conversions or silent bugs (like passing `true` for a price argument by mistake).

---

## 2. Object Pool / Memory Management (`include/hft/core/object_pool.hpp`)

**1. Engineering Goal**
Pre-allocate all memory the engine will ever need at startup to guarantee zero runtime allocation latency during the hot loop. 

**2. Execution Logic**
The `ObjectPool<T>` constructor initializes a single contiguous `std::vector<T>` sized to `capacity`. During initialization, it threads all elements together into an intrusive singly-linked "free list" (reusing the `next` pointer of the `Order`). 
*   `acquire()`: Pops the head off the free list in O(1) time and returns a pointer. If the free list is empty, it returns `nullptr`.
*   `release(T* obj)`: Pushes the object back onto the head of the free list in O(1) time.

**3. Interview Grid**
*   **Q:** *What happens if the engine receives more concurrent orders than the `ObjectPool` capacity?*
    **A:** The `acquire()` method will return `nullptr`. In our current implementation, the TCP ingestion thread checks for `nullptr` and would drop the order, logging an error. In a real system, you must size the pool appropriately for the maximum expected book size or implement a slow-path allocation fallback (though falling back to `new` ruins deterministic latency).
*   **Q:** *Why reuse the `next` pointer for the free list instead of maintaining a `std::vector<bool>` of used slots?*
    **A:** Using a free list pointer gives us O(1) acquire and release without searching. If we used a boolean array or bitmap, we would have to scan for the next available slot, making `acquire()` O(N) in the worst case and destroying our latency budget as the book fills up.
*   **Q:** *Is your `ObjectPool` thread-safe?*
    **A:** No, it is explicitly single-threaded. In this architecture, we have two pools: one owned exclusively by the TCP ingestion thread (`ring_buffer_pool`), and another owned exclusively by the internal `OrderBook` matching thread. Since they don't share a single pool, we don't need atomics or mutexes, avoiding severe cache contention.

---

## 3. Matching Engine Logic (`include/hft/core/order_book.hpp`)

**1. Engineering Goal**
Match incoming aggressive orders against resting passive orders using strict Price-Time priority as fast as possible, and support O(1) constant-time cancellations.

**2. Execution Logic**
The `OrderBook` maintains two `std::map` structures: `bids_` (sorted `std::greater<>` so the highest price is first) and `asks_` (sorted `std::less<>` so the lowest price is first). Each `PriceLevel` contains a pointer to the `head` and `tail` of the intrusive linked list of resting orders.
When `add_order()` receives an aggressive BUY, it traverses the `asks_` map starting from the best ask. It walks the linked list at that price level, deducting quantities. If a resting order is partially filled, it remains. If fully filled, it is unlinked and released back to the `ObjectPool`.
Cancellations are O(1) because we maintain a `std::unordered_map<uint64_t, Order*>` (`order_index_`). We look up the exact pointer, unlink it from its `prev`/`next` siblings, and release it.

**3. Interview Grid**
*   **Q:** *You used `std::map` for your price levels. `std::map` is a Red-Black Tree and has O(log N) lookup. Is that fast enough for HFT?*
    **A:** For Phase 2, yes, but for ultra-low latency, no. A Red-Black tree scatters nodes across the heap, causing cache misses. In a true sub-microsecond HFT system, we would replace the `std::map` with a flat array indexed by price ticks (a direct-mapped array) where lookup is O(1) and perfectly cache-local.
*   **Q:** *Walk me through the exact pointer operations when an order in the middle of a price level's linked list is cancelled.*
    **A:** Because we have the direct pointer from `order_index_`, we access `order->prev` and `order->next`. We set `order->prev->next = order->next`, and `order->next->prev = order->prev`. We then update the `PriceLevel`'s volume, remove it from the index, and release the order. All operations are local and O(1).
*   **Q:** *Why do you return a `std::vector<Trade>` from `add_order`? Doesn't that allocate?*
    **A:** Yes, it does. In our implementation, `add_order` creates a vector of trades and returns it by value. This was a pragmatic choice for clean API boundaries. To eliminate this allocation, we would pass a pre-allocated array (or a lambda callback) into `add_order` to process trades inline as they happen.

---

## 4. Lock-Free SPSC Ring Buffer (`include/hft/net/spsc_ring_buffer.hpp`)

**1. Engineering Goal**
Pass pointers safely from the TCP ingestion thread to the Matching thread with absolute minimal latency, completely avoiding OS-level mutexes or condition variables.

**2. Execution Logic**
A template array `T buffer_[Capacity]`. `head` (producer index) and `tail` (consumer index) are `std::atomic<size_t>`.
The capacity *must* be a power of 2, allowing us to use a bitwise AND (`& (Capacity - 1)`) instead of the modulo operator (`%`), which is significantly faster on modern CPUs.
We use C++ memory models: `try_push` does a `memory_order_relaxed` read on the tail, writes the data, and does a `memory_order_release` store on the head. `try_pop` does a `memory_order_acquire` load on the head. We also pad the atomics with `alignas(std::hardware_destructive_interference_size)` (usually 64 bytes) to prevent False Sharing.

**3. Interview Grid**
*   **Q:** *Why is `memory_order_release` on the producer's `head` update critical?*
    **A:** It ensures that the actual data written into `buffer_[head]` is globally visible to other threads *before* the `head` pointer itself is updated. If we used `relaxed` for everything, the CPU or compiler could reorder instructions, causing the consumer to see the updated `head` and read garbage data from the buffer before the producer's write actually landed in memory.
*   **Q:** *What is False Sharing and how did you prevent it here?*
    **A:** False sharing occurs when two independent atomic variables (like `head` and `tail`) sit on the same 64-byte CPU cache line. When the producer modifies `head`, it invalidates the entire cache line for the consumer, forcing a slow RAM fetch even though `tail` didn't change. I prevented this by aligning `head` and `tail` to `std::hardware_destructive_interference_size`, ensuring they live on separate cache lines.
*   **Q:** *Why does the ring buffer pass `Order*` instead of the raw `char[]` bytes from the network?*
    **A:** Passing a pointer avoids copying 22 bytes of data. The TCP thread acquires an `Order` from the IPC pool, parses the bytes into it, and hands off the pointer. The matching thread reads the pointer and returns it to the pool when done.

---

## 5. TCP Ingestion Thread (`src/net/tcp_server.cpp`)

**1. Engineering Goal**
Accept binary TCP connections from trading bots, rapidly parse the fixed-size 22-byte wire protocol, and push resulting orders into the ring buffer.

**2. Execution Logic**
Uses Standalone Asio (`asio::io_context`). `async_accept` listens for incoming bots. Each connection loops on `async_read` asking for exactly 22 bytes (`[1b type][8b id][1b side][8b price][4b qty]`). The bytes are decoded using `ntohll`/`ntohl` to handle network-byte-order endianness. It then pulls an `Order*` from `ring_buffer_pool`, populates it, and calls `ring_buffer.try_push()`.

**3. Interview Grid**
*   **Q:** *You used `async_read`. Is Asio's asynchronous event loop fast enough for HFT?*
    **A:** No. `async_read` relies on epoll/kqueue (or IOCP on Windows) and callback queues, introducing OS kernel overhead and context switches. A true ultra-low latency ingestion engine uses kernel-bypass (like Solarflare OpenOnload or DPDK) to read packets directly from the NIC into user space via busy-polling. Asio was used here as a standard, portable baseline.
*   **Q:** *How do you handle endianness?*
    **A:** The wire protocol specifies Network Byte Order (Big Endian). X86 CPUs are Little Endian. I use standard POSIX macros like `ntohl` (network to host long) and a custom byte-swap for the 64-bit integers to ensure the parsed values are correct regardless of the host CPU architecture.
*   **Q:** *What happens if the ring buffer is full when `try_push` is called?*
    **A:** `try_push` returns `false` instantly. The TCP thread increments a `dropped_` counter and releases the `Order*` back to the pool to prevent memory leaks. We explicitly *do not* block or sleep, as that would freeze the ingestion of all other connections.

---

## 6. WebSocket Broadcaster Thread (`src/ws/broadcaster.cpp`)

**1. Engineering Goal**
Stream live snapshots and trade events to the React frontend in real-time.

**2. Execution Logic**
We use `websocketpp` (configured for ASIO standalone mode) to host a WS server on port 9002.
The broadcaster spins up its own thread. It maintains a set of active connection handles. It provides two methods:
1. `broadcast_trades(trades)`: Called directly by the matching engine when a trade occurs.
2. An internal ASIO timer that fires every 100ms, acquires a read lock (`std::shared_lock`) on the `OrderBook`, pulls the top 15 bids/asks, serializes them to JSON using `nlohmann/json`, and sends the snapshot to all clients.

**3. Interview Grid**
*   **Q:** *Divergence Check: The original spec requested `uWebSockets`. Why did you use `websocketpp`?*
    **A:** `uWebSockets` relies on `uSockets` and often requires `libuv` or specific Linux kernel epoll headers. Getting it to compile cleanly under MinGW on Windows is notoriously fragile and prone to deep linking errors. I opted for `websocketpp` because it is header-only, highly portable, and integrates natively with our existing Standalone Asio dependency, guaranteeing a clean build.
*   **Q:** *Your matching thread directly calls `broadcaster.broadcast_trades()`. What is the architectural flaw here?*
    **A:** This is the biggest latency flaw in the current implementation. `websocketpp` requires taking an internal mutex, and we use `nlohmann::json` to construct a string inside `broadcast_trades`. This forces the ultra-fast matching thread to pay the cost of string serialization and lock contention. In a production system, the matching thread would push raw trades into a second SPSC ring buffer, and a dedicated broadcast thread would handle the JSON formatting.
*   **Q:** *Why use a `std::shared_mutex` for the OrderBook?*
    **A:** It allows multiple readers (e.g., the 100ms snapshot timer) to safely traverse the tree simultaneously using a `std::shared_lock`, while the matching thread takes an exclusive `std::unique_lock` to modify the book. This prevents the snapshot generator from crashing due to iterator invalidation if a match occurs mid-snapshot.

---

## 7. Python Bot Simulators (`bots/`)

**1. Engineering Goal**
Generate realistic background traffic to stress-test the C++ engine.

**2. Execution Logic**
Uses Python 3.11+ `asyncio`. 
`market_maker.py` maintains quotes on both sides of a drifting mid-price.
`aggressive_taker.py` blasts marketable orders to force matches.
`protocol.py` strictly uses `struct.pack("!B Q B q I", ...)` to enforce the Big Endian 22-byte network format.

**3. Interview Grid**
*   **Q:** *Why use `struct.pack` instead of JSON for the bots?*
    **A:** The engine expects a raw binary protocol. JSON has massive parsing overhead and unpredictable payload sizes. A fixed 22-byte binary struct is infinitely faster for the C++ engine to decode.
*   **Q:** *How do you simulate the market maker cancelling orders?*
    **A:** The bots track their own sent `order_id`s in a local dictionary. When the mid-price drifts, the bot sends a new 22-byte payload with `type=1` (CANCEL), `qty=0`, and the original `order_id`.
*   **Q:** *Why does the orchestrator crash or show "Connection refused" on Windows when pushed to 10,000 orders/sec?*
    **A:** Python's `asyncio` on Windows uses `ProactorEventLoop`, which can exhaust ephemeral ports or hit WSAENOBUFS limits when opening/writing too many sockets concurrently. The engine itself is fine, but the local OS network stack starts rejecting the Python bot's aggressive loopback connections.

---

## 8. React Frontend (`frontend/`)

**1. Engineering Goal**
Visualize the limit order book, recent trades, and market depth in real-time.

**2. Execution Logic**
Vite + React + TS dashboard. `useOrderBookSocket.ts` maintains the raw WebSocket connection. We use an exponential backoff algorithm to aggressively reconnect if the engine drops. `MarketDepthChart.tsx` uses `lightweight-charts` to accumulate resting quantities and plot a step-area chart.

**3. Interview Grid**
*   **Q:** *Why doesn't the React frontend reconstruct the order book locally using the trade events?*
    **A:** The frontend is strictly a "dumb client". The C++ backend is the sole source of truth and sends 100ms full snapshots. Computing book state locally on the client introduces desynchronization risks, requires complex sequence tracking, and burns client CPU.
*   **Q:** *How did you implement the exponential backoff reconnect?*
    **A:** In the `ws.onclose` handler, we use `setTimeout` to call `connect()` after `backoffRef.current` milliseconds. We then multiply the ref by 1.5, capping it at a max of 10 seconds. When `ws.onopen` fires, we reset the backoff back to 1 second.
*   **Q:** *How do you prevent the `trades` array from causing an out-of-memory error over a long session?*
    **A:** Inside the React state setter, we unshift the new trade to the array and explicitly truncate the array: `if (newTrades.length > 50) newTrades.length = 50;`. This acts as a bounded rolling window.

---

## 9. Latency Profiling (`scripts/latency_bench.py` & `main.cpp`)

**1. Engineering Goal**
Identify the true bottlenecks in the system by comparing internal matching speed against end-to-end network latency.

**2. Execution Logic**
`latency_bench.py` sends a resting order at an extreme price (e.g., 500,000), then sends an aggressive probe order to match it, recording the nanosecond timestamp right before socket write. It listens on the WS port for that specific price to arrive and calculates the delta. 
Internally, `main.cpp` uses a log-bucketed array to time the gap between a successful `try_pop` and the return of `add_order()`.

**3. Interview Grid**
*   **Q:** *Why did you use a log-bucketed array for the internal histogram instead of just keeping a list of all latencies and sorting them?*
    **A:** Keeping a list of every latency observation requires dynamic memory allocation (`push_back`), which would destroy the very latency we are trying to measure. A log-bucketed array uses a fixed-size `std::vector<uint64_t>` (32 elements) and simply increments an integer using `std::log2(ns)`, running in O(1) time with virtually zero overhead.
*   **Q:** *The E2E latency is ~2000µs (p99), but the matching latency is ~32µs (p99). Where is the missing time going?*
    **A:** The missing 1.9ms is swallowed by the OS network stack (TCP windowing, loopback routing), the Python socket overhead, the thread context switch from TCP ingestion to Matching, and primarily the `nlohmann::json` string serialization blocking the websocket broadcast.
*   **Q:** *Why did we probe at price 500,000 during the benchmark?*
    **A:** The WS protocol for trades does not include an `order_id`. To definitively correlate the exact moment our probe order crossed, we execute the trade at a bizarre price far away from the active market maker bots (~10,000). When the WS emits a trade at 500,000, we know with 100% certainty it belongs to our benchmark probe.

---

## Known Gaps / Honest Answers
If pressed on limitations during an interview, admit these freely:
1. **Dependency Substitution:** The original spec asked for `uWebSockets`. I substituted `websocketpp` due to notorious MinGW compilation issues with `uSockets/libuv`. 
2. **Serialization on the Critical Path:** The matching thread calls `broadcaster.broadcast_trades(...)` directly. This holds up the next match while JSON is generated and ASIO locks are taken. In production, this must be decoupled via another SPSC queue.
3. **`std::map` usage:** While O(log N) is okay for a prototype, true HFT requires flat array direct-mapping for price levels to ensure cache locality.
4. **Lack of Kernel Bypass:** The TCP ingestion uses standard OS sockets via Asio. Real systems bypass the OS entirely (e.g. Solarflare).
5. **No Thread Pinning:** We didn't use `pthread_setaffinity_np` to pin the matching thread to an isolated CPU core, meaning OS scheduler jitter is still present.
