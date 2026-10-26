"""Minimal client for the official CanMV VS Code native backend."""

from __future__ import annotations

import argparse
import json
import queue
import struct
import subprocess
import threading
import time
from pathlib import Path


MAGIC = b"CM"
MSG_REQUEST = 0x01
MSG_RESPONSE = 0x02
MSG_EVENT = 0x03


class BackendClient:
    def __init__(self, executable: Path):
        creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        self.process = subprocess.Popen(
            [str(executable)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=None,
            bufsize=0,
            creationflags=creationflags,
        )
        self.messages: queue.Queue[tuple[int, dict]] = queue.Queue()
        self.next_id = 1
        self.board_ready = False
        self.reader = threading.Thread(target=self._reader_loop, daemon=True)
        self.reader.start()

    @staticmethod
    def _read_exact(stream, size: int) -> bytes:
        chunks = bytearray()
        while len(chunks) < size:
            data = stream.read(size - len(chunks))
            if not data:
                raise EOFError("CanMV backend closed its output")
            chunks.extend(data)
        return bytes(chunks)

    def _reader_loop(self) -> None:
        try:
            while True:
                header = self._read_exact(self.process.stdout, 7)
                if header[:2] != MAGIC:
                    raise RuntimeError("bad CanMV backend frame magic")
                message_type = header[2]
                payload_size = struct.unpack("<I", header[3:])[0]
                payload = self._read_exact(self.process.stdout, payload_size)
                self.messages.put((message_type, json.loads(payload)))
        except Exception as exc:
            self.messages.put((-1, {"error": str(exc)}))

    def _write_request(self, request_id: int, method: str, params: dict) -> None:
        payload = json.dumps(
            {"id": request_id, "method": method, "params": params},
            separators=(",", ":"),
        ).encode("utf-8")
        frame = MAGIC + bytes((MSG_REQUEST,)) + struct.pack("<I", len(payload)) + payload
        self.process.stdin.write(frame)
        self.process.stdin.flush()

    def _show_event(self, message: dict) -> None:
        event = message.get("event", "")
        params = message.get("params", {})
        if event == "boardReady":
            self.board_ready = True
        if event == "scriptOutput":
            print(params.get("text", ""), end="", flush=True)
        else:
            print("EVENT", event, json.dumps(params, ensure_ascii=False), flush=True)

    def request(self, method: str, params: dict | None = None, timeout: float = 20.0):
        request_id = self.next_id
        self.next_id += 1
        self._write_request(request_id, method, params or {})
        deadline = time.monotonic() + timeout

        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"timeout waiting for {method}")
            message_type, message = self.messages.get(timeout=remaining)
            if message_type == MSG_EVENT:
                self._show_event(message)
                continue
            if message_type == -1:
                raise RuntimeError(message["error"])
            if message_type != MSG_RESPONSE or message.get("id") != request_id:
                continue
            if message.get("error"):
                raise RuntimeError(
                    f"{method}: {message['error']['code']} "
                    f"{message['error']['message']}"
                )
            return message.get("result")

    def wait_for_ready(self, timeout: float = 15.0) -> None:
        if self.board_ready:
            return
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("timeout waiting for CanMV boardReady")
            message_type, message = self.messages.get(timeout=remaining)
            if message_type == -1:
                raise RuntimeError(message["error"])
            if message_type != MSG_EVENT:
                continue
            self._show_event(message)
            if message.get("event") == "boardReady":
                return

    def monitor(self, seconds: float) -> None:
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            try:
                message_type, message = self.messages.get(timeout=1.0)
            except queue.Empty:
                continue
            if message_type == MSG_EVENT:
                self._show_event(message)
            elif message_type == -1:
                raise RuntimeError(message["error"])

    def close(self) -> None:
        try:
            self.request("disconnectBoard", timeout=3.0)
        except Exception:
            pass
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                self.process.kill()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("backend", type=Path)
    parser.add_argument("script", type=Path)
    parser.add_argument("--port", default="COM8")
    parser.add_argument("--baud", type=int, default=12_000_000)
    parser.add_argument("--monitor-seconds", type=float, default=300.0)
    parser.add_argument("--detect-only", action="store_true")
    args = parser.parse_args()

    source = args.script.read_text(encoding="utf-8")
    client = BackendClient(args.backend.resolve())
    try:
        print("DETECT", client.request("detectBoards"), flush=True)
        if args.detect_only:
            return
        board = client.request(
            "connectBoard",
            {"port": args.port, "baudRate": args.baud},
            timeout=30.0,
        )
        print("CONNECTED", json.dumps(board, ensure_ascii=False), flush=True)
        client.wait_for_ready()
        running = client.request("scriptRunning")
        print("SCRIPT", running, flush=True)
        if running.get("running"):
            print("STOP", client.request("stopScript", timeout=15.0), flush=True)
            stop_deadline = time.monotonic() + 10.0
            while time.monotonic() < stop_deadline:
                time.sleep(0.5)
                running = client.request("scriptRunning")
                print("SCRIPT", running, flush=True)
                if not running.get("running"):
                    break
        print("RUN", client.request("runScript", {"script": source}), flush=True)
        client.monitor(args.monitor_seconds)
    finally:
        client.close()


if __name__ == "__main__":
    main()
