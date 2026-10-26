#!/usr/bin/env python3
"""Deterministic replay tests for the E2025 eight-channel line tracker.

Pattern bits are D1..D8 from bit0..bit7.  A set bit always means that the
sensor is over the black line, regardless of the module's raw active-low
electrical output.
"""

from __future__ import annotations

import csv
import json
from dataclasses import asdict, dataclass
from pathlib import Path


BASE_PWM = 1800
SEARCH_PWM = 1400
KP = 75
KD = 25
PWM_LIMIT = 5000
TURN_PWM = 2200

WEIGHTS = (-35, -25, -15, -5, 5, 15, 25, 35)
CENTER = 0x18  # D4 + D5
LEFT_OFFSET = 0x0C  # D3 + D4
RIGHT_OFFSET = 0x30  # D5 + D6
LEFT_CORNER = 0x03  # D1 + D2
RIGHT_CORNER = 0xC0  # D7 + D8


@dataclass(frozen=True)
class ControlSample:
    pattern: int
    error: int
    left_pwm: int
    right_pwm: int
    mode: str


@dataclass(frozen=True)
class CourseResult:
    requested_laps: int
    completed_laps: int
    corner_events: int
    elapsed_ticks: int
    elapsed_seconds: float


def raw_levels_to_pattern(raw_levels: int) -> int:
    """Convert white=1/black=0 module levels into black-line pattern bits."""
    return (~raw_levels) & 0xFF


def weighted_error(pattern: int, previous_error: int) -> int:
    active_weights = [weight for bit, weight in enumerate(WEIGHTS) if pattern & (1 << bit)]
    if not active_weights:
        return previous_error
    return int(sum(active_weights) / len(active_weights))


def limit_pwm(value: int) -> int:
    return max(-PWM_LIMIT, min(PWM_LIMIT, value))


class TrackController:
    def __init__(self) -> None:
        self.last_error = 0
        self.last_turn_dir = 1

    def step(self, pattern: int) -> ControlSample:
        pattern &= 0xFF
        if pattern == 0:
            if self.last_turn_dir < 0:
                return ControlSample(pattern, self.last_error, 0, SEARCH_PWM, "SEARCH_LEFT")
            return ControlSample(pattern, self.last_error, SEARCH_PWM, 0, "SEARCH_RIGHT")

        error = weighted_error(pattern, self.last_error)
        if error < -8:
            self.last_turn_dir = -1
        elif error > 8:
            self.last_turn_dir = 1

        if (pattern & 0x03) == 0x03 and (pattern & 0x3C) == 0:
            self.last_error = error
            return ControlSample(pattern, error, 0, TURN_PWM, "CORNER_LEFT")
        if (pattern & 0xC0) == 0xC0 and (pattern & 0x3C) == 0:
            self.last_error = error
            return ControlSample(pattern, error, TURN_PWM, 0, "CORNER_RIGHT")

        turn = KP * error + KD * (error - self.last_error)
        self.last_error = error
        left_pwm = max(500, limit_pwm(BASE_PWM + turn))
        right_pwm = max(500, limit_pwm(BASE_PWM - turn))
        return ControlSample(pattern, error, left_pwm, right_pwm, "FOLLOW")


class CornerDetector:
    """Mirror the 10 ms firmware detector: 20 ms qualify, 50 ms clear."""

    def __init__(self) -> None:
        self.armed = True
        self.active_ticks = 0
        self.clear_ticks = 0

    def step(self, pattern: int) -> bool:
        corner_now = (pattern & 0x03) == 0x03 or (pattern & 0xC0) == 0xC0
        if corner_now:
            self.clear_ticks = 0
            self.active_ticks = min(2, self.active_ticks + 1)
            if self.armed and self.active_ticks >= 2:
                self.armed = False
                return True
        else:
            self.active_ticks = 0
            self.clear_ticks = min(5, self.clear_ticks + 1)
            if self.clear_ticks >= 5:
                self.armed = True
        return False


