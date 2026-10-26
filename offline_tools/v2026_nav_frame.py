"""Build and inspect V2026 external-vision navigation frames."""

from __future__ import annotations

import argparse
import struct


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def build_frame(sequence: int, flags: int, linear_mm_s: int,
                angular_mdeg_s: int, heading_cdeg: int,
                timeout_ms: int) -> bytes:
    if not 50 <= timeout_ms <= 1000:
        raise ValueError("timeout_ms must be in 50..1000")
    payload = struct.pack(
        "<BBBBBBhhhH",
        0xA5,
        0x5A,
        0x01,
        0x01,
        sequence & 0xFF,
        flags & 0xFF,
        linear_mm_s,
        angular_mdeg_s,
        heading_cdeg,
        timeout_ms,
    )
    return payload + struct.pack("<H", crc16_ccitt(payload[2:]))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seq", type=int, default=0)
    parser.add_argument("--flags", type=lambda value: int(value, 0), default=1)
    parser.add_argument("--linear", type=int, default=200)
    parser.add_argument("--angular", type=int, default=0)
    parser.add_argument("--heading", type=int, default=0)
    parser.add_argument("--timeout", type=int, default=200)
    args = parser.parse_args()
    frame = build_frame(args.seq, args.flags, args.linear, args.angular,
                        args.heading, args.timeout)
    print(frame.hex(" ").upper())


if __name__ == "__main__":
    main()
