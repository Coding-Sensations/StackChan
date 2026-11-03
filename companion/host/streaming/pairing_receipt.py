"""Non-secret fixed-width USB pairing receipts; no I/O or persistence."""
import struct
from dataclasses import dataclass

WIRE = struct.Struct('>4sBB16s')
READY, STAGED, REJECTED, EXPIRED = 1, 2, 3, 4

@dataclass(frozen=True)
class Receipt:
    status: int
    boot_nonce: int = 0
    reset_reason: int = 0
    request: str = ''

def decode_receipt(frame):
    if type(frame) is not bytes or len(frame) != WIRE.size:
        raise ValueError('invalid_receipt')
    magic, version, status, payload = WIRE.unpack(frame)
    if magic != b'SCPS' or version != 1 or status not in (READY, STAGED, REJECTED, EXPIRED):
        raise ValueError('invalid_receipt')
    if status == READY:
        nonce, reason, reserved = struct.unpack('>II8s', payload)
        if not nonce or reason > 64 or reserved != bytes(8):
            raise ValueError('invalid_receipt')
        return Receipt(status, nonce, reason)
    if status == STAGED:
        return Receipt(status, request=payload.hex())
    if payload != bytes(16):
        raise ValueError('invalid_receipt')
    return Receipt(status)
