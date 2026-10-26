#!/usr/bin/env python3
"""Offline replay of the feature-gated E2025 drive adapter."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

from dual_motor_diag_safety_sim import ESTOP, NONE, Monitor
from dual_speed_pi_sim import FEEDFORWARD, IntegerPI
from e2025_track_sim import CENTER, LEFT_CORNER
from track_cascade_sim import TrackOuter


SAMPLE_TICKS = 10


@dataclass(frozen=True)
class DriveOutput:
    active: bool = False
    sample_updated: bool = False
    left_target: int = 0
    right_target: int = 0
    left_sample: int = 0
    right_sample: int = 0
    left_pwm: int = 0
    right_pwm: int = 0
    fault: str = NONE


@dataclass
class DriveAdapter:
    outer: TrackOuter = field(default_factory=TrackOuter)
    left_pi: IntegerPI = field(default_factory=IntegerPI)
    right_pi: IntegerPI = field(default_factory=IntegerPI)
    guard: Monitor = field(default_factory=Monitor)
    left_accumulator: int = 0
    right_accumulator: int = 0
    left_pwm: int = 0
    right_pwm: int = 0
    previous_left_target: int = 0
    previous_right_target: int = 0
    sample_ticks: int = 0
    was_enabled: bool = False
    fault: str = NONE

    def start_session(self) -> None:
        self.outer = TrackOuter()
        self.left_pi.reset()
        self.right_pi.reset()
        self.guard.reset()
        self.left_accumulator = 0
        self.right_accumulator = 0
        self.left_pwm = FEEDFORWARD
        self.right_pwm = FEEDFORWARD
        self.previous_left_target = 0
        self.previous_right_target = 0
        self.sample_ticks = 0
        self.was_enabled = True

    def tick(
        self,
        *,
        track_enable: bool,
        emergency_stop: bool = False,
        pattern: int = CENTER,
        left_delta: int = 0,
        right_delta: int = 0,
    ) -> DriveOutput:
        if track_enable and emergency_stop and self.fault == NONE:
            self.fault = ESTOP

        if not track_enable:
            self.left_accumulator = 0
            self.right_accumulator = 0
            self.left_pwm = 0
            self.right_pwm = 0
            self.sample_ticks = 0
            self.was_enabled = False
            return DriveOutput(fault=self.fault)
        if self.fault != NONE:
            return DriveOutput(fault=self.fault)
        if not self.was_enabled:
            self.start_session()

        outer = self.outer.step(pattern)
        if outer.left_target == 0:
            self.left_pwm = 0
            self.left_pi.reset()
        elif self.previous_left_target == 0:
            self.left_pwm = FEEDFORWARD
            self.left_pi.reset()
        if outer.right_target == 0:
            self.right_pwm = 0
            self.right_pi.reset()
        elif self.previous_right_target == 0:
            self.right_pwm = FEEDFORWARD
            self.right_pi.reset()
        self.previous_left_target = outer.left_target
        self.previous_right_target = outer.right_target

        self.left_accumulator += left_delta
        self.right_accumulator += right_delta
        self.sample_ticks += 1
        if self.sample_ticks < SAMPLE_TICKS:
            return DriveOutput(
                active=True,
                left_target=outer.left_target,
                right_target=outer.right_target,
                left_pwm=self.left_pwm,
                right_pwm=self.right_pwm,
            )

        left_sample = self.left_accumulator
        right_sample = self.right_accumulator
        self.fault = self.guard.step_targets(
            False,
            left_sample,
            right_sample,
            outer.left_target,
            outer.right_target,
        )
        if self.fault == NONE:
            self.left_pwm = self.left_pi.step(outer.left_target, abs(left_sample))
            self.right_pwm = self.right_pi.step(
                outer.right_target, abs(right_sample)
            )
            active = True
        else:
            self.left_pwm = 0
            self.right_pwm = 0
            active = False

        self.left_accumulator = 0
        self.right_accumulator = 0
        self.sample_ticks = 0
        return DriveOutput(
            active=active,
            sample_updated=True,
            left_target=outer.left_target,
            right_target=outer.right_target,
            left_sample=left_sample,
            right_sample=right_sample,
            left_pwm=self.left_pwm,
            right_pwm=self.right_pwm,
            fault=self.fault,
        )


def source_checks(workspace: Path) -> None:
    adapter = (workspace / "car_final_ccs/user/e2025_drive_adapter.c").read_text(
        encoding="utf-8"
    )
    main = (workspace / "car_final_ccs/user/main.c").read_text(encoding="utf-8")
    config = (workspace / "car_final_ccs/user/app_config.h").read_text(
        encoding="utf-8"
    )
    projectspec = (workspace / "car_final_ccs/car_final_ccs.projectspec").read_text(
        encoding="utf-8"
    )

    assert "#define E2025_OFFLINE_SIMULATION 1" in config
    assert "#define E2025_ENABLE_CASCADE_TRACKING 0" in config
    assert "#if E2025_OFFLINE_SIMULATION || E2025_ENABLE_CASCADE_TRACKING" in main
    assert "motor_safe_init_disabled();" in main
    assert "motor_set_standby(0U);" in main
    assert "input->track_pattern = pattern;" in main
    assert "gpio_get(GPIOB, DL_GPIO_PIN_27)" in main
    assert "e2025_status_was_tracking(&status)" in main
    assert "input->emergency_stop && track_was_active" in main
    assert "e2025_drive_adapter_tick(" in main
    assert "dual_motor_guard_step_targets(" in adapter
    assert "E2025_DRIVE_SAMPLE_TICKS" in adapter
    assert "user/e2025_drive_adapter.c" in projectspec


def main() -> None:
    workspace = Path(__file__).resolve().parents[1]
    results: list[dict[str, object]] = []

    adapter = DriveAdapter()
    disabled = adapter.tick(track_enable=False, emergency_stop=True)
    assert not disabled.active and disabled.left_pwm == disabled.right_pwm == 0
    assert disabled.fault == NONE
    results.append({"case": "disabled_is_zero_and_does_not_latch_estop", "status": "PASS"})

    samples = []
    for _ in range(10):
        samples.append(
            adapter.tick(
                track_enable=True, pattern=CENTER, left_delta=1, right_delta=1
            )
        )
    assert not any(item.sample_updated for item in samples[:-1])
    assert samples[-1].sample_updated
    assert (samples[-1].left_sample, samples[-1].right_sample) == (10, 10)
    assert (samples[-1].left_pwm, samples[-1].right_pwm) == (
        FEEDFORWARD,
        FEEDFORWARD,
    )
    results.append({"case": "ten_10ms_ticks_form_one_100ms_sample", "status": "PASS"})

    corner = adapter.tick(
        track_enable=True, pattern=LEFT_CORNER, left_delta=0, right_delta=1
    )
    assert corner.active and corner.left_target == 0 and corner.left_pwm == 0
    assert corner.right_target > 0 and corner.right_pwm > 0
    results.append({"case": "corner_zero_target_stops_wheel_same_tick", "status": "PASS"})

    adapter = DriveAdapter()
    for _ in range(200):
        corner = adapter.tick(
            track_enable=True, pattern=LEFT_CORNER, left_delta=0, right_delta=1
        )
    assert corner.fault == NONE and corner.active
    results.append({"case": "zero_target_is_ignored_by_feedback_guard", "status": "PASS"})

    adapter = DriveAdapter()
    for _ in range(160):
        missing = adapter.tick(track_enable=True, pattern=CENTER)
    assert missing.fault == "NO FEEDBACK"
    assert not missing.active and missing.left_pwm == missing.right_pwm == 0
    results.append({"case": "missing_feedback_latches_and_stops", "status": "PASS"})

    adapter = DriveAdapter()
    adapter.tick(track_enable=True, pattern=CENTER, left_delta=1, right_delta=1)
    stopped = adapter.tick(
        track_enable=True,
        emergency_stop=True,
        pattern=CENTER,
        left_delta=1,
        right_delta=1,
    )
    assert stopped.fault == ESTOP and not stopped.active
    assert stopped.left_pwm == stopped.right_pwm == 0
    latched = adapter.tick(track_enable=False)
    assert latched.fault == ESTOP
    results.append({"case": "sw4_estop_is_same_tick_and_latched", "status": "PASS"})

    source_checks(workspace)
    results.append({"case": "firmware_gate_and_safety_markers", "status": "PASS"})

    output = workspace / "output/e2025_drive_adapter_sim"
    output.mkdir(parents=True, exist_ok=True)
    summary = {
        "suite": "e2025_drive_adapter",
        "outer_tick_ms": 10,
        "inner_sample_ms": 100,
        "feature_enabled_in_default_firmware": False,
        "hardware_verified": False,
        "target_counts_are_placeholders": True,
        "cases": results,
    }
    (output / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    for result in results:
        print(f"PASS {result['case']}")
    print(f"PASS summary={output / 'summary.json'}")


if __name__ == "__main__":
    main()
