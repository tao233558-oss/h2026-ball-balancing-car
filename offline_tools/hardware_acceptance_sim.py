#!/usr/bin/env python3
"""Offline checks for the read-only hardware acceptance firmware profile."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "output" / "hardware_acceptance_sim"
KEY_DEBOUNCE_TICKS = 3
ENCODER_MIN_ACTIVITY = 4


@dataclass
class AcceptanceModel:
    key_seen: int = 0
    key_debounce: list[int] = field(default_factory=lambda: [0, 0, 0, 0])
    track_white_seen: int = 0
    track_black_seen: int = 0
    left_activity: int = 0
    right_activity: int = 0
    motor_pwm_left: int = 0
    motor_pwm_right: int = 0
    standby: int = 0
    k230: str = "BLOCKED_UART2_UNVERIFIED"

    def tick(self, key_raw: int, track_black: int, enc1: int, enc2: int) -> None:
        key_raw &= 0x0F
        track_black &= 0xFF
        for key in range(4):
            bit = 1 << key
            if key_raw & bit:
                self.key_debounce[key] = min(
                    KEY_DEBOUNCE_TICKS, self.key_debounce[key] + 1
                )
                if self.key_debounce[key] >= KEY_DEBOUNCE_TICKS:
                    self.key_seen |= bit
            else:
                self.key_debounce[key] = 0

        self.track_black_seen |= track_black
        self.track_white_seen |= (~track_black) & 0xFF
        self.left_activity += abs(enc1)
        self.right_activity += abs(enc2)

        # The profile has no path that may energize an actuator.
        self.motor_pwm_left = 0
        self.motor_pwm_right = 0
        self.standby = 0

    @property
    def track_pass(self) -> int:
        return self.track_white_seen & self.track_black_seen

    @property
    def inputs_pass(self) -> bool:
        return (
            self.key_seen == 0x0F
            and self.track_pass == 0xFF
            and self.left_activity >= ENCODER_MIN_ACTIVITY
            and self.right_activity >= ENCODER_MIN_ACTIVITY
        )


def require(condition: bool, name: str, checks: dict[str, str]) -> None:
    checks[name] = "PASS" if condition else "FAIL"
    if not condition:
        raise AssertionError(name)


def source_checks(checks: dict[str, str]) -> None:
    config = (ROOT / "car_final_ccs/user/app_config.h").read_text(encoding="utf-8")
    source = (ROOT / "car_final_ccs/user/hardware_acceptance.c").read_text(
        encoding="utf-8"
    )
    main = (ROOT / "car_final_ccs/user/main.c").read_text(encoding="utf-8")
    spec = (ROOT / "car_final_ccs/car_final_ccs.projectspec").read_text(
        encoding="utf-8"
    )

    require("#define APP_PROFILE_HW_ACCEPT  0" in config,
            "acceptance_profile_default_off", checks)
    require("APP_PROFILE_HW_ACCEPT" in main and "hardware_acceptance_run();" in main,
            "main_has_acceptance_entry", checks)
    require("motor_safe_init_disabled();" in source
            and source.count("Set_left_pwm(0);") >= 2
            and source.count("Set_right_pwm(0);") >= 2
            and source.count("motor_set_standby(0U);") >= 2,
            "firmware_reasserts_safe_motor_outputs", checks)
    require("no STEP/DIR/laser output" in source
            and "K230=BLOCKED_UART2_UNVERIFIED" in source,
            "unverified_outputs_are_explicitly_blocked", checks)
    require('path="user/hardware_acceptance.c"' in spec,
            "ccs_projectspec_includes_acceptance_source", checks)


def main() -> int:
    checks: dict[str, str] = {}
    model = AcceptanceModel()

    # All sensors see white at boot.
    model.tick(0x00, 0x00, 0, 0)
    require(model.track_white_seen == 0xFF, "white_baseline_seen", checks)

    # A one-tick glitch must not count as a valid key press.
    model.tick(0x0F, 0x00, 0, 0)
    model.tick(0x00, 0x00, 0, 0)
    require(model.key_seen == 0x00, "single_tick_key_noise_rejected", checks)

    # Press each switch for 30 ms, then release it.
    for key in range(4):
        for _ in range(KEY_DEBOUNCE_TICKS):
            model.tick(1 << key, 0x00, 0, 0)
        model.tick(0x00, 0x00, 0, 0)
    require(model.key_seen == 0x0F, "all_four_keys_pass", checks)

    # Move a black strip over every channel once.
    for channel in range(8):
        model.tick(0x00, 1 << channel, 0, 0)
    require(model.track_pass == 0xFF, "all_track_channels_see_white_and_black", checks)

    model.tick(0x00, 0x00, 3, 0)
    require(not model.inputs_pass, "encoder_threshold_rejects_tiny_activity", checks)
    model.tick(0x00, 0x00, -1, -4)
    require(model.inputs_pass, "both_encoder_channels_pass", checks)
    require(model.motor_pwm_left == 0 and model.motor_pwm_right == 0
            and model.standby == 0,
            "actuators_remain_disabled", checks)
    require(model.k230 == "BLOCKED_UART2_UNVERIFIED",
            "k230_not_falsely_reported_as_pass", checks)

    source_checks(checks)
    summary = {
        "tick_ms": 10,
        "key_debounce_ms": KEY_DEBOUNCE_TICKS * 10,
        "encoder_min_activity": ENCODER_MIN_ACTIVITY,
        "final": {
            "key_pass_mask": f"0x{model.key_seen:02X}",
            "track_pass_mask": f"0x{model.track_pass:02X}",
            "left_encoder_activity": model.left_activity,
            "right_encoder_activity": model.right_activity,
            "motor_pwm_left": model.motor_pwm_left,
            "motor_pwm_right": model.motor_pwm_right,
            "standby": model.standby,
            "k230": model.k230,
            "inputs_pass": model.inputs_pass,
        },
        "checks": checks,
    }
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    summary_path = OUT_DIR / "summary.json"
    summary_path.write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    for name, status in checks.items():
        print(f"{status} {name}")
    print(f"PASS summary={summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
