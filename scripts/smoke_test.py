import asyncio
import websockets
import socket
import struct
import time
import json

def pack_order(type_val, order_id, side, price, qty):
    # [1 byte type][8 bytes order_id][1 byte side][8 bytes price][4 bytes quantity]
    # > = big-endian
    return struct.pack('>B Q B Q I', type_val, order_id, side, price, qty)

async def test_ws_and_tcp():
    print("Connecting to WS on port 9002...")
    async with websockets.connect("ws://127.0.0.1:9002") as ws:
        print("Connected to WS. Waiting for first snapshot...")
        msg = await ws.recv()
        print("Initial WS message:", msg)
        
        print("Connecting to TCP on port 9001...")
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', 9001))
        
        print("Sending BUY order...")
        t0 = time.time()
        s.sendall(pack_order(0, 1, 0, 10000, 100)) # BUY 100 @ 10000
        
        print("Sending SELL order that crosses...")
        # A short sleep ensures the BUY rests in the book before SELL hits
        time.sleep(0.1)
        
        t1 = time.time()
        s.sendall(pack_order(0, 2, 1, 10000, 50)) # SELL 50 @ 10000
        
        print("Waiting for WS trade message...")
        while True:
            msg = await ws.recv()
            data = json.loads(msg)
            if data['type'] == 'trade':
                t2 = time.time()
                print(f"Trade received! End-to-end latency: {(t2 - t1)*1000:.2f} ms")
                print("Trade data:", data)
                break
            else:
                print("Received snapshot...")
                
        # Get one more snapshot to see the book state
        msg = await ws.recv()
        print("Final snapshot:", msg)
        s.close()

if __name__ == "__main__":
    asyncio.run(test_ws_and_tcp())
