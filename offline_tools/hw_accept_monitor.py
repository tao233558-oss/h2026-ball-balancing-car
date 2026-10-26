#!/usr/bin/env python3
"""Monitor Tianmengxing HWACC serial lines and save auditable reports."""

from __future__ import annotations

import argparse
import csv
import json
import re
import sys
import time
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterable, TextIO


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT_DIR = ROOT / "output" / "hardware_acceptance_reports"
REQUIRED_FIELDS = (
    "KEY_RAW",
    "KEY_PASS",
    "TRACK_BLACK",
    "TRACK_PASS",
    "ENC_L",
    "ENC_R",
    "MOTOR",
    "STBY",
    "K230",
    "RESULT",
)


@dataclass(frozen=True)
class HwAcceptSample:
    timestamp: str
    key_raw: int
    key_pass: int
    track_black: int
    track_pass: int
    enc_l: int
    enc_r: int
    motor: int
    stby: int
    k230: str
    result: str
    raw_line: str


def _parse_number(value: str, field: str) -> int:
    try:
        return int(value, 0)
    except ValueError as exc:
        raise ValueError(f"字段 {field} 不是有效数字: {value}") from exc


def parse_hwacc_line(line: str, timestamp: str | None = None) -> HwAcceptSample | None:
    """Parse one line; return None for unrelated serial text."""
    match = re.search(r"(?:^|\s)HWACC\s+(.+)$", line.strip())
    if match is None:
        return None

    fields: dict[str, str] = {}
    for token in match.group(1).split():
        if "=" in token:
            name, value = token.split("=", 1)
            fields[name] = value

    missing = [name for name in REQUIRED_FIELDS if name not in fields]
    if missing:
        raise ValueError("HWACC 缺少字段: " + ", ".join(missing))

    return HwAcceptSample(
        timestamp=timestamp or datetime.now().astimezone().isoformat(timespec="milliseconds"),
        key_raw=_parse_number(fields["KEY_RAW"], "KEY_RAW") & 0x0F,
        key_pass=_parse_number(fields["KEY_PASS"], "KEY_PASS") & 0x0F,
        track_black=_parse_number(fields["TRACK_BLACK"], "TRACK_BLACK") & 0xFF,
        track_pass=_parse_number(fields["TRACK_PASS"], "TRACK_PASS") & 0xFF,
        enc_l=_parse_number(fields["ENC_L"], "ENC_L"),
        enc_r=_parse_number(fields["ENC_R"], "ENC_R"),
        motor=_parse_number(fields["MOTOR"], "MOTOR"),
        stby=_parse_number(fields["STBY"], "STBY"),
        k230=fields["K230"],
        result=fields["RESULT"],
        raw_line=line.rstrip("\r\n"),
    )


def count_bits(value: int) -> int:
    return bin(value).count("1")


def assess_sample(sample: HwAcceptSample) -> dict[str, object]:
    keys = {f"SW{index + 1}": bool(sample.key_pass & (1 << index)) for index in range(4)}
    track = {f"D{index + 1}": bool(sample.track_pass & (1 << index)) for index in range(8)}
    left_encoder_pass = sample.enc_l >= 4
    right_encoder_pass = sample.enc_r >= 4
    outputs_safe = sample.motor == 0 and sample.stby == 0
    inputs_pass = all(keys.values()) and all(track.values()) and left_encoder_pass and right_encoder_pass
    k230_blocked = sample.k230.startswith("BLOCKED")

    if not outputs_safe:
        verdict = "UNSAFE_OUTPUT"
    elif not inputs_pass:
        verdict = "INCOMPLETE"
    elif k230_blocked:
        verdict = "INPUT_PASS_K230_BLOCKED"
    elif sample.k230 == "PASS":
        verdict = "PASS"
    else:
        verdict = "INPUT_PASS_K230_UNKNOWN"

    return {
        "keys": keys,
        "key_pass_count": sum(keys.values()),
        "track": track,
        "track_pass_count": sum(track.values()),
        "left_encoder_pass": left_encoder_pass,
        "right_encoder_pass": right_encoder_pass,
        "outputs_safe": outputs_safe,
        "inputs_pass": inputs_pass,
        "k230_blocked": k230_blocked,
        "verdict": verdict,
    }


