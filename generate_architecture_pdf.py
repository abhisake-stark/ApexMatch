from markdown_pdf import Section, MarkdownPdf
import os

md_content = """# ApexMatch: High-Frequency Trading Engine Architecture

## 1. High-Level Overview
ApexMatch is an ultra-low-latency, High-Frequency Trading (HFT) Limit Order Book (LOB) matching engine. It acts as the core backend infrastructure for a financial exchange, taking incoming buy/sell orders, organizing them by price and time, and executing trades instantly when prices cross.

## 2. End-to-End System Flow
The architecture is designed to handle millions of messages per second without freezing or slowing down. Here is how data flows through the system:

1. **Order Generation (Python Asyncio Bots):** 
   Automated market makers and retail taker scripts constantly send binary data representing `BUY` and `SELL` limit orders over TCP connections.
2. **TCP Ingestion (C++ Network Thread):** 
   The `tcp_server` (built with Standalone Asio) receives incoming raw bytes, deserializes them, and translates them into C++ `Order` objects using a pre-allocated Object Pool.
3. **Lock-Free Handoff (SPSC Ring Buffer):** 
   To avoid slowing down the networking thread, the system avoids OS mutex locks. Instead, the `tcp_server` pushes the new order into a Single-Producer Single-Consumer (SPSC) Ring Buffer using atomic hardware instructions.
4. **The Matching Engine (C++ Core Thread):** 
   A dedicated matching thread constantly polls the ring buffer. When an order arrives, it is inserted into the `OrderBook`. If a BUY matches a SELL, a trade is instantly executed.
5. **Broadcasting (C++ WebSocket Thread):** 
   The `broadcaster` thread packages market depth changes and executed trades into JSON and pushes them out via WebSockets (using Websocketpp).
6. **Data Visualization (React Frontend):** 
   A Vite + React application ingests the high-frequency WebSocket stream, updating a live TradingView-style Market Depth chart and a Time & Sales trade tape at 60 FPS.

## 3. Core Technologies Used
* **C++20:** The core matching engine, optimized for microsecond-latency.
* **Standalone Asio:** Non-blocking TCP socket networking.
* **Websocketpp:** Broadcasting JSON market updates.
* **Python (asyncio):** High-concurrency market simulation bots.
* **React, TypeScript, Vite:** Real-time frontend dashboard.
* **Lightweight-Charts:** High-performance HTML5 canvas financial charting.

## 4. Advanced Optimizations
* **Zero Garbage Collection (Object Pooling):** The engine avoids latency spikes caused by `new` and `delete`. At startup, 100,000 orders are pre-allocated in memory. When an order is created or destroyed, it simply cycles in and out of a recycled "free list".
* **Intrusive Linked Lists:** Price levels in the order book do not use standard `std::list` (which allocates memory per node). Instead, `prev` and `next` pointers are embedded directly into the `Order` structs, allowing order manipulation through pure pointer swapping.
* **Fixed-Point Determinism:** Prices are represented as 64-bit integers (`int64_t`) instead of floats, eliminating IEEE 754 precision loss and guaranteeing perfect mathematical determinism.
"""

def generate_pdf():
    try:
        # Create PDF with a Table of Contents
        pdf = MarkdownPdf(toc_level=1)
        pdf.add_section(Section(md_content))
        
        # Save to current directory
        output_path = os.path.join(os.getcwd(), "ApexMatch_Architecture_Flow.pdf")
        pdf.save(output_path)
        print(f"Successfully generated {output_path}")
    except Exception as e:
        print(f"Error generating PDF: {e}")

if __name__ == "__main__":
    generate_pdf()
