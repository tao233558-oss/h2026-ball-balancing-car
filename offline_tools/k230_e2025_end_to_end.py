#!/usr/bin/env python3
"""K230 encoder -> byte stream -> MSPM0 link-model end-to-end replay."""

from __future__ import annotations

import importlib.util
import hashlib
import json
import sys
from pathlib import Path

from e2025_offline_sim import encode_vision_frame
from k230_link_sim import FRAME_BAD, FRAME_OK, LinkModel


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from vision_e2025.protocol import VisionPacket, encode as pc_encode


OUTPUT = ROOT / "output" / "k230_e2025_end_to_end"

TARGET_VALID = 0x01
SPOT_VALID = 0x02
CIRCLE_COMPLETE = 0x04


def load_k230_protocol():
    path = ROOT / "k230_e2025" / "aim_protocol.py"
    spec = importlib.util.spec_from_file_location("k230_aim_protocol", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def feed(link: LinkModel, frames: list[bytes]) -> None:
    for frame in frames:
        result = None
        for byte in frame:
            result = link.push(byte)
        if result != FRAME_OK:
            raise AssertionError(f"valid frame rejected: {result}")


def write_stream(name: str, frames: list[bytes]) -> dict[str, object]:
    stream = b"".join(frames)
    path = OUTPUT / f"{name}.bin"
    path.write_bytes(stream)
    return {"name": name, "frames": len(frames), "bytes": len(stream),
            "file": str(path.relative_to(ROOT))}


def main() -> int:
    protocol = load_k230_protocol()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    checks: list[dict[str, str]] = []
    failures: list[str] = []
    streams: list[dict[str, object]] = []

    def check(name: str, condition: bool) -> None:
        checks.append({"check": name, "result": "PASS" if condition else "FAIL"})
        if not condition:
            failures.append(name)

    # All three independent encoders must produce the exact firmware layout.
    frame_k230 = protocol.make_packet(7, 0x03, 160, 120, 155, 124, 32)
    frame_pc = pc_encode(VisionPacket(7, 0x03, 160, 120, 155, 124, 32))
    frame_old_model = encode_vision_frame(7, 0x03, 160, 120, 155, 124, 32)
    check("K230, PC and MSPM0 reference encoders are byte-identical",
          frame_k230 == frame_pc == frame_old_model and len(frame_k230) == 15)

    # E2: spot error converges to the target over 40 frames at 20 FPS.
    center_frames = []
    for sequence in range(40):
        error = max(0, 80 - sequence * 4)
        center_frames.append(protocol.make_packet(
            sequence, TARGET_VALID | SPOT_VALID,
            320, 240, 320 - error, 240 + error // 2, 0))
    center_link = LinkModel()
    feed(center_link, center_frames)
    check("E2 center stream reaches an aligned valid sample",
          center_link.connected and center_link.frames_ok == 40 and
          center_link.sample is not None and center_link.sample[2:6] ==
          (320, 240, 320, 240))
    streams.append(write_stream("e2_center", center_frames))

    # E3: camera searches for one second, then publishes valid coordinates.
    search_frames = []
    for sequence in range(40):
        flags = 0 if sequence < 20 else TARGET_VALID | SPOT_VALID
        search_frames.append(protocol.make_packet(
            sequence, flags, 320 if flags else 0, 240 if flags else 0,
            318 if flags else 0, 242 if flags else 0, 0))
    search_link = LinkModel()
    feed(search_link, search_frames)
    check("E3 search stream transitions from invalid to valid target",
          search_link.frames_ok == 40 and search_link.sample is not None and
          search_link.sample[1] == (TARGET_VALID | SPOT_VALID))
    streams.append(write_stream("e3_search_then_lock", search_frames))

    # E4/E5 use the same CENTER vision mode while the vehicle moves.
    moving_frames = [protocol.make_packet(
        sequence, TARGET_VALID | SPOT_VALID, 320, 240,
        320 + ((sequence % 9) - 4), 240, 0) for sequence in range(120)]
    moving_link = LinkModel()
    feed(moving_link, moving_frames)
    check("E4/E5 moving-center stream remains connected across sequence flow",
          moving_link.connected and moving_link.frames_ok == 120 and
          moving_link.frames_bad == 0)
    streams.append(write_stream("e4_e5_moving_center", moving_frames))

    # E6: a repeated CIRCLE heartbeat must not restart phase. One full phase
    # wrap produces exactly one completion flag in this replay.
    circle_frames = []
    phase = 0
    complete_count = 0
    for sequence in range(130):
        next_phase = (phase + 2) & 0xFF
        complete = next_phase < phase
        flags = TARGET_VALID | SPOT_VALID
        if complete:
            flags |= CIRCLE_COMPLETE
            complete_count += 1
        phase = next_phase
        circle_frames.append(protocol.make_packet(
            sequence, flags, 320, 240, 318, 241, phase))
    circle_link = LinkModel()
    feed(circle_link, circle_frames)
    check("E6 circle stream completes one uninterrupted phase revolution",
          circle_link.frames_ok == 130 and complete_count == 1 and
          circle_link.missing_frames == 0)
    streams.append(write_stream("e6_circle", circle_frames))

    # Fault injection: corrupt CRC, drop two sequence numbers, timeout, recover.
    fault_link = LinkModel()
    good = protocol.make_packet(10, 0x03, 320, 240, 300, 250, 0)
    damaged = good[:-1] + bytes((good[-1] ^ 0x01,))
    result = None
    for byte in damaged:
        result = fault_link.push(byte)
    check("corrupt CRC is rejected", result == FRAME_BAD and
          fault_link.frames_bad == 1 and not fault_link.connected)
    feed(fault_link, [protocol.make_packet(11, 0x03, 320, 240, 310, 245, 0),
                      protocol.make_packet(14, 0x03, 320, 240, 315, 242, 0)])
    check("sequence gap reports two missing frames",
          fault_link.missing_frames == 2)
    for _ in range(21):
        fault_link.tick()
    check("200 ms link timeout disconnects after the boundary",
          not fault_link.connected and fault_link.age_ticks == 21)
    feed(fault_link, [protocol.make_packet(15, 0x03, 320, 240, 320, 240, 0)])
    check("valid frame reconnects after timeout", fault_link.connected)

    main_c = (ROOT / "car_final_ccs/user/main.c").read_text(encoding="utf-8")
    command_c = (ROOT / "car_final_ccs/user/e2025_k230_command.c").read_text(
        encoding="utf-8")
    k230_main = (ROOT / "k230_e2025/main.py").read_text(encoding="utf-8")
    config = (ROOT / "car_final_ccs/user/app_config.h").read_text(encoding="utf-8")
    projectspec = (ROOT / "car_final_ccs/car_final_ccs.projectspec").read_text(
        encoding="utf-8")
    isr = main_c.split("void TIMG8_IRQHandler()", 1)[1]
    check("MSPM0 maps E2-E5 to CENTER and E6 to CIRCLE",
          'return "CENTER\\n"' in command_c and
          'return "CIRCLE\\n"' in command_c and
          "E2025_TASK_MOVE_DRAW_CIRCLE" in command_c)
    check("UART2 command transmission stays out of 10 ms ISR",
          "e2025_k230_command_service" not in isr and
          "uart_sendbyte(UART2" not in isr)
    check("K230 FPIOA uses GPIO5/GPIO6 for header pins 11/13",
          "UART_TX_GPIO = 5" in k230_main and
          "UART_RX_GPIO = 6" in k230_main and
          "UART_TX_PIN = 11" not in k230_main)
    check("CIRCLE heartbeat does not reset an active circle",
          "if run_mode != MODE_CIRCLE:" in k230_main)
    check("source defaults remain motor-safe with UART2 off",
          "#define E2025_OFFLINE_SIMULATION 1" in config and
          "#define E2025_ENABLE_K230_UART2 0" in config)
    check("CCS connection remains XDS110",
          "TIXDS110_Connection.xml" in projectspec)

    firmware = (ROOT / "output/firmware/car_final_ccs_e2025_k230_safe.out")
    firmware_sha256 = None
    if firmware.is_file():
        firmware_sha256 = hashlib.sha256(firmware.read_bytes()).hexdigest().upper()
    check("UART2 integrated safe firmware exists", firmware_sha256 is not None)

    summary = {
        "result": "PASS" if not failures else "FAIL",
        "hardware_validation": "NOT_RUN",
        "protocol_frame_bytes": 15,
        "uart": "UART2 115200 8N1",
        "mspm0_pins": {"tx": "PB15", "rx": "PA22"},
        "k230_header": {"tx": "pin 11 / GPIO5", "rx": "pin 13 / GPIO6"},
        "commands": {"idle_track_terminal": "STOP", "aim_move": "CENTER",
                     "draw_circle": "CIRCLE", "heartbeat_ms": 1000},
        "integrated_safe_firmware": str(firmware.relative_to(ROOT)),
        "integrated_safe_firmware_sha256": firmware_sha256,
        "streams": streams,
        "checks": checks,
        "failures": failures,
    }
    report = OUTPUT / "summary.json"
    report.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
                      encoding="utf-8")
    print(f"K230 E2025 end-to-end replay: {summary['result']}")
    print("Hardware validation: NOT_RUN")
    print(f"Report: {report}")
    if failures:
        for failure in failures:
            print(f"ERROR: {failure}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
