import asyncio
import struct
import time
import json
import numpy as np

# [1 byte type][8 bytes order_id][1 byte side][8 bytes price][4 bytes quantity] = 22 bytes
FMT = "!B Q B q I"

async def ws_listener(ws_url, expected_prices, latencies, stop_event):
    import websockets
    try:
        async with websockets.connect(ws_url) as ws:
            while not stop_event.is_set():
                try:
                    msg = await asyncio.wait_for(ws.recv(), timeout=0.1)
                    data = json.loads(msg)
                    if data.get("type") == "trade":
                        recv_ts = time.perf_counter_ns()
                        price = data.get("price")
                        qty = data.get("qty")
                        
                        # Match by unique price
                        if price in expected_prices and qty == 1:
                            send_ts = expected_prices[price]
                            latencies.append((recv_ts - send_ts) / 1000.0) # in microseconds
                            del expected_prices[price]
                except asyncio.TimeoutError:
                    continue
    except Exception as e:
        print(f"WS error: {e}")

async def tcp_sender(host, port, expected_prices, num_probes, stop_event):
    reader, writer = await asyncio.open_connection(host, port)
    
    # We will send a probe order that crosses immediately.
    # To ensure it crosses, we'll place a resting order at an extreme price first.
    # We use very high prices to not interfere with the bots trading around 10000.
    
    base_price = 500000
    
    for i in range(num_probes):
        # 1. Place a resting SELL order at base_price + i
        resting_price = base_price + i
        resting_id = 9000000 + i
        msg = struct.pack(FMT, 0, resting_id, 1, resting_price, 1) # type 0, SELL, qty 1
        writer.write(msg)
        await writer.drain()
        
        # Wait a bit for the resting order to settle
        await asyncio.sleep(0.01)
        
        # 2. Place an aggressive BUY order at base_price + i
        probe_id = 8000000 + i
        msg = struct.pack(FMT, 0, probe_id, 0, resting_price, 1) # type 0, BUY, qty 1
        
        # Record time right before write
        send_ts = time.perf_counter_ns()
        writer.write(msg)
        await writer.drain()
        
        expected_prices[resting_price] = send_ts
        
        # Send probes at ~10Hz
        await asyncio.sleep(0.1)
        
    stop_event.set()
    writer.close()
    await writer.wait_closed()

async def run_benchmark(num_probes=100):
    expected_prices = {}
    latencies = []
    stop_event = asyncio.Event()
    
    print(f"Starting latency benchmark with {num_probes} probes...")
    
    ws_task = asyncio.create_task(ws_listener("ws://127.0.0.1:9002", expected_prices, latencies, stop_event))
    tcp_task = asyncio.create_task(tcp_sender("127.0.0.1", 9001, expected_prices, num_probes, stop_event))
    
    await asyncio.gather(ws_task, tcp_task)
    
    if len(latencies) == 0:
        print("No matches recorded.")
        return
        
    p50 = np.percentile(latencies, 50)
    p95 = np.percentile(latencies, 95)
    p99 = np.percentile(latencies, 99)
    p999 = np.percentile(latencies, 99.9)
    
    print("\n--- End-to-End Latency Results (microseconds) ---")
    print(f"Probes captured: {len(latencies)} / {num_probes}")
    print(f"p50:   {p50:.2f} us")
    print(f"p95:   {p95:.2f} us")
    print(f"p99:   {p99:.2f} us")
    print(f"p99.9: {p999:.2f} us")

if __name__ == "__main__":
    asyncio.run(run_benchmark(200))
