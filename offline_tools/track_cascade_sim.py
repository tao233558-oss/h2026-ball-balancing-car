#!/usr/bin/env python3
"""Joint replay: line-sensor outer loop plus encoder PI inner loops."""

from __future__ import annotations

import csv
import json
from dataclasses import dataclass, field
from pathlib import Path

from dual_motor_diag_safety_sim import NONE, Monitor
from dual_speed_pi_sim import IntegerPI, MotorPlant, PWM_MAX
from e2025_track_sim import (
    CENTER,
    LEFT_CORNER,
    LEFT_OFFSET,
    RIGHT_OFFSET,
    weighted_error,
)


BASE_TARGET = 10
MIN_TARGET = 7
MAX_TARGET = 13
SEARCH_TARGET = 7
CORNER_TARGET = 10
CORRECTION_DIVISOR = 4
CORRECTION_LIMIT = 3
SAMPLE_TICKS = 10


def c_div(value: int, divisor: int) -> int:
    """C99 signed integer division truncates toward zero."""
    return int(value / divisor)


@dataclass(frozen=True)
class OuterOutput:
    pattern: int
    raw_error: int
    filtered_error: int
    left_target: int
    right_target: int
    mode: str


@dataclass
class TrackOuter:
    filtered_error: int = 0
    last_turn_direction: int = 1
    last_raw_error: int = 0

    def step(self, pattern: int) -> OuterOutput:
        pattern &= 0xFF
        raw_error = weighted_error(pattern, self.last_raw_error)
        if pattern != 0:
            self.last_raw_error = raw_error

        if pattern == 0:
            if self.last_turn_direction < 0:
                return OuterOutput(
                    pattern,
                    raw_error,
                    self.filtered_error,
                    0,
                    SEARCH_TARGET,
                    "SEARCH_LEFT",
                )
            return OuterOutput(
                pattern,
                raw_error,
                self.filtered_error,
                SEARCH_TARGET,
                0,
                "SEARCH_RIGHT",
            )

        if raw_error < -8:
            self.last_turn_direction = -1
        elif raw_error > 8:
            self.last_turn_direction = 1

        if (pattern & 0x03) == 0x03 and (pattern & 0x3C) == 0:
            self.filtered_error = raw_error
            return OuterOutput(
                pattern, raw_error, raw_error, 0, CORNER_TARGET, "CORNER_LEFT"
            )
        if (pattern & 0xC0) == 0xC0 and (pattern & 0x3C) == 0:
            self.filtered_error = raw_error
            return OuterOutput(
                pattern, raw_error, raw_error, CORNER_TARGET, 0, "CORNER_RIGHT"
            )

        self.filtered_error = c_div(self.filtered_error + raw_error, 2)
        correction = max(
            -CORRECTION_LIMIT,
            min(CORRECTION_LIMIT, c_div(self.filtered_error, CORRECTION_DIVISOR)),
        )
        return OuterOutput(
            pattern,
            raw_error,
            self.filtered_error,
            max(MIN_TARGET, min(MAX_TARGET, BASE_TARGET + correction)),
            max(MIN_TARGET, min(MAX_TARGET, BASE_TARGET - correction)),
            "FOLLOW",
        )


@dataclass
class CascadeRig:
    outer: TrackOuter = field(default_factory=TrackOuter)
    left_pi: IntegerPI = field(default_factory=IntegerPI)
    right_pi: IntegerPI = field(default_factory=IntegerPI)
    guard: Monitor = field(default_factory=Monitor)
    plant: MotorPlant = field(default_factory=MotorPlant)
    sample_ticks: int = 0
    left_pwm: int = 0
    right_pwm: int = 0
    left_counts: int = 0
    right_counts: int = 0
    fault: str = NONE
    output: OuterOutput = field(
        default_factory=lambda: OuterOutput(CENTER, 0, 0, 10, 10, "FOLLOW")
    )

    def tick(self, pattern: int) -> dict[str, int | str] | None:
        self.output = self.outer.step(pattern)
        sample_due = self.sample_ticks == 0
        row: dict[str, int | str] | None = None

        if sample_due and self.fault == NONE:
            self.left_counts, self.right_counts = self.plant.step(
                self.left_pwm, self.right_pwm
            )
            detected = self.guard.step_targets(
                False,
                self.left_counts,
                self.right_counts,
                self.output.left_target,
                self.output.right_target,
            )
            if detected != NONE:
                self.fault = detected
                self.left_pwm = 0
                self.right_pwm = 0
            else:
                self.left_pwm = self.left_pi.step(
                    self.output.left_target, abs(self.left_counts)
                )
                self.right_pwm = self.right_pi.step(
                    self.output.right_target, abs(self.right_counts)
                )

            row = {
                "pattern_hex": f"0x{pattern:02X}",
                "mode": self.output.mode,
                "raw_error": self.output.raw_error,
                "filtered_error": self.output.filtered_error,
                "left_target": self.output.left_target,
                "right_target": self.output.right_target,
                "left_counts": self.left_counts,
                "right_counts": self.right_counts,
                "left_pwm": self.left_pwm,
                "right_pwm": self.right_pwm,
                "fault": self.fault,
            }

        self.sample_ticks = (self.sample_ticks + 1) % SAMPLE_TICKS
        return row


