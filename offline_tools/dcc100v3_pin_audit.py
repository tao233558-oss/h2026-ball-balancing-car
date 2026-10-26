#!/usr/bin/env python3
"""Static DCC-100v3 pin and safety audit for the Tianmengxing car project."""

from __future__ import annotations

import csv
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "car_final_ccs"
DCC_ROOT = ROOT / "咸鱼24电赛h/DCC-100v3双路步进电机驱动模块资料"


RECOMMENDED = [
    ("PAN_STEP", "PA26", 30, "DCC_PWM1", "existing"),
    ("PAN_DIR", "PA14", 7, "DCC_DIR1", "existing"),
    ("TILT_STEP", "PB4", 52, "DCC_PWM2", "new"),
    ("TILT_DIR", "PB5", 53, "DCC_DIR2", "new"),
    ("DCC_AWAKE", "PB17", 14, "DCC_SLP1+SLP2", "new"),
]

VENDOR_EXAMPLE_PINS = {
    "PWM1": "PA28",
    "DIR1": "PA31",
    "DCY1": "PB24",
    "SLP1": "PB20",
    "RST1": "PA7",
    "PWM2": "PA12",
    "DIR2": "PA13",
    "DCY2": "PA14",
    "SLP2": "PA15",
    "RST2": "PA16",
}

ACTIVE_OWNERS = {
    "PA28": "track_D4",
    "PA31": "track_D3",
    "PB24": "TB6612_AIN1",
    "PB20": "TB6612_AIN2",
    "PA7": "track_D6",
    "PA12": "left_encoder_A",
    "PA13": "left_encoder_B",
    "PA14": "existing_stepper_DIR",
    "PA15": "right_encoder_B",
    "PA16": "track_D8",
}


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def main() -> None:
    checks: list[str] = []
    manual = DCC_ROOT / "DCC-100v3说明书-2026-05-22.pdf"
    vendor_syscfg = (
        DCC_ROOT
        / "【云台】08_按键上下左右遥控开环电机"
        / "yuntai_8_yao2kong4kai1huan2/empty.syscfg"
    )
    guide = PROJECT / "docs/DCC100V3_INTEGRATION_AUDIT.md"
    executor_header = read(PROJECT / "user/e2025_aim_executor.h")
    executor_source = read(PROJECT / "user/e2025_aim_executor.c")
    config = read(PROJECT / "user/app_config.h")
    main_source = read(PROJECT / "user/main.c")
    source_text = "\n".join(
        read(path) for path in (PROJECT / "user").glob("*.c")
    )

    assert manual.is_file() and manual.stat().st_size > 1_000_000
    checks.append("dcc100v3_manual_present")

    vendor_text = read(vendor_syscfg)
    for signal, pin in VENDOR_EXAMPLE_PINS.items():
        assert re.search(
            rf'\.\$assign\s*=\s*"{re.escape(pin)}"', vendor_text
        ), (signal, pin)
        assert pin in ACTIVE_OWNERS, (signal, pin)
    checks.append("all_vendor_example_control_pins_conflict_with_current_car")

    assert "#define E2025_OFFLINE_SIMULATION 1" in config
    assert "#define E2025_ENABLE_AIM_ADAPTER 0" in config
    assert "#define E2025_AIM_PHYSICAL_LIMITS_PRESENT 0" in config
    assert "#define APP_PROFILE_AIM_DIAG   0" in config
    assert "e2025_aim_executor_tick(" not in main_source
    checks.append("runtime_gpio_backend_remains_disabled")
    checks.append("physical_limit_switches_confirmed_absent")

    assert "driver_awake_allowed" in executor_header
    assert "driver_awake_allowed = 1U" in executor_source
    checks.append("dcc_sleep_wake_permission_is_logical_only")

    forbidden_runtime_patterns = {
        "PB4": "gpio_init(GPIOB, DL_GPIO_PIN_4",
        "PB5": "gpio_init(GPIOB, DL_GPIO_PIN_5",
        "PB17": "gpio_init(GPIOB, DL_GPIO_PIN_17",
    }
    for pin, pattern in forbidden_runtime_patterns.items():
        assert pattern not in source_text, pin
    checks.append("normal_e2025_runtime_has_no_direct_dcc_gpio_init")

    for pattern in (
        "#define TILT_STEPPER_PWM_IOMUX PB4",
        "#define TILT_STEPPER_DIR_IOMUX PB5",
        "#define DCC_AWAKE_IOMUX PB17",
        "#define AIM_DIAG_PULSES_PER_PRESS 8U",
        "static void aim_diag_run(void)",
    ):
        assert pattern in main_source
    assert "gpio_set(DCC_AWAKE_PORT, DCC_AWAKE_PIN, 0U);" in main_source
    checks.append("low_speed_dual_axis_diagnostic_profile_present")

    guide_text = read(guide)
    for net, pin, lqfp, dcc_signal, status in RECOMMENDED:
        assert net in guide_text and pin in guide_text
        assert status in ("existing", "new")
        assert lqfp > 0 and dcc_signal.startswith("DCC_")
    checks.append("integration_guide_contains_recommended_pin_plan")

    output = ROOT / "output/dcc100v3_integration_audit"
    output.mkdir(parents=True, exist_ok=True)
    with (output / "recommended_pin_plan.csv").open(
        "w", encoding="utf-8", newline=""
    ) as handle:
        writer = csv.writer(handle)
        writer.writerow(("net", "mcu_pin", "lqfp64", "dcc_signal", "status"))
        writer.writerows(RECOMMENDED)

    summary = {
        "suite": "dcc100v3_integration_audit",
        "status": "PASS",
        "module_logic_level_v": 3.3,
        "module_motor_supply_v": "11-26",
        "new_recommended_pins": ["PB4", "PB5", "PB17"],
        "physical_limit_switches_present": False,
        "position_reference": "power_on_relative_zero_only",
        "hardware_gpio_bound": False,
        "diagnostic_gpio_bound": True,
        "diagnostic_profile_default_enabled": False,
        "checks": checks,
        "remaining_confirmations": [
            "latest_pcb_routes_pb4_pb5_pb17_to_dcc100v3",
            "laser_enable_pin_and_active_polarity",
            "scope_check_of_pwm_dir_sleep_timing",
        ],
        "accepted_risks": [
            "no_automatic_homing_or_absolute_mechanical_reference",
            "relative_soft_limits_can_drift_after_missed_steps_or_obstruction",
        ],
    }
    (output / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
