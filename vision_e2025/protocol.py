"""K230 -> MSPM0 15-byte aiming protocol.

The layout exactly matches car_final_ccs/user/aim_protocol.[ch].
"""

from __future__ import annotations

from dataclasses import dataclass
from struct import pack, unpack_from


SYNC = b"\xAA\x55"
VERSION = 1
FRAME_SIZE = 15

FLAG_TARGET_VALID = 0x01
FLAG_SPOT_VALID = 0x02
FLAG_CIRCLE_COMPLETE = 0x04


@dataclass(frozen=True)
class VisionPacket:
    sequence: int
    flags: int
    target_x: int
    target_y: int
    spot_x: int
    spot_y: int
    circle_phase: int = 0

    @property
    def target_valid(self) -> bool:
        return bool(self.flags & FLAG_TARGET_VALID)

    @property
    def spot_valid(self) -> bool:
        return bool(self.flags & FLAG_SPOT_VALID)


def crc8(data: bytes) -> int:
    """CRC-8, polynomial 0x07, initial value 0x00."""
    crc = 0
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def encode(packet: VisionPacket) -> bytes:
    payload = pack(
        "<BBBhhhhB",
        VERSION,
        packet.sequence & 0xFF,
        packet.flags & 0xFF,
        packet.target_x,
        packet.target_y,
        packet.spot_x,
        packet.spot_y,
        packet.circle_phase & 0xFF,
    )
    return SYNC + payload + bytes((crc8(payload),))


def decode(frame: bytes) -> VisionPacket:
    if len(frame) != FRAME_SIZE:
        raise ValueError(f"expected {FRAME_SIZE} bytes, got {len(frame)}")
    if frame[:2] != SYNC:
        raise ValueError("bad sync")
    if crc8(frame[2:14]) != frame[14]:
        raise ValueError("bad CRC8")

    version, sequence, flags, tx, ty, sx, sy, phase = unpack_from(
        "<BBBhhhhB", frame, 2
    )
    if version != VERSION:
        raise ValueError(f"unsupported version {version}")
    return VisionPacket(sequence, flags, tx, ty, sx, sy, phase)


def hex_bytes(frame: bytes) -> str:
    return " ".join(f"{value:02X}" for value in frame)