def make_course_trace(laps: int) -> list[int]:
    trace: list[int] = []
    for corner_index in range(laps * 4):
        corner = LEFT_CORNER if corner_index % 2 == 0 else RIGHT_CORNER
        trace.extend([CENTER] * 60)
        trace.extend([corner] * 4)
        trace.append(CENTER)  # one-sample contact bounce; must not recount
        trace.extend([corner] * 8)
        trace.extend([CENTER] * 15)
    return trace


def replay_course(laps: int) -> tuple[CourseResult, list[dict[str, int | str]]]:
    controller = TrackController()
    detector = CornerDetector()
    events = 0
    rows: list[dict[str, int | str]] = []

    for tick, pattern in enumerate(make_course_trace(laps), start=1):
        control = controller.step(pattern)
        event = detector.step(pattern)
        events += int(event)
        rows.append(
            {
                "tick": tick,
                "pattern_hex": f"0x{pattern:02X}",
                "error": control.error,
                "left_pwm": control.left_pwm,
                "right_pwm": control.right_pwm,
                "mode": control.mode,
                "corner_event": int(event),
                "completed_laps": events // 4,
            }
        )

    elapsed_ticks = len(rows)
    return (
        CourseResult(laps, events // 4, events, elapsed_ticks, elapsed_ticks * 0.01),
        rows,
    )


def verify_firmware_markers(workspace: Path) -> None:
    tracker_source = (workspace / "car_final_ccs/user/xunji.c").read_text(encoding="utf-8")
    main_source = (workspace / "car_final_ccs/user/main.c").read_text(encoding="utf-8")
    diag_source = (workspace / "car_final_ccs/user/track_diag.c").read_text(encoding="utf-8")
    encoder_source = (workspace / "car_final_ccs/user/encoder.c").read_text(encoding="utf-8")
    encoder_diag_source = (workspace / "car_final_ccs/user/encoder_diag.c").read_text(encoding="utf-8")
    motor_diag_source = (workspace / "car_final_ccs/user/motor_diag.c").read_text(encoding="utf-8")
    motor_source = (workspace / "car_final_ccs/ml_libs/ml_motor.c").read_text(encoding="utf-8")
    app_config = (workspace / "car_final_ccs/user/app_config.h").read_text(encoding="utf-8")
    projectspec = (workspace / "car_final_ccs/car_final_ccs.projectspec").read_text(encoding="utf-8")
    assert "return digtal(channel) ? 0U : 1U;" in tracker_source
    assert "filter->active_ticks < 2U" in tracker_source
    assert "filter->clear_ticks < 5U" in tracker_source
    assert "track_corner_filter_step(&corner_filter, pattern)" in main_source
    assert "#define APP_PROFILE_TRACK_DIAG 0" in app_config
    assert "motor_safe_init_disabled();" in diag_source
    assert "motor_set_standby(0U);" in diag_source
    assert diag_source.count("Set_left_pwm(0);") >= 2
    assert diag_source.count("Set_right_pwm(0);") >= 2
    assert "user/track_diag.c" in projectspec
    assert "static volatile int32_t Encoder_count1" in encoder_source
    assert "encoder_read_and_reset" in encoder_source
    assert "__disable_irq();" in encoder_source
    assert "#define ENCODER_ENABLE_LEGACY_KEY_ISR 0" in encoder_source
    assert "#define APP_PROFILE_ENCODER_DIAG 0" in app_config
    assert encoder_diag_source.count("Set_left_pwm(0);") >= 2
    assert encoder_diag_source.count("Set_right_pwm(0);") >= 2
    assert "motor_safe_init_disabled();" in encoder_diag_source
    assert "motor_set_standby(0U);" in encoder_diag_source
    assert "user/encoder_diag.c" in projectspec
    assert "void motor_safe_init_disabled(void)" in motor_source
    assert "#define APP_PROFILE_MOTOR_DIAG 0" in app_config
    assert "MOTOR_DIAG_ARM_RELEASE_TICKS  50U" in motor_diag_source
    assert "if(sw4_raw == 0U)" in motor_diag_source
    assert "motor_set_standby(1U);" in motor_diag_source
    assert "user/motor_diag.c" in projectspec


def run_tests(workspace: Path) -> dict[str, object]:
    verify_firmware_markers(workspace)

    # Raw module levels for a centered line are 11100111 (D4/D5 low).
    assert raw_levels_to_pattern(0xE7) == CENTER

    center = TrackController().step(CENTER)
    assert (center.error, center.left_pwm, center.right_pwm) == (0, BASE_PWM, BASE_PWM)

    left = TrackController().step(LEFT_OFFSET)
    right = TrackController().step(RIGHT_OFFSET)
    assert left.error < 0 and left.left_pwm < left.right_pwm
    assert right.error > 0 and right.left_pwm > right.right_pwm

    lost_controller = TrackController()
    lost_controller.step(LEFT_OFFSET)
    lost = lost_controller.step(0)
    assert (lost.left_pwm, lost.right_pwm, lost.mode) == (0, SEARCH_PWM, "SEARCH_LEFT")

    assert TrackController().step(LEFT_CORNER).mode == "CORNER_LEFT"
    assert TrackController().step(RIGHT_CORNER).mode == "CORNER_RIGHT"
    saturated = TrackController().step(0x01)
    assert 0 <= saturated.left_pwm <= PWM_LIMIT
    assert 0 <= saturated.right_pwm <= PWM_LIMIT

    noise_detector = CornerDetector()
    assert not noise_detector.step(LEFT_CORNER)
    assert not noise_detector.step(CENTER)

    bounce_detector = CornerDetector()
    bounce_trace = [CENTER] * 5 + [LEFT_CORNER] * 25 + [CENTER] + [LEFT_CORNER] * 10
    assert sum(bounce_detector.step(pattern) for pattern in bounce_trace) == 1
    assert sum(bounce_detector.step(CENTER) for _ in range(5)) == 0
    assert sum(bounce_detector.step(RIGHT_CORNER) for _ in range(2)) == 1

    one_lap, _ = replay_course(1)
    two_laps, _ = replay_course(2)
    five_laps, five_lap_rows = replay_course(5)
    assert one_lap.completed_laps == 1 and one_lap.corner_events == 4
    assert two_laps.completed_laps == 2 and two_laps.corner_events == 8
    assert five_laps.completed_laps == 5 and five_laps.corner_events == 20
    assert five_laps.elapsed_seconds < 20.0

    output_dir = workspace / "output/track_sim"
    output_dir.mkdir(parents=True, exist_ok=True)
    with (output_dir / "five_lap_replay.csv").open("w", newline="", encoding="utf-8-sig") as handle:
        writer = csv.DictWriter(handle, fieldnames=five_lap_rows[0].keys())
        writer.writeheader()
        writer.writerows(five_lap_rows)

    summary: dict[str, object] = {
        "sensor_polarity": "raw white=1, black=0; internal pattern bit 1=black",
        "tick_ms": 10,
        "controller_cases": {
            "center": asdict(center),
            "left_offset": asdict(left),
            "right_offset": asdict(right),
            "lost_after_left": asdict(lost),
        },
        "corner_filter": {"qualify_ms": 20, "clear_to_rearm_ms": 50},
        "one_lap": asdict(one_lap),
        "two_laps": asdict(two_laps),
        "five_laps": asdict(five_laps),
        "checks": {
            "active_low_conversion": "PASS",
            "steering_direction": "PASS",
            "lost_line_search": "PASS",
            "single_tick_corner_noise_rejected": "PASS",
            "long_corner_bounce_not_recounted": "PASS",
            "five_lap_synthetic_trace_under_20_seconds": "PASS",
            "firmware_markers_match_model": "PASS",
            "diagnostic_profile_safe_default": "PASS",
            "encoder_atomic_read_and_safe_diag": "PASS",
        },
    }
    (output_dir / "summary.json").write_text(
        json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    return summary


def main() -> None:
    workspace = Path(__file__).resolve().parents[1]
    summary = run_tests(workspace)
    print(json.dumps(summary, indent=2, ensure_ascii=False))
    print("E2025 track replay validation: PASS")


if __name__ == "__main__":
    main()
