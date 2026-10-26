#!/usr/bin/env python3
"""Audit the final Tianmengxing serial/I2C pin plan against source and TI SDK."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "car_final_ccs"
OUTPUT = ROOT / "output" / "uart_pin_audit"

UART_PLAN = [
    {
        "interface": "CH340 debug",
        "instance": "UART0",
        "tx": "PA10",
        "rx": "PA11",
        "baud": "115200",
        "default_state": "enabled",
        "note": "Tianmengxing onboard CH340; do not share with K230",
    },
    {
        "interface": "IMU",
        "instance": "UART1",
        "tx": "PA8",
        "rx": "PA9",
        "baud": "9600",
        "default_state": "enabled",
        "note": "Existing IMU parser",
    },
    {
        "interface": "K230",
        "instance": "UART2",
        "tx": "PB15",
        "rx": "PA22",
        "baud": "115200",
        "default_state": "feature-gated off",
        "note": "K230 physical 11 TX -> PA22 RX; PB15 TX -> K230 physical 13 RX; physical 9 GND",
    },
    {
        "interface": "Bluetooth",
        "instance": "UART3",
        "tx": "PB2",
        "rx": "PB3",
        "baud": "module dependent",
        "default_state": "reserved; not initialized",
        "note": "Moved from conflicting PB4/PB5; confirm module baud before enable",
    },
]

PIN_OCCUPANCY = [
    ("PA10", "UART0_TX", "CH340 debug", "dedicated"),
    ("PA11", "UART0_RX", "CH340 debug", "dedicated"),
    ("PA8", "UART1_TX", "IMU", "dedicated"),
    ("PA9", "UART1_RX", "IMU", "dedicated"),
    ("PB15", "UART2_TX", "K230", "dedicated"),
    ("PA22", "UART2_RX", "K230", "dedicated"),
    ("PB2", "UART3_TX", "Bluetooth", "dedicated"),
    ("PB3", "UART3_RX", "Bluetooth", "dedicated"),
    ("PB0", "software I2C SCL", "OLED + TOF200F", "shared bus i2c0_soft"),
    ("PB1", "software I2C SDA", "OLED + TOF200F", "shared bus i2c0_soft"),
    ("PB25", "GPIO input", "track D7 / R3", "dedicated"),
]

SOURCE_CHECKS = {
    "ml_libs/ml_uart.c": [
        "IOMUX_PINCM21_PF_UART0_TX",
        "IOMUX_PINCM22_PF_UART0_RX",
        "IOMUX_PINCM19_PF_UART1_TX",
        "IOMUX_PINCM20_PF_UART1_RX",
        "IOMUX_PINCM32_PF_UART2_TX",
        "IOMUX_PINCM47_PF_UART2_RX",
        "IOMUX_PINCM15_PF_UART3_TX",
        "IOMUX_PINCM16_PF_UART3_RX",
    ],
    "ml_libs/ml_oled.h": [
        "#define OLED_SCL_Pin  DL_GPIO_PIN_0",
        "#define OLED_SDA_Pin  DL_GPIO_PIN_1",
        "#define OLED_SCL PB0",
        "#define OLED_SDA PB1",
    ],
    "ml_libs/ml_oled.c": [
        "DL_GPIO_disableOutput(OLED_GPIO, pin)",
        "Release SDA so the slave can drive the ACK bit",
    ],
    "ml_libs/ml_i2c.h": [
        "#define I2C_SCL              PB0",
        "#define I2C_SDA              PB1",
    ],
    "ml_libs/ml_i2c.c": [
        "DL_GPIO_disableOutput(I2C_GPIO, pin)",
        "I2C_WriteBuffer",
        "I2C_ReadBuffer",
    ],
    "user/tof200f.h": [
        "TOF200F_I2C_ADDRESS_8BIT 0x52U",
        "TOF200F_EXPECTED_MODEL_ID 0xEEAAU",
    ],
    "user/xunji.c": [
        "D7/R3=PB25",
        "gpio_init(GPIOB, DL_GPIO_PIN_25, PB25, IN_UP)",
        "gpio_get(GPIOB, DL_GPIO_PIN_25)",
    ],
    "user/track.c": [
        "gpio_init(GPIOB, DL_GPIO_PIN_25, PB25, IN_UP)",
        "gpio_get(GPIOB, DL_GPIO_PIN_25)",
    ],
    "user/app_config.h": [
        "#define E2025_ENABLE_K230_UART0_BRIDGE 0",
        "#define E2025_ENABLE_K230_UART2 0",
    ],
    "user/main.c": [
        "#if E2025_ENABLE_K230_UART2",
        "uart_init(UART2, 115200, 1)",
        "Wiring: PB0=SCL, PB1=SDA",
        "tof200f_read_mm",
    ],
    "user/isr.c": [
        "void UART2_IRQHandler(void)",
        "k230_link_push_byte(byte)",
    ],
}

FORBIDDEN_SOURCE_MARKERS = {
    "user/xunji.c": ["D7=PA3", "DL_GPIO_PIN_3,  PA3,  IN_UP"],
    "user/track.c": ["DL_GPIO_PIN_3,  PA3,  IN_UP"],
    "user/main.c": ["tof_uart_init", "TOF wiring: PB04", "PB05(RX)<-TOF TX"],
}

SDK_MACROS = [
    "IOMUX_PINCM21_PF_UART0_TX",
    "IOMUX_PINCM22_PF_UART0_RX",
    "IOMUX_PINCM19_PF_UART1_TX",
    "IOMUX_PINCM20_PF_UART1_RX",
    "IOMUX_PINCM32_PF_UART2_TX",
    "IOMUX_PINCM47_PF_UART2_RX",
    "IOMUX_PINCM15_PF_UART3_TX",
    "IOMUX_PINCM16_PF_UART3_RX",
]


def find_sdk_header(explicit: str | None) -> Path:
    candidates = []
    if explicit:
        candidates.append(Path(explicit))
    candidates.extend(
        [
            Path(r"E:\2\TI\CCS\mspm0_sdk_2_00_01_00_local\source\ti\devices\msp\m0p\mspm0g350x.h"),
            Path(r"C:\ti\mspm0_sdk_2_00_01_00\source\ti\devices\msp\m0p\mspm0g350x.h"),
        ]
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise FileNotFoundError("MSPM0G350X SDK header not found; pass --sdk-header")


def check_sources(errors: list[str]) -> list[dict[str, str]]:
    results = []
    for relative, markers in SOURCE_CHECKS.items():
        path = PROJECT / relative
        if not path.is_file():
            errors.append(f"missing source: {relative}")
            continue
        text = path.read_text(encoding="utf-8")
        for marker in markers:
            ok = marker in text
            results.append({"file": relative, "marker": marker, "result": "PASS" if ok else "FAIL"})
            if not ok:
                errors.append(f"{relative}: missing marker {marker!r}")

    for relative, markers in FORBIDDEN_SOURCE_MARKERS.items():
        path = PROJECT / relative
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        for marker in markers:
            ok = marker not in text
            results.append(
                {
                    "file": relative,
                    "marker": f"forbidden: {marker}",
                    "result": "PASS" if ok else "FAIL",
                }
            )
            if not ok:
                errors.append(f"{relative}: stale marker still present {marker!r}")
    return results


def check_uniqueness(errors: list[str]) -> None:
    instances = [row["instance"] for row in UART_PLAN]
    if len(instances) != len(set(instances)):
        errors.append("UART instance is assigned to more than one endpoint")

    uart_pins = []
    for row in UART_PLAN:
        uart_pins.extend([row["tx"], row["rx"]])
    if len(uart_pins) != len(set(uart_pins)):
        errors.append("UART pin is assigned more than once")

    restricted = {"PA2", "PA5", "PA6", "PA19", "PA20", "PB6", "PB7", "PB8", "PB9", "PB10", "PB11", "PB14", "PB21", "PB22", "PB26"}
    bad = sorted(set(uart_pins) & restricted)
    if bad:
        errors.append("new serial plan uses Tianmengxing restricted pins: " + ", ".join(bad))


def write_csv(path: Path, fieldnames: list[str], rows: list[dict[str, str]]) -> None:
    with path.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdk-header", help="Path to mspm0g350x.h")
    args = parser.parse_args()

    errors: list[str] = []
    source_results = check_sources(errors)
    check_uniqueness(errors)

    sdk_header = find_sdk_header(args.sdk_header)
    sdk_text = sdk_header.read_text(encoding="utf-8", errors="replace")
    for macro in SDK_MACROS:
        if macro not in sdk_text:
            errors.append(f"TI SDK header missing mux macro: {macro}")

    OUTPUT.mkdir(parents=True, exist_ok=True)
    write_csv(OUTPUT / "uart_plan.csv", list(UART_PLAN[0]), UART_PLAN)
    occupancy_rows = [
        {"pin": pin, "function": function, "endpoint": endpoint, "sharing": sharing}
        for pin, function, endpoint, sharing in PIN_OCCUPANCY
    ]
    write_csv(OUTPUT / "pin_occupancy.csv", ["pin", "function", "endpoint", "sharing"], occupancy_rows)

    summary = {
        "result": "PASS" if not errors else "FAIL",
        "hardware_validation": "NOT_RUN",
        "sdk_header": str(sdk_header),
        "uart_endpoint_count": len(UART_PLAN),
        "source_checks": source_results,
        "errors": errors,
    }
    (OUTPUT / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )

    print(f"UART/pin plan audit: {summary['result']}")
    print(f"TI SDK header: {sdk_header}")
    print(f"Report: {OUTPUT / 'summary.json'}")
    for error in errors:
        print(f"ERROR: {error}")
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
