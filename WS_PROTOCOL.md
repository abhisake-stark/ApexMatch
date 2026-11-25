# ApexMatch WebSocket Protocol

The WebSocket API broadcasts JSON messages from the matching engine to the frontend clients. Messages fall into two categories: Periodic Order Book Snapshots and Real-Time Trade Events.

## 1. Snapshot Message
Broadcast periodically (e.g., every 100ms) by the broadcaster thread. It contains the top 15 price levels of both the Bid and Ask sides of the Order Book.

```json
{
  "type": "snapshot",
  "timestamp": 1693300000000,
  "bids": [
    {"price": 10000, "qty": 150, "orders": 3},
    {"price": 9999, "qty": 1000, "orders": 1}
  ],
  "asks": [
    {"price": 10002, "qty": 500, "orders": 5},
    {"price": 10005, "qty": 200, "orders": 2}
  ]
}
```

- `type`: Always `"snapshot"`.
- `timestamp`: System timestamp in milliseconds.
- `bids`: Array of top 15 bid levels, ordered descending by price (highest bid first).
- `asks`: Array of top 15 ask levels, ordered ascending by price (lowest ask first).
- `price`: Integer fixed-point price.
- `qty`: Total quantity of all resting orders at this price level.
- `orders`: Number of individual resting orders at this price level.

## 2. Trade Message
Broadcast immediately upon an order fill. If an incoming order walks the book and fills against multiple resting orders, multiple trade messages are generated and broadcast.

```json
{
  "type": "trade",
  "timestamp": 1693300000000,
  "price": 10000,
  "qty": 50,
  "aggressor_side": "BUY"
}
```

- `type`: Always `"trade"`.
- `timestamp`: The transaction timestamp (from the matching engine) in milliseconds.
- `price`: The execution price of the trade (the resting order's price).
- `qty`: The quantity executed in this trade.
- `aggressor_side`: `"BUY"` or `"SELL"`, indicating the side of the incoming order that triggered the fill.
