# ApexMatch Binary Wire Protocol

This document defines the raw binary protocol used by ApexMatch for ingesting HFT orders over TCP.

All messages must be exactly 22 bytes in length. All multi-byte integer fields (64-bit and 32-bit) must be encoded in **Network Byte Order (Big-Endian)**.

## Message Layout (22 Bytes)

| Offset | Field | Size | Type | Description |
|---|---|---|---|---|
| `0` | `Type` | 1 byte | `uint8_t` | The type of the message. `0` = NEW order, `1` = CANCEL order. |
| `1` | `Order ID` | 8 bytes | `uint64_t` | A unique identifier for the order (Big-Endian). |
| `9` | `Side` | 1 byte | `uint8_t` | The side of the order. `0` = BUY, `1` = SELL. |
| `10` | `Price` | 8 bytes | `uint64_t` | The price in fixed-point integer format (Big-Endian). |
| `18` | `Quantity` | 4 bytes | `uint32_t` | The quantity of the order (Big-Endian). |

## Serialization Notes
- Messages are streamed continuously over the TCP connection. There are no newline delimiters or message separators.
- A client must send all 22 bytes for a message to be processed.
- The matching engine uses the `Order ID` field to identify resting orders for CANCEL operations. For CANCELs, `Side`, `Price`, and `Quantity` fields must still be provided in the 22-byte frame but may be ignored by the engine logic depending on the internal order book implementation.
