import struct

# 1 byte type, 8 bytes order_id, 1 byte side, 8 bytes price, 4 bytes quantity
# Type: 0 = NEW, 1 = CANCEL
# Side: 0 = BUY, 1 = SELL
FORMAT = '>B Q B Q I'
PACKED_SIZE = struct.calcsize(FORMAT)

def pack_order(msg_type: int, order_id: int, side: int, price: int, qty: int) -> bytes:
    """Encodes an order into the 22-byte wire protocol format."""
    return struct.pack(FORMAT, msg_type, order_id, side, price, qty)

def unpack_order(data: bytes) -> tuple:
    """Decodes a 22-byte binary message into (msg_type, order_id, side, price, qty)."""
    return struct.unpack(FORMAT, data)

if __name__ == "__main__":
    assert PACKED_SIZE == 22, f"Expected 22 bytes, got {PACKED_SIZE}"
    
    # Simple round-trip self-test
    msg_type, order_id, side, price, qty = 0, 123456789, 1, 10050, 200
    encoded = pack_order(msg_type, order_id, side, price, qty)
    assert len(encoded) == 22
    
    decoded = unpack_order(encoded)
    assert decoded == (msg_type, order_id, side, price, qty), f"Mismatch: {decoded}"
    
    print("protocol.py self-test passed.")
