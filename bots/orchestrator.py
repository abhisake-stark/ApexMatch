import asyncio
import argparse
import random
import math
from bots.protocol import pack_order

# Global state for the simulation
class SimulationState:
    def __init__(self, start_price: int):
        self.true_price = start_price
        self.volatility = 0.0005 # Baseline volatility

async def run_price_simulation(state: SimulationState):
    """Simulates the true market price using a random walk with occasional volatility jumps."""
    while True:
        # GBM-like random walk on the integer price
        # dz = normal distribution
        drift = 0.0
        shock = random.gauss(0, state.volatility)
        
        # 1% chance of a volatility jump (macro event / institutional flow)
        if random.random() < 0.01:
            shock = random.gauss(0, state.volatility * 10)
        
        pct_change = drift + shock
        state.true_price = int(state.true_price * (1 + pct_change))
        
        # Keep price sane
        if state.true_price < 1000:
            state.true_price = 1000
            
        await asyncio.sleep(2.0) # Slower price drift

async def run_market_maker(bot_id: int, host: str, port: int, tick_interval: float, spread: int, state: SimulationState, stats: dict):
    stats.update({'new': 0, 'cancel': 0, 'buy': 0, 'sell': 0})
    writer = None
    
    base_order_id = bot_id * 1000000
    order_seq = 1
    
    current_bid_id = None
    current_ask_id = None
    current_bid_price = None
    current_ask_price = None
    
    # MM inventory simulation (to skew quotes)
    inventory = 0

    while True:
        try:
            if writer is None:
                reader, writer = await asyncio.open_connection(host, port)
                
            # Quote around the true price, skewed by inventory
            # If inventory is positive (long), lower the ask to sell, lower the bid to avoid buying
            skew = int(-inventory * 0.1) 
            
            # MM adds some noise to its spread
            my_spread = spread + random.randint(0, 4)
            
            new_bid_price = state.true_price - (my_spread // 2) + skew
            new_ask_price = state.true_price + (my_spread // 2) + skew
            
            # Avoid crossing own quotes due to skew
            if new_bid_price >= new_ask_price:
                new_ask_price = new_bid_price + 1
            
            # Only replace if price changed significantly or randomly to refresh queue position
            replace_bid = current_bid_price is None or abs(current_bid_price - new_bid_price) > 2 or random.random() < 0.1
            replace_ask = current_ask_price is None or abs(current_ask_price - new_ask_price) > 2 or random.random() < 0.1
            
            # Cancel old quotes
            if replace_bid and current_bid_id is not None:
                writer.write(pack_order(1, current_bid_id, 0, current_bid_price, 0))
                stats['cancel'] += 1
            if replace_ask and current_ask_id is not None:
                writer.write(pack_order(1, current_ask_id, 1, current_ask_price, 0))
                stats['cancel'] += 1
            
            if replace_bid or replace_ask:
                await writer.drain()
            
            # Place new quotes
            qty = random.randint(50, 200)
            
            if replace_bid:
                current_bid_id = base_order_id + order_seq
                current_bid_price = new_bid_price
                order_seq += 1
                writer.write(pack_order(0, current_bid_id, 0, current_bid_price, qty))
                stats['new'] += 1
                stats['buy'] += 1
                
            if replace_ask:
                current_ask_id = base_order_id + order_seq
                current_ask_price = new_ask_price
                order_seq += 1
                writer.write(pack_order(0, current_ask_id, 1, current_ask_price, qty))
                stats['new'] += 1
                stats['sell'] += 1
                
            if replace_bid or replace_ask:
                await writer.drain()
                
            # Simulate inventory drift towards 0 (mean reversion of assumed fills)
            if inventory > 0: inventory -= 1
            elif inventory < 0: inventory += 1
                
            await asyncio.sleep(tick_interval)
            
        except asyncio.CancelledError:
            if writer is not None:
                writer.close()
                await writer.wait_closed()
            break
        except Exception as e:
            if writer is not None:
                writer.close()
                writer = None
            await asyncio.sleep(1.0)

async def run_retail_taker(bot_id: int, host: str, port: int, tick_interval: float, state: SimulationState, stats: dict):
    """Retail flow: Frequent, small random market orders."""
    stats.update({'new': 0, 'cancel': 0, 'buy': 0, 'sell': 0})
    writer = None
    base_order_id = (100 + bot_id) * 1000000
    order_seq = 1

    while True:
        try:
            if writer is None:
                reader, writer = await asyncio.open_connection(host, port)
                
            await asyncio.sleep(tick_interval * random.uniform(0.5, 1.5))
            
            side = random.choice([0, 1])
            qty = random.randint(1, 15) # Small retail sizes
            
            # Aggressive limit order (effectively market order)
            price = state.true_price + 100 if side == 0 else state.true_price - 100
            
            order_id = base_order_id + order_seq
            order_seq += 1
            
            writer.write(pack_order(0, order_id, side, price, qty))
            await writer.drain()
            
            stats['new'] += 1
            if side == 0: stats['buy'] += 1
            else: stats['sell'] += 1
            
        except asyncio.CancelledError:
            if writer is not None: writer.close()
            break
        except Exception as e:
            if writer is not None: writer = None
            await asyncio.sleep(1.0)

async def run_institutional_taker(bot_id: int, host: str, port: int, state: SimulationState, stats: dict):
    """Institutional flow: Rare, massive block orders that sweep the book."""
    stats.update({'new': 0, 'cancel': 0, 'buy': 0, 'sell': 0})
    writer = None
    base_order_id = (200 + bot_id) * 1000000
    order_seq = 1

    while True:
        try:
            if writer is None:
                reader, writer = await asyncio.open_connection(host, port)
                
            # Sleep for a long time (rare events)
            await asyncio.sleep(random.uniform(30.0, 60.0))
            
            # 70% chance to trade in the direction of the trend (momentum)
            side = random.choice([0, 1])
            qty = random.randint(1000, 5000) # Huge block sizes
            
            # Sweep through the book
            price = state.true_price + 200 if side == 0 else state.true_price - 200
            
            order_id = base_order_id + order_seq
            order_seq += 1
            
            writer.write(pack_order(0, order_id, side, price, qty))
            await writer.drain()
            
            stats['new'] += 1
            if side == 0: stats['buy'] += 1
            else: stats['sell'] += 1
            
            # Induce a volatility shock into the global state due to the large order
            state.volatility = 0.002
            await asyncio.sleep(1.0)
            state.volatility = 0.0005 # Return to normal
            
        except asyncio.CancelledError:
            if writer is not None: writer.close()
            break
        except Exception as e:
            if writer is not None: writer = None
            await asyncio.sleep(1.0)

async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--makers", type=int, default=5)
    parser.add_argument("--retail", type=int, default=3)
    parser.add_argument("--institutions", type=int, default=1)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9001)
    args = parser.parse_args()

    state = SimulationState(10000)
    tasks = []
    
    # 1. Price Simulator
    tasks.append(asyncio.create_task(run_price_simulation(state)))
    
    # 2. Market Makers
    maker_stats = [{} for _ in range(args.makers)]
    for i in range(args.makers):
        tick_interval = 2.0 + (i * 0.5)  # Much slower MMs
        tasks.append(asyncio.create_task(run_market_maker(i+1, args.host, args.port, tick_interval, 8, state, maker_stats[i])))
        
    # 3. Retail Takers
    retail_stats = [{} for _ in range(args.retail)]
    for i in range(args.retail):
        tick_interval = 3.0 + (i * 1.0) # Slower retail flow
        tasks.append(asyncio.create_task(run_retail_taker(i+1, args.host, args.port, tick_interval, state, retail_stats[i])))
        
    # 4. Institutional Takers
    inst_stats = [{} for _ in range(args.institutions)]
    for i in range(args.institutions):
        # Institutions sweep every 30-60 seconds instead of 5-15
        tasks.append(asyncio.create_task(run_institutional_taker(i+1, args.host, args.port, state, inst_stats[i])))

    print(f"Realistic Market Simulation Started.")
    print(f"Spawned {args.makers} MMs, {args.retail} Retail, {args.institutions} Institutions.")
    print("Press Ctrl+C to stop.\n")

    try:
        # Run indefinitely until cancelled by the user
        while True:
            await asyncio.sleep(10)
            print(f"[Simulation] True Price: {state.true_price}")
    except asyncio.CancelledError:
        pass

    for task in tasks:
        task.cancel()
        
    await asyncio.gather(*tasks, return_exceptions=True)
    print("Simulation stopped.")

if __name__ == "__main__":
    loop = asyncio.get_event_loop()
    main_task = asyncio.ensure_future(main())
    try:
        loop.run_until_complete(main_task)
    except KeyboardInterrupt:
        main_task.cancel()
        loop.run_until_complete(main_task)
