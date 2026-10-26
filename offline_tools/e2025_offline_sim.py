#!/usr/bin/env python3
"""Deterministic PC-side smoke test for the E2025 offline baseline."""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from struct import pack, unpack_from


TICK_MS = 10
CORNER_PERIOD = 75


class Task(IntEnum):
    TRACK_N = 1
    AIM_2S = 2
    AIM_4S = 3
    MOVE_1 = 4
    MOVE_2 = 5
    CIRCLE = 6


LIMITS = {
    Task.TRACK_N: 2000,
    Task.AIM_2S: 200,
    Task.AIM_4S: 400,
    Task.MOVE_1: 2000,
    Task.MOVE_2: 4000,
    Task.CIRCLE: 2000,
}


@dataclass
class Result:
    task: Task
    laps: int
    ticks: int
    state: str


def crc8(data: bytes) -> int:
    crc = 0
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def encode_vision_frame(
    sequence: int,
    flags: int,
    target_x: int,
    target_y: int,
    spot_x: int,
    spot_y: int,
    circle_phase: int,
) -> bytes:
    payload = pack(
        "<BBBhhhhB",
        1,
        sequence & 0xFF,
        flags & 0xFF,
        target_x,
        target_y,
        spot_x,
        spot_y,
        circle_phase & 0xFF,
    )
    return bytes((0xAA, 0x55)) + payload + bytes((crc8(payload),))


def decode_vision_frame(frame: bytes) -> tuple[int, ...]:
    if len(frame) != 15 or frame[:2] != b"\xAA\x55":
        raise ValueError("bad frame size or sync")
    if crc8(frame[2:14]) != frame[14]:
        raise ValueError("bad CRC8")
    version, sequence, flags, tx, ty, sx, sy, phase = unpack_from("<BBBhhhhB", frame, 2)
    if version != 1:
        raise ValueError("bad protocol version")
    return sequence, flags, tx, ty, sx, sy, phase


def simulate(
    task: Task,
    requested_laps: int,
    *,
    vision_enabled: bool = True,
    emergency_tick: int | None = None,
) -> Result:
    if task in (Task.MOVE_1, Task.CIRCLE):
        requested_laps = 1
    elif task == Task.MOVE_2:
        requested_laps = 2
    requested_laps = max(1, min(5, requested_laps))

    elapsed = 0
    corners = 0
    laps = 0
    stable = 0
    limit = LIMITS[task]

    while elapsed < limit:
        if emergency_tick is not None and elapsed >= emergency_tick:
            return Result(task, laps, elapsed, "ABORT")

        corner_period = 300 if task == Task.CIRCLE else CORNER_PERIOD
        corner_event = (
            task in (Task.TRACK_N, Task.MOVE_1, Task.MOVE_2, Task.CIRCLE)
            and elapsed > 0
            and elapsed % corner_period == 0
        )
        vision_delay = 100 if task == Task.AIM_4S else 0
        vision_valid = vision_enabled and task != Task.TRACK_N and elapsed >= vision_delay
        error = max(0, 100 - max(0, elapsed - vision_delay) * 2)
        circle_complete = task == Task.CIRCLE and elapsed >= 1200

        elapsed += 1

        if corner_event:
            corners += 1
            if corners == 4:
                corners = 0
                laps += 1

        if vision_valid and error <= 6 and error // 2 <= 6:
            stable += 1
        else:
            stable = 0
        aim_locked = stable >= 3

        if task == Task.TRACK_N and laps >= requested_laps:
            return Result(task, laps, elapsed, "DONE")
        if task in (Task.AIM_2S, Task.AIM_4S) and aim_locked:
            return Result(task, laps, elapsed, "DONE")
        if task in (Task.MOVE_1, Task.MOVE_2) and laps >= requested_laps:
            return Result(task, laps, elapsed, "DONE")
        if task == Task.CIRCLE and laps >= 1 and circle_complete:
            return Result(task, laps, elapsed, "DONE")

    return Result(task, laps, elapsed, "TIMEOUT")


def main() -> None:
    cases = [
        (Task.TRACK_N, 5),
        (Task.AIM_2S, 1),
        (Task.AIM_4S, 1),
        (Task.MOVE_1, 1),
        (Task.MOVE_2, 2),
        (Task.CIRCLE, 1),
    ]
    results = [simulate(task, laps) for task, laps in cases]

    no_vision = simulate(Task.AIM_2S, 1, vision_enabled=False)
    emergency = simulate(Task.TRACK_N, 5, emergency_tick=50)
    assert no_vision.state == "TIMEOUT" and no_vision.ticks == LIMITS[Task.AIM_2S]
    assert emergency.state == "ABORT" and emergency.ticks == 50

    frame = encode_vision_frame(7, 0x03, 160, 120, 155, 124, 32)
    assert decode_vision_frame(frame) == (7, 0x03, 160, 120, 155, 124, 32)
    damaged = frame[:-1] + bytes((frame[-1] ^ 0x01,))
    try:
        decode_vision_frame(damaged)
    except ValueError:
        pass
    else:
        raise AssertionError("CRC test failed")

    print("task      laps   time_s   result")
    print("--------- ------ -------- --------")
    for result in results:
        print(f"{result.task.name:<9} {result.laps:^6} {result.ticks*TICK_MS/1000:>8.2f} {result.state:>8}")

    if any(result.state != "DONE" for result in results):
        raise SystemExit("offline scenario failed")
    print("protocol CRC8: PASS")
    print("no-vision timeout: PASS")
    print("emergency stop: PASS")
    print("all offline scenarios: PASS")


if __name__ == "__main__":
    main()
