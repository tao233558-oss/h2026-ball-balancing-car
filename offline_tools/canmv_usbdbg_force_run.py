"""Run a temporary script through legacy CanMV USBDBG without writing SD."""

from __future__ import annotations

import argparse
import struct
import time
from pathlib import Path

import serial


CMD_PREFIX = 0x30
CMD_SCRIPT_EXEC = 0x05
CMD_SCRIPT_STOP = 0x06
CMD_SCRIPT_RUNNING = 0x87
CMD_TX_BUF_LEN = 0x8E
CMD_TX_BUF = 0x8F


def header(opcode: int, response_field: int = 0) -> bytes:
    return bytes((CMD_PREFIX, opcode)) + struct.pack("<I", response_field)


def read_exact(port: serial.Serial, size: int) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + 2.0
    while len(data) < size and time.monotonic() < deadline:
        data.extend(port.read(size - len(data)))
    if len(data) != size:
        raise TimeoutError(f"short USBDBG read: {len(data)} of {size}")
    return bytes(data)


def script_running(port: serial.Serial) -> bool:
    port.write(header(CMD_SCRIPT_RUNNING))
    port.flush()
    return struct.unpack("<I", read_exact(port, 4))[0] != 0


def drain_script_output(port: serial.Serial) -> bytes:
    port.write(header(CMD_TX_BUF_LEN))
    port.flush()
    length = struct.unpack("<I", read_exact(port, 4))[0]
    if length == 0:
        return b""
    port.write(header(CMD_TX_BUF, length))
    port.flush()
    return read_exact(port, length)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("script", type=Path)
    parser.add_argument("--port", default="COM8")
    parser.add_argument("--baud", type=int, default=12_000_000)
    parser.add_argument("--monitor-seconds", type=float, default=30.0)
    args = parser.parse_args()

    source = args.script.read_bytes() + b"\n"
    if len(source) % 64 == 0:
        source += b"\n"

    port = serial.Serial(
        args.port,
        args.baud,
        timeout=0.2,
        write_timeout=3.0,
        rtscts=False,
        dsrdtr=False,
    )
    try:
        port.rts = True
        port.dtr = False
        time.sleep(0.05)
        port.dtr = True
        time.sleep(0.25)
        port.reset_input_buffer()

        for attempt in range(3):
            port.write(header(CMD_SCRIPT_STOP))
            port.flush()
            time.sleep(0.5)
            try:
                print(f"stop[{attempt + 1}] running={script_running(port)}")
            except TimeoutError as exc:
                print(f"stop[{attempt + 1}] status={exc}")
                port.reset_input_buffer()

        port.write(header(CMD_SCRIPT_EXEC, len(source)))
        port.flush()
        time.sleep(0.5)
        port.write(source)
        port.flush()
        time.sleep(0.5)
        print("script sent, bytes=", len(source))

        deadline = time.monotonic() + args.monitor_seconds
        while time.monotonic() < deadline:
            try:
                output = drain_script_output(port)
                if output:
                    print(output.decode("utf-8", errors="replace"), end="", flush=True)
            except TimeoutError as exc:
                print("\nmonitor:", exc)
                port.reset_input_buffer()
            time.sleep(0.1)
    finally:
        port.close()


if __name__ == "__main__":
    main()