def format_status(sample: HwAcceptSample) -> str:
    status = assess_sample(sample)
    left = "PASS" if status["left_encoder_pass"] else "WAIT"
    right = "PASS" if status["right_encoder_pass"] else "WAIT"
    safety = "PASS" if status["outputs_safe"] else "DANGER"
    k230 = "BLOCKED" if status["k230_blocked"] else sample.k230
    return (
        f"按键 {status['key_pass_count']}/4 | "
        f"循迹 {status['track_pass_count']}/8 | "
        f"编码器 L={sample.enc_l} {left}, R={sample.enc_r} {right} | "
        f"输出安全 {safety} | K230 {k230} | {status['verdict']}"
    )


class MonitorSession:
    def __init__(self, source: str) -> None:
        self.source = source
        self.started_at = datetime.now().astimezone()
        self.ended_at: datetime | None = None
        self.samples: list[HwAcceptSample] = []
        self.raw_lines: list[str] = []
        self.parse_errors: list[str] = []
        self.ignored_lines = 0

    def add_line(self, line: str, *, echo: bool = True) -> HwAcceptSample | None:
        self.raw_lines.append(line.rstrip("\r\n"))
        try:
            sample = parse_hwacc_line(line)
        except ValueError as exc:
            message = f"{datetime.now():%H:%M:%S} {exc}"
            self.parse_errors.append(message)
            if echo:
                print("解析警告:", message, file=sys.stderr)
            return None

        if sample is None:
            self.ignored_lines += 1
            return None
        self.samples.append(sample)
        if echo:
            print(f"[{datetime.now():%H:%M:%S}] {format_status(sample)}")
        return sample

    def finish(self) -> None:
        if self.ended_at is None:
            self.ended_at = datetime.now().astimezone()

    def summary(self) -> dict[str, object]:
        self.finish()
        final = self.samples[-1] if self.samples else None
        final_status = assess_sample(final) if final else None
        return {
            "schema_version": 1,
            "source": self.source,
            "started_at": self.started_at.isoformat(timespec="seconds"),
            "ended_at": self.ended_at.isoformat(timespec="seconds") if self.ended_at else None,
            "sample_count": len(self.samples),
            "ignored_line_count": self.ignored_lines,
            "parse_errors": self.parse_errors,
            "final_sample": asdict(final) if final else None,
            "final_status": final_status,
            "scope_note": (
                "PASS only covers keys, track inputs, encoder activity and disabled outputs; "
                "K230, motors, pan/tilt and laser still require later hardware tests."
            ),
        }

    def write_reports(self, output_dir: Path, prefix: str = "hw_accept") -> dict[str, Path]:
        self.finish()
        output_dir.mkdir(parents=True, exist_ok=True)
        stamp = self.started_at.strftime("%Y%m%d_%H%M%S")
        base = output_dir / f"{prefix}_{stamp}"
        paths = {
            "csv": base.with_suffix(".csv"),
            "json": base.with_suffix(".json"),
            "log": base.with_suffix(".log"),
        }

        with paths["csv"].open("w", encoding="utf-8-sig", newline="") as stream:
            fieldnames = [
                "timestamp", "key_raw", "key_pass", "key_pass_count",
                "track_black", "track_pass", "track_pass_count",
                "enc_l", "enc_r", "left_encoder_pass", "right_encoder_pass",
                "motor", "stby", "outputs_safe", "k230", "result", "verdict",
            ]
            writer = csv.DictWriter(stream, fieldnames=fieldnames)
            writer.writeheader()
            for sample in self.samples:
                status = assess_sample(sample)
                writer.writerow({
                    "timestamp": sample.timestamp,
                    "key_raw": f"0x{sample.key_raw:02X}",
                    "key_pass": f"0x{sample.key_pass:02X}",
                    "key_pass_count": status["key_pass_count"],
                    "track_black": f"0x{sample.track_black:02X}",
                    "track_pass": f"0x{sample.track_pass:02X}",
                    "track_pass_count": status["track_pass_count"],
                    "enc_l": sample.enc_l,
                    "enc_r": sample.enc_r,
                    "left_encoder_pass": status["left_encoder_pass"],
                    "right_encoder_pass": status["right_encoder_pass"],
                    "motor": sample.motor,
                    "stby": sample.stby,
                    "outputs_safe": status["outputs_safe"],
                    "k230": sample.k230,
                    "result": sample.result,
                    "verdict": status["verdict"],
                })

        paths["json"].write_text(
            json.dumps(self.summary(), ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        paths["log"].write_text("\n".join(self.raw_lines) + "\n", encoding="utf-8")
        return paths


def process_lines(lines: Iterable[str], session: MonitorSession, *, until_pass: bool) -> None:
    for line in lines:
        sample = session.add_line(line)
        if sample is not None and until_pass and assess_sample(sample)["inputs_pass"]:
            break


def import_serial():
    try:
        import serial
        import serial.tools.list_ports
    except ImportError as exc:
        raise RuntimeError(
            "实时COM口功能需要 pyserial。请运行: python -m pip install pyserial"
        ) from exc
    return serial


def list_ports() -> int:
    try:
        serial = import_serial()
    except RuntimeError as exc:
        print(exc, file=sys.stderr)
        return 2
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("没有发现串口。请检查Type-C数据线和CH340驱动。")
        return 1
    for port in ports:
        print(f"{port.device}\t{port.description}\t{port.hwid}")
    return 0


def run_serial(args: argparse.Namespace, session: MonitorSession) -> int:
    try:
        serial = import_serial()
    except RuntimeError as exc:
        print(exc, file=sys.stderr)
        return 2

    start = time.monotonic()
    try:
        with serial.Serial(args.port, args.baudrate, timeout=0.5) as stream:
            print(f"已打开 {args.port}，{args.baudrate} 8N1；按 Ctrl+C 可停止并保存报告。")
            while True:
                if args.duration is not None and time.monotonic() - start >= args.duration:
                    break
                data = stream.readline()
                if not data:
                    continue
                line = data.decode("utf-8", errors="replace")
                sample = session.add_line(line)
                if sample is not None and args.until_pass and assess_sample(sample)["inputs_pass"]:
                    print("当前可验证输入已经通过，自动停止采集。")
                    break
    except serial.SerialException as exc:
        print(
            f"串口打开/读取失败: {exc}\n"
            "请关闭友善串口助手等占用该COM口的软件，并确认端口号正确。",
            file=sys.stderr,
        )
        return 2
    except KeyboardInterrupt:
        print("\n用户停止采集，正在保存报告。")
    return 0


def save_and_print(session: MonitorSession, args: argparse.Namespace) -> int:
    paths = session.write_reports(Path(args.output_dir), args.report_prefix)
    summary = session.summary()
    final_status = summary["final_status"]
    print("报告已保存：")
    for kind, path in paths.items():
        print(f"  {kind.upper()}: {path}")
    if final_status is None:
        print("结论：没有收到有效HWACC数据。", file=sys.stderr)
        return 1
    print(f"结论：{final_status['verdict']}")
    return 0 if final_status["inputs_pass"] and final_status["outputs_safe"] else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="天猛星小车HWACC串口验收与报告工具")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--list", action="store_true", help="列出串口")
    mode.add_argument("--port", help="实时读取串口，例如 COM3")
    mode.add_argument("--replay", help="回放已经保存的串口文本日志")
    parser.add_argument("--baudrate", type=int, default=115200, help="默认115200")
    parser.add_argument("--duration", type=float, help="实时采集秒数；不填则运行到Ctrl+C")
    parser.add_argument("--until-pass", action="store_true", help="输入通过后自动停止")
    parser.add_argument("--output-dir", default=str(DEFAULT_OUTPUT_DIR), help="报告目录")
    parser.add_argument("--report-prefix", default="hw_accept", help="报告文件名前缀")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.list:
        return list_ports()

    source = f"serial:{args.port}" if args.port else f"replay:{Path(args.replay).resolve()}"
    session = MonitorSession(source)
    result = 0
    if args.replay:
        replay_path = Path(args.replay)
        if not replay_path.is_file():
            print(f"找不到回放文件: {replay_path}", file=sys.stderr)
            return 2
        with replay_path.open("r", encoding="utf-8", errors="replace") as stream:
            process_lines(stream, session, until_pass=args.until_pass)
    else:
        result = run_serial(args, session)

    report_result = save_and_print(session, args)
    return result if result != 0 else report_result


if __name__ == "__main__":
    raise SystemExit(main())