def append_segment(
    trace: list[tuple[str, int]], phase: str, pattern: int, ticks: int
) -> None:
    trace.extend((phase, pattern) for _ in range(ticks))


def make_trace() -> list[tuple[str, int]]:
    trace: list[tuple[str, int]] = []
    append_segment(trace, "center_start", CENTER, 300)
    append_segment(trace, "left_curve", LEFT_OFFSET, 200)
    append_segment(trace, "center_after_left", CENTER, 100)
    append_segment(trace, "right_curve", RIGHT_OFFSET, 200)
    append_segment(trace, "center_before_noise", CENTER, 100)
    append_segment(trace, "single_tick_noise", 0x01, 1)
    append_segment(trace, "noise_recovery", CENTER, 99)
    append_segment(trace, "left_before_loss", LEFT_OFFSET, 30)
    append_segment(trace, "line_lost", 0x00, 60)
    append_segment(trace, "line_reacquired", CENTER, 200)
    append_segment(trace, "left_corner", LEFT_CORNER, 100)
    append_segment(trace, "final_center", CENTER, 300)
    return trace


def target_aware_guard_checks() -> None:
    guard = Monitor()
    for _ in range(20):
        assert guard.step_targets(False, 0, 10, 0, 10) == NONE

    guard.reset()
    for _ in range(5):
        assert guard.step_targets(False, 7, 13, 7, 13) == NONE
    for _ in range(20):
        assert guard.step_targets(False, 7, 13, 7, 13) == NONE

    guard.reset()
    for _ in range(5):
        assert guard.step_targets(False, 0, 10, 10, 10) == NONE
    assert guard.step_targets(False, 0, 10, 10, 10) == NONE
    assert guard.step_targets(False, 0, 10, 10, 10) == NONE
    assert guard.step_targets(False, 0, 10, 10, 10) == "LEFT MISSING"


def source_checks(workspace: Path) -> None:
    source = (
        workspace / "car_final_ccs/user/track_cascade_control.c"
    ).read_text(encoding="utf-8")
    guard_source = (
        workspace / "car_final_ccs/user/dual_motor_guard.c"
    ).read_text(encoding="utf-8")
    projectspec = (workspace / "car_final_ccs/car_final_ccs.projectspec").read_text(
        encoding="utf-8"
    )
    app_config = (workspace / "car_final_ccs/user/app_config.h").read_text(
        encoding="utf-8"
    )
    main_source = (workspace / "car_final_ccs/user/main.c").read_text(
        encoding="utf-8"
    )

    assert "Counts are per 100 ms" in source
    assert "(controller->filtered_error + weighted_error) / 2" in source
    assert "controller->config.correction_limit" in source
    assert "TRACK_CASCADE_SEARCH_LEFT" in source
    assert "TRACK_CASCADE_CORNER_RIGHT" in source
    assert "Set_left_pwm" not in source
    assert "Set_right_pwm" not in source
    assert "dual_motor_guard_step_targets" in guard_source
    assert "left_abs * right_target" in guard_source
    assert "user/track_cascade_control.c" in projectspec
    assert "#define APP_PROFILE_E2025      1" in app_config
    assert "#define E2025_OFFLINE_SIMULATION 1" in app_config
    assert "#define E2025_ENABLE_CASCADE_TRACKING 0" in app_config
    assert "#if E2025_ENABLE_CASCADE_TRACKING" in main_source
    assert "e2025_drive_adapter_tick(" in main_source


