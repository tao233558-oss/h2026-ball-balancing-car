#!/usr/bin/env python3
"""Offline model and static audit for the E2025 TOF telemetry adapter."""

from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "car_final_ccs"
OUTPUT = ROOT / "output" / "e2025_tof_adapter_sim"

STALE_TICKS = 50
IO_ERROR = 0xFE
UNAVAILABLE = 0xFF


@dataclass
class Mailbox:
    packed: int = (1 << 24) | (UNAVAILABLE << 16)

    def publish(self, distance_mm: int, status: int) -> None:
        sequence = ((self.packed >> 24) + 1) & 0xFF
        if sequence == 0:
            sequence = 1
        self.packed = (sequence << 24) | (status << 16) | distance_mm


@dataclass
class Reader:
    sequence: int = 0
    present: bool = False
    valid: bool = False
    status: int = UNAVAILABLE
    distance_mm: int = 0
    age_ticks: int = 0xFFFF

    def tick(self, mailbox: Mailbox) -> dict[str, int | bool]:
        packed = mailbox.packed
        sequence = (packed >> 24) & 0xFF
        fresh = False
        if sequence != self.sequence:
            self.sequence = sequence
            self.status = (packed >> 16) & 0xFF
            self.distance_mm = packed & 0xFFFF
            self.age_ticks = 0
            self.present = self.status != UNAVAILABLE
            self.valid = self.status == 0
            fresh = self.status < IO_ERROR
        elif self.age_ticks < 0xFFFF:
            self.age_ticks += 1

        if self.age_ticks > STALE_TICKS:
            self.valid = False

        return {
            "present": self.present,
            "fresh": fresh,
            "valid": self.valid,
            "status": self.status,
            "distance_mm": self.distance_mm,
            "age_ticks": self.age_ticks,
        }


def expect(name: str, condition: bool, failures: list[str]) -> dict[str, str]:
    result = "PASS" if condition else "FAIL"
    if not condition:
        failures.append(name)
    return {"check": name, "result": result}


def main() -> int:
    failures: list[str] = []
    checks: list[dict[str, str]] = []
    mailbox = Mailbox()
    reader = Reader()

    sample = reader.tick(mailbox)
    checks.append(expect("initial sample is unavailable",
                         not sample["present"] and not sample["valid"], failures))

    mailbox.publish(740, 0)
    sample = reader.tick(mailbox)
    checks.append(expect("valid sample is fresh and preserved",
                         sample["present"] and sample["fresh"] and
                         sample["valid"] and sample["distance_mm"] == 740,
                         failures))

    sample = reader.tick(mailbox)
    checks.append(expect("unchanged sample ages without becoming fresh",
                         not sample["fresh"] and sample["valid"] and
                         sample["age_ticks"] == 1, failures))

    mailbox.publish(815, 4)
    sample = reader.tick(mailbox)
    checks.append(expect("VL53L0X range status invalidates distance",
                         sample["present"] and sample["fresh"] and
                         not sample["valid"] and sample["status"] == 4,
                         failures))

    mailbox.publish(500, 0)
    reader.tick(mailbox)
    for _ in range(STALE_TICKS + 1):
        sample = reader.tick(mailbox)
    checks.append(expect("sample becomes invalid after 500 ms timeout",
                         not sample["valid"] and
                         sample["age_ticks"] == STALE_TICKS + 1, failures))

    mailbox.publish(0, IO_ERROR)
    sample = reader.tick(mailbox)
    checks.append(expect("I2C error is present but invalid",
                         sample["present"] and not sample["fresh"] and
                         not sample["valid"] and sample["status"] == IO_ERROR,
                         failures))

    main_source = (PROJECT / "user/main.c").read_text(encoding="utf-8")
    app_source = (PROJECT / "user/e2025_app.c").read_text(encoding="utf-8")
    project_source = (PROJECT / "car_final_ccs.projectspec").read_text(
        encoding="utf-8")
    config_source = (PROJECT / "user/app_config.h").read_text(encoding="utf-8")
    isr_source = main_source.split("void TIMG8_IRQHandler()", 1)[1]
    task_source = app_source.split("switch(g_e2025.task)", 1)[1].split(
        "void e2025_get_status", 1)[0]

    checks.append(expect("ISR reads adapter snapshot",
                         "e2025_tof_snapshot_tick" in isr_source, failures))
    forbidden_isr = (
        "tof200f_init", "tof200f_read_mm", "tof200f_poll_mm",
        "tof200f_start_continuous", "VL53L0X_", "delay_ms", "delay_us",
    )
    checks.append(expect("ISR contains no blocking TOF operation",
                         not any(marker in isr_source for marker in forbidden_isr),
                         failures))
    checks.append(expect("main loop owns nonblocking TOF polling",
                         "e2025_tof_runtime_service();" in main_source and
                         "tof200f_poll_mm" in main_source, failures))
    checks.append(expect("TOF remains telemetry-only in task decisions",
                         "distance_" not in task_source and
                         "tof_" not in task_source, failures))
    checks.append(expect("CCS project includes TOF adapter",
                         "user/e2025_tof_adapter.c" in project_source, failures))
    checks.append(expect("source defaults remain motor-safe with TOF off",
                         "#define E2025_OFFLINE_SIMULATION 1" in config_source and
                         "#define E2025_ENABLE_TOF200F 0" in config_source,
                         failures))
    checks.append(expect("CCS connection remains XDS110",
                         "TIXDS110_Connection.xml" in project_source, failures))

    firmware = (ROOT / "output" / "firmware" /
                "car_final_ccs_e2025_tof_safe.out")
    firmware_sha256 = None
    if firmware.is_file():
        firmware_sha256 = hashlib.sha256(firmware.read_bytes()).hexdigest().upper()
    checks.append(expect("integrated safe firmware exists",
                         firmware_sha256 is not None, failures))

    summary = {
        "result": "PASS" if not failures else "FAIL",
        "hardware_validation": "NOT_RUN",
        "poll_period_ms": 50,
        "stale_timeout_ms": STALE_TICKS * 10,
        "task_behavior_changed": False,
        "integrated_safe_firmware": str(firmware.relative_to(ROOT)),
        "integrated_safe_firmware_sha256": firmware_sha256,
        "checks": checks,
        "failures": failures,
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    report = OUTPUT / "summary.json"
    report.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
                      encoding="utf-8")
    print(f"E2025 TOF adapter simulation: {summary['result']}")
    print("Hardware validation: NOT_RUN")
    print(f"Report: {report}")
    if failures:
        for failure in failures:
            print(f"ERROR: {failure}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
