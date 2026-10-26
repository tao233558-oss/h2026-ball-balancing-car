#!/usr/bin/env python3
"""Static audit for the TOF200F/VL53L0X PB0/PB1 integration."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "car_final_ccs"
OUTPUT = ROOT / "output" / "tof200f_i2c_audit"


REQUIRED = {
    "ml_libs/ml_i2c.h": [
        "#define I2C_SCL              PB0",
        "#define I2C_SDA              PB1",
        "I2C_WriteBuffer",
        "I2C_ReadBuffer",
    ],
    "ml_libs/ml_i2c.c": [
        "DL_GPIO_disableOutput(I2C_GPIO, pin)",
        "I2C_CLOCK_HIGH_TIMEOUT_US",
        "I2C_WaitAck",
    ],
    "user/tof200f.h": [
        "TOF200F_I2C_ADDRESS_8BIT 0x52U",
        "TOF200F_EXPECTED_MODEL_ID 0xEEAAU",
        "tof200f_start_continuous",
        "tof200f_poll_mm",
    ],
    "user/tof200f.c": [
        "VL53L0X_DataInit",
        "VL53L0X_PerformRefCalibration",
        "VL53L0X_PerformRefSpadManagement",
        "VL53L0X_PerformSingleRangingMeasurement",
        "VL53L0X_GetMeasurementDataReady",
        "VL53L0X_ClearInterruptMask",
    ],
    "user/main.c": [
        "Wiring: PB0=SCL, PB1=SDA",
        "01 06 00 09 00 01 98 08",
        "tof200f_read_mm",
        "e2025_tof_runtime_service",
        "e2025_tof_snapshot_tick",
    ],
    "car_final_ccs.projectspec": [
        "user/tof200f.c",
        "user/e2025_tof_adapter.c",
        "vl53l0x/core/src/vl53l0x_api.c",
        "vl53l0x/platform/vl53l0x_platform.c",
        "TIXDS110_Connection.xml",
    ],
}

FORBIDDEN = {
    "user/main.c": [
        "tof_uart_init",
        "TOF wiring: PB04",
        "PB05(RX)<-TOF TX",
    ],
    "ml_libs/ml_i2c.c": [
        "gpio_set(I2C_GPIO, pin, 1U)",
    ],
}


def main() -> int:
    errors: list[str] = []
    checks: list[dict[str, str]] = []

    for relative, markers in REQUIRED.items():
        path = PROJECT / relative
        if not path.is_file():
            errors.append(f"missing file: {relative}")
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for marker in markers:
            passed = marker in text
            checks.append({"file": relative, "check": marker,
                           "result": "PASS" if passed else "FAIL"})
            if not passed:
                errors.append(f"{relative}: missing {marker!r}")

    for relative, markers in FORBIDDEN.items():
        path = PROJECT / relative
        text = path.read_text(encoding="utf-8", errors="replace")
        for marker in markers:
            passed = marker not in text
            checks.append({"file": relative, "check": f"forbidden: {marker}",
                           "result": "PASS" if passed else "FAIL"})
            if not passed:
                errors.append(f"{relative}: stale marker {marker!r}")

    main_text = (PROJECT / "user/main.c").read_text(
        encoding="utf-8", errors="replace")
    isr_text = main_text.split("void TIMG8_IRQHandler()", 1)[1]
    for marker in ("tof200f_init", "tof200f_read_mm", "tof200f_poll_mm",
                   "VL53L0X_", "delay_ms", "delay_us"):
        passed = marker not in isr_text
        checks.append({"file": "user/main.c",
                       "check": f"ISR forbidden: {marker}",
                       "result": "PASS" if passed else "FAIL"})
        if not passed:
            errors.append(f"user/main.c ISR contains {marker!r}")

    firmware = ROOT / "output" / "firmware" / "car_final_ccs_tof200f_i2c_test.out"
    digest = None
    if firmware.is_file():
        digest = hashlib.sha256(firmware.read_bytes()).hexdigest().upper()
    else:
        errors.append(f"missing firmware: {firmware.relative_to(ROOT)}")

    summary = {
        "result": "PASS" if not errors else "FAIL",
        "hardware_validation": "NOT_RUN",
        "bus": {"scl": "PB0", "sda": "PB1", "open_drain": True},
        "vl53l0x_address_7bit": "0x29",
        "expected_model_id": "0xEEAA",
        "firmware_sha256": digest,
        "checks": checks,
        "errors": errors,
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    report = OUTPUT / "summary.json"
    report.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
                      encoding="utf-8")

    print(f"TOF200F I2C audit: {summary['result']}")
    print("Hardware validation: NOT_RUN")
    print(f"Report: {report}")
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
