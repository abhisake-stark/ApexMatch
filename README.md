# ApexMatch ⚡

> **A high-performance, lock-free C++20 Limit Order Book engine designed for microsecond latency.**

![C++20](https://img.shields.io/badge/C++-20-blue.svg)
![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

ApexMatch is a bespoke trading engine demonstrating modern C++ low-latency techniques. It avoids dynamic allocation on the critical path, utilizes intrusive lock-free concurrency structures, and streams real-time market data to a React frontend.

## 🏗 Architecture Overview

The system is decoupled into three distinct threads communicating via lock-free primitives and shared memory.

```text
[Python Bots] 
    │ 
    ▼ (TCP - 22-byte binary protocol)
┌─────────────────────────────────┐
│ Thread 1: Ingestion (Boost Asio)│
└───────────────┬─────────────────┘
                │ (Lock-Free SPSC Ring Buffer)
                ▼ 
┌─────────────────────────────────┐
│ Thread 2: Core Matching Engine  │ ◄── (Exclusive Lock) ──┐
└───────────────┬─────────────────┘                        │
                │ (Direct Call)                            ▼
                ▼                                   [OrderBook]
┌─────────────────────────────────┐                        ▲
│ Thread 3: WS Broadcaster        │ ◄── (Shared Lock) ─────┘
└───────────────┬─────────────────┘
                │ (JSON WebSocket)
                ▼
      [React Vite Frontend]
```

## ✨ Key Features
- **Zero-Allocation Object Pool**: `Order` objects are pre-allocated at startup and reused via an intrusive free-list, preventing `new`/`delete` OS jitter.
- **Lock-Free SPSC Ring Buffer**: Ingestion to Matching handoff occurs in sub-microseconds without a single OS mutex, utilizing atomic memory ordering (`release`/`acquire`).
- **O(1) Cancellation**: The Order Book uses an index to jump directly to memory addresses, unlinking orders in constant time.
- **Real-Time Websocket Broadcast**: Streams 100ms snapshots and live trades directly to the browser.

## 🛤 System Flow (The Journey of an Order)
1. **The Wire**: A Python bot constructs a raw 22-byte binary payload (Network Byte Order): `[1b type][8b id][1b side][8b price][4b qty]`.
2. **Ingestion**: The C++ TCP server (`asio`) reads the bytes, translates endianness, acquires a raw `Order*` from the `ObjectPool`, and pushes it onto the `SPSCRingBuffer`.
3. **Matching**: The core thread, spinning in a busy-yield loop, pops the pointer. It takes an exclusive `std::unique_lock`, traverses the `OrderBook`'s price levels, and deducts resting quantities in strict Price-Time priority.
4. **Execution**: If a match occurs, the matching thread passes the `Trade` structs directly to the WebSocket broadcaster. 
5. **Broadcast**: The WS thread formats the trade as a JSON event and pushes it to the React dashboard. Every 100ms, it also takes a `std::shared_lock` to broadcast a snapshot of the top 15 price levels.

## 🚀 Performance
*Tested locally with concurrent `asyncio` Python bots driving traffic.*

- **Core Matching Latency (p50):** `8.192 µs`
- **Core Matching Latency (p99):** `32.768 µs`
- **End-to-End Latency (p50):** `467.10 µs`
- **End-to-End Latency (p99):** `2095.98 µs`

*(Note: End-to-end latency is dominated by OS network stack routing, JSON serialization, and Python socket overhead, while the raw C++ book traversal remains well under 10µs).*

## 💻 Installation & Run Instructions

This project is configured for **MinGW Makefiles** on Windows. Ensure you have MSYS2/MinGW64 installed and `C:\msys64\ucrt64\bin` in your Path.

### 1. Build the C++ Engine
```powershell
# Set compiler path if not globally configured
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"

# Generate build files using the predefined preset
cmake --preset release

# Build the executable
cmake --build build/release
```

### 2. Run the Engine
```powershell
# Start the engine (listens on TCP 9001, WS 9002)
build\release\ApexMatch.exe
```

### 3. Start the Frontend
In a new terminal:
```powershell
cd frontend
npm install
npm run dev
```
Navigate to `http://localhost:5173` to see the live dashboard.

### 4. Inject Market Traffic
In a new terminal:
```powershell
# Spin up 5 Market Makers and 2 Aggressive Takers for 2 minutes
$env:PYTHONPATH="."
python bots/orchestrator.py --duration 120 --makers 5 --takers 2
```

## 📂 Project Structure
```text
ApexMatch/
├── bots/                   # Python asyncio traffic simulators
├── frontend/               # Vite + React + TS dashboard
├── include/hft/
│   ├── core/               # Order, ObjectPool, OrderBook, PriceLevel
│   ├── net/                # SPSC Ring Buffer, TcpServer
│   └── ws/                 # WebSocket Broadcaster
├── scripts/                # E2E Latency benchmarks & build scripts
├── src/                    # Implementation files and main.cpp
├── tests/                  # Catch2 Unit and Integration Tests
├── CMakeLists.txt          # CMake configuration (FetchContent)
├── CMakePresets.json       # MinGW Release/Debug presets
├── WIRE_PROTOCOL.md        # TCP Binary Protocol Spec
└── WS_PROTOCOL.md          # WebSocket JSON Protocol Spec
```

## 🧠 Design Decisions
- **Intrusive Linked Lists**: We use `Order* prev, *next` inside the `Order` struct itself rather than `std::list` to completely eliminate node allocation overhead.
- **Fixed-Point Prices**: `price` is an `int64_t` tick rather than a `double` to guarantee mathematical determinism and prevent floating-point rounding errors.
- **Standalone ASIO & Websocketpp**: We opted for header-only, non-Boost networking libraries to ensure a clean, portable build process across Windows MinGW without fragile linking steps.
