import asyncio
import argparse
from protocol import pack_order

async def async_input(prompt):
    loop = asyncio.get_event_loop()
    return await loop.run_in_executor(None, input, prompt)

async def main():
    parser = argparse.ArgumentParser(description="Manual Trader Terminal for StarkLOB")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9001)
    args = parser.parse_args()

    print("==========================================")
    print("🚀 StarkLOB Manual Trader Terminal")
    print("==========================================")
    print("Commands:")
    print("  b <qty> <price>   -> Buy Limit Order  (e.g., 'b 10 9900')")
    print("  s <qty> <price>   -> Sell Limit Order (e.g., 's 10 9950')")
    print("  c <id> <side> <p> -> Cancel Order     (e.g., 'c 999000001 0 9900')")
    print("  exit              -> Quit")
    print("==========================================\n")
    
    try:
        reader, writer = await asyncio.open_connection(args.host, args.port)
        print(f"✅ Connected to Matching Engine at {args.host}:{args.port}\n")
    except Exception as e:
        print(f"❌ Failed to connect to engine at {args.host}:{args.port}")
        return

    # Use a very high base ID to avoid colliding with the orchestrator's bots
    base_order_id = 999000000
    order_seq = 1
    
    while True:
        try:
            line = await async_input("trade> ")
            if not line.strip(): 
                continue
                
            parts = line.strip().split()
            cmd = parts[0].lower()
            
            if cmd == "exit":
                break
                
            elif cmd in ("b", "buy", "s", "sell"):
                if len(parts) != 3:
                    print("⚠️ Usage: b <qty> <price>")
                    continue
                    
                side = 0 if cmd in ("b", "buy") else 1
                qty = int(parts[1])
                price = int(float(parts[2]))
                
                order_id = base_order_id + order_seq
                order_seq += 1
                
                writer.write(pack_order(0, order_id, side, price, qty))
                await writer.drain()
                
                side_str = "BUY" if side == 0 else "SELL"
                print(f"✅ Sent {side_str} {qty} @ {price} (Order ID: {order_id})")
                
            elif cmd in ("c", "cancel"):
                if len(parts) != 4:
                    print("⚠️ Usage: c <order_id> <side: 0=buy, 1=sell> <price>")
                    continue
                    
                order_id = int(parts[1])
                side = int(parts[2])
                price = int(float(parts[3]))
                
                writer.write(pack_order(1, order_id, side, price, 0))
                await writer.drain()
                
                print(f"✅ Sent CANCEL for Order ID: {order_id}")
                
            else:
                print("❌ Unknown command. Use: b, s, c, or exit.")
                
        except ValueError:
            print("❌ Invalid number format. Quantities and prices must be numbers.")
        except Exception as e:
            print(f"❌ Error: {e}")
            
    writer.close()
    await writer.wait_closed()
    print("Disconnected.")

if __name__ == "__main__":
    # Suppress verbose asyncio errors on Windows when forcefully closed
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nExiting.")