def main() -> None:
    workspace = Path(__file__).resolve().parents[1]
    results: list[dict[str, object]] = []

    outer = TrackOuter()
    center = outer.step(CENTER)
    assert (center.left_target, center.right_target) == (10, 10)
    for _ in range(5):
        left = outer.step(LEFT_OFFSET)
    assert left.left_target < left.right_target
    for _ in range(5):
        right = outer.step(RIGHT_OFFSET)
    assert right.left_target > right.right_target
    results.append({"case": "outer_loop_direction", "status": "PASS"})

    outer = TrackOuter()
    for _ in range(10):
        outer.step(CENTER)
    noise = outer.step(0x01)
    assert MIN_TARGET <= noise.left_target <= MAX_TARGET
    assert MIN_TARGET <= noise.right_target <= MAX_TARGET
    for _ in range(6):
        recovered = outer.step(CENTER)
    assert (recovered.left_target, recovered.right_target) == (10, 10)
    results.append({"case": "single_tick_noise_bounded_and_recovers", "status": "PASS"})

    outer = TrackOuter()
    for _ in range(5):
        outer.step(LEFT_OFFSET)
    lost = outer.step(0)
    corner = outer.step(LEFT_CORNER)
    assert (lost.left_target, lost.right_target, lost.mode) == (0, 7, "SEARCH_LEFT")
    assert (corner.left_target, corner.right_target, corner.mode) == (
        0,
        10,
        "CORNER_LEFT",
    )
    results.append({"case": "lost_line_and_corner_targets", "status": "PASS"})

    target_aware_guard_checks()
    results.append({"case": "target_aware_encoder_guard", "status": "PASS"})

    rig = CascadeRig()
    rows: list[dict[str, int | str]] = []
    phase_rows: dict[str, list[dict[str, int | str]]] = {}
    for tick, (phase, pattern) in enumerate(make_trace()):
        row = rig.tick(pattern)
        if row is not None:
            row = {"tick_10ms": tick, "phase": phase, **row}
            rows.append(row)
            phase_rows.setdefault(phase, []).append(row)

    assert rig.fault == NONE
    assert all(0 <= int(row["left_target"]) <= MAX_TARGET for row in rows)
    assert all(0 <= int(row["right_target"]) <= MAX_TARGET for row in rows)
    assert all(0 <= int(row["left_pwm"]) <= PWM_MAX for row in rows)
    assert all(0 <= int(row["right_pwm"]) <= PWM_MAX for row in rows)
    assert all(
        int(row["left_target"]) < int(row["right_target"])
        for row in phase_rows["left_curve"][-10:]
    )
    assert all(
        int(row["left_target"]) > int(row["right_target"])
        for row in phase_rows["right_curve"][-10:]
    )
    assert any(int(row["left_target"]) == 0 for row in phase_rows["line_lost"])
    assert any(int(row["left_pwm"]) == 0 for row in phase_rows["line_lost"])
    assert any(int(row["left_target"]) == 0 for row in phase_rows["left_corner"])
    assert any(int(row["left_pwm"]) == 0 for row in phase_rows["left_corner"])
    final = phase_rows["final_center"][-1]
    assert 9 <= int(final["left_counts"]) <= 11
    assert 9 <= int(final["right_counts"]) <= 11
    results.append(
        {
            "case": "joint_outer_inner_replay",
            "status": "PASS",
            "final_left_counts": final["left_counts"],
            "final_right_counts": final["right_counts"],
            "final_left_pwm": final["left_pwm"],
            "final_right_pwm": final["right_pwm"],
        }
    )

    source_checks(workspace)
    results.append({"case": "firmware_source_isolated_safe", "status": "PASS"})

    output = workspace / "output/track_cascade_sim"
    output.mkdir(parents=True, exist_ok=True)
    with (output / "joint_replay.csv").open(
        "w", newline="", encoding="utf-8-sig"
    ) as handle:
        writer = csv.DictWriter(handle, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)

    summary = {
        "suite": "track_outer_plus_wheel_pi_inner",
        "outer_tick_ms": 10,
        "inner_sample_ms": 100,
        "target_counts_are_placeholders": True,
        "hardware_verified": False,
        "target_limits": {"minimum_follow": MIN_TARGET, "maximum": MAX_TARGET},
        "cases": results,
    }
    (output / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )

    for result in results:
        print(f"PASS {result['case']}")
    print(f"PASS summary={output / 'summary.json'}")
    print(f"PASS replay={output / 'joint_replay.csv'}")


if __name__ == "__main__":
    main()
