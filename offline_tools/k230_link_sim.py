#!/usr/bin/env python3
"""Offline byte-stream, packet-loss and timeout replay for k230_link.c."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

from e2025_offline_sim import decode_vision_frame, encode_vision_frame


TIMEOUT_TICKS = 20
NO_FRAME = "NO_FRAME"
FRAME_OK = "FRAME_OK"
FRAME_BAD = "FRAME_BAD"


@dataclass
class LinkModel:
    buffer: bytearray = field(default_factory=bytearray)
    sample: tuple[int, ...] | None = None
    bytes_received: int = 0
    frames_ok: int = 0
    frames_bad: int = 0
    duplicate_frames: int = 0
    missing_frames: int = 0
    sequence_resets: int = 0
    age_ticks: int = 0xFFFF
    have_sequence: bool = False
    last_sequence: int = 0

    def publish(self, decoded: tuple[int, ...]) -> None:
        sequence = decoded[0]
        if self.have_sequence:
            delta = (sequence - self.last_sequence) & 0xFF
            if delta == 0:
                self.duplicate_frames += 1
            elif delta > 128:
                self.sequence_resets += 1
                self.last_sequence = sequence
            else:
                if delta > 1:
                    self.missing_frames += delta - 1
                self.last_sequence = sequence
        else:
            self.have_sequence = True
            self.last_sequence = sequence
        self.sample = decoded
        self.age_ticks = 0
        self.frames_ok += 1

    def push(self, byte: int) -> str:
        byte &= 0xFF
        self.bytes_received += 1
        if not self.buffer:
            if byte == 0xAA:
                self.buffer.append(byte)
            return NO_FRAME
        if len(self.buffer) == 1:
            if byte == 0x55:
                self.buffer.append(byte)
            elif byte == 0xAA:
                self.buffer[0] = byte
            else:
                self.buffer.clear()
            return NO_FRAME

        self.buffer.append(byte)
        if len(self.buffer) < 15:
            return NO_FRAME
        frame = bytes(self.buffer)
        self.buffer.clear()
        try:
            decoded = decode_vision_frame(frame)
        except ValueError:
            self.frames_bad += 1
            return FRAME_BAD
        self.publish(decoded)
        return FRAME_OK

    def push_frame(self, sequence: int, *, corrupt_crc: bool = False) -> str:
        frame = encode_vision_frame(sequence, 0x03, 160, 120, 156, 124, 0)
        if corrupt_crc:
            frame = frame[:-1] + bytes((frame[-1] ^ 0x01,))
        result = NO_FRAME
        for byte in frame:
            result = self.push(byte)
        return result

    def tick(self) -> None:
        if self.sample is not None:
            self.age_ticks = min(0xFFFF, self.age_ticks + 1)

    @property
    def connected(self) -> bool:
        return self.sample is not None and self.age_ticks <= TIMEOUT_TICKS


def source_checks(workspace: Path) -> None:
    link = (workspace / "car_final_ccs/user/k230_link.c").read_text(
        encoding="utf-8"
    )
    serial = (workspace / "car_final_ccs/user/serical.c").read_text(
        encoding="utf-8"
    )
    main = (workspace / "car_final_ccs/user/main.c").read_text(encoding="utf-8")
    config = (workspace / "car_final_ccs/user/app_config.h").read_text(
        encoding="utf-8"
    )
    projectspec = (workspace / "car_final_ccs/car_final_ccs.projectspec").read_text(
        encoding="utf-8"
    )

    assert "#define E2025_ENABLE_K230_UART0_BRIDGE 0" in config
    assert "#if E2025_ENABLE_K230_UART0_BRIDGE" in serial
    assert "k230_link_push_byte(ch)" in serial
    assert "aim_protocol_push" not in serial
    assert "return k230_link_get_snapshot(sample, 0);" in serial
    assert "k230_link_init();" in main
    assert "k230_link_tick_10ms();" in main
    assert "have_vision_sequence = 0U;" in main
    assert "g_k230_publish_guard" in link
    assert "g_k230_missing_frames" in link
    assert "K230_LINK_TIMEOUT_TICKS" in link
    assert "DL_UART" not in link and "GPIO" not in link
    assert "user/k230_link.c" in projectspec


def main() -> None:
    workspace = Path(__file__).resolve().parents[1]
    results: list[dict[str, object]] = []

    fragmented = LinkModel()
    frame = encode_vision_frame(7, 0x03, 160, 120, 155, 124, 32)
    for byte in frame[:-1]:
        assert fragmented.push(byte) == NO_FRAME
        assert not fragmented.connected
    assert fragmented.push(frame[-1]) == FRAME_OK
    assert fragmented.connected and fragmented.sample is not None
    assert fragmented.sample[0] == 7 and fragmented.frames_ok == 1
    results.append({"case": "fragmented_frame_publishes_only_after_crc", "status": "PASS"})

    noisy = LinkModel()
    for byte in b"debug\r\n\x00\xaa\x01noise":
        noisy.push(byte)
    assert noisy.push_frame(8) == FRAME_OK and noisy.connected
    results.append({"case": "noise_then_sync_resynchronizes", "status": "PASS"})

    damaged = LinkModel()
    assert damaged.push_frame(9, corrupt_crc=True) == FRAME_BAD
    assert damaged.frames_bad == 1 and not damaged.connected
    assert damaged.push_frame(10) == FRAME_OK and damaged.connected
    results.append({"case": "bad_crc_counted_without_poisoning_next_frame", "status": "PASS"})

    duplicate = LinkModel()
    duplicate.push_frame(20)
    duplicate.push_frame(20)
    assert duplicate.frames_ok == 2 and duplicate.duplicate_frames == 1
    assert duplicate.missing_frames == 0
    results.append({"case": "duplicate_sequence_counted", "status": "PASS"})

    gap = LinkModel()
    gap.push_frame(10)
    gap.push_frame(13)
    assert gap.missing_frames == 2 and gap.last_sequence == 13
    gap.push_frame(250)
    assert gap.sequence_resets == 1 and gap.last_sequence == 250
    gap.push_frame(255)
    gap.push_frame(0)
    assert gap.last_sequence == 0
    results.append({"case": "gap_restart_and_wrap_accounting", "status": "PASS"})

    timeout = LinkModel()
    timeout.push_frame(30)
    for _ in range(TIMEOUT_TICKS):
        timeout.tick()
    assert timeout.connected and timeout.age_ticks == TIMEOUT_TICKS
    timeout.tick()
    assert not timeout.connected
    timeout.push_frame(31)
    assert timeout.connected and timeout.age_ticks == 0
    results.append({"case": "link_timeout_and_reconnect", "status": "PASS"})

    source_checks(workspace)
    results.append({"case": "uart_independent_firmware_boundary", "status": "PASS"})

    output = workspace / "output/k230_link_sim"
    output.mkdir(parents=True, exist_ok=True)
    summary = {
        "suite": "k230_link",
        "frame_bytes": 15,
        "tick_ms": 10,
        "link_timeout_ms": TIMEOUT_TICKS * 10,
        "uart0_bridge_enabled_in_default_firmware": False,
        "dedicated_uart_pins_selected": False,
        "hardware_verified": False,
        "cases": results,
    }
    (output / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    for result in results:
        print(f"PASS {result['case']}")
    print(f"PASS summary={output / 'summary.json'}")


if __name__ == "__main__":
    main()
