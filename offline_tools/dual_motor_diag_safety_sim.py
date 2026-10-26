#!/usr/bin/env python3
"""Offline safety replay for the suspended dual-wheel diagnostic profile."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path


PWM = 600
ARM_RELEASE_TICKS = 50
DEBOUNCE_TICKS = 3
SAMPLE_TICKS = 10
GRACE_SAMPLES = 5
NO_FEEDBACK_SAMPLES = 10
SIDE_MISSING_SAMPLES = 3
DIRECTION_FAULT_SAMPLES = 3
MISMATCH_SAMPLES = 5
ACTIVITY_MIN = 2
MISMATCH_MIN = 4
MISMATCH_PERCENT = 50

NONE = "NONE"
ESTOP = "E-STOP"
NO_FEEDBACK = "NO FEEDBACK"
LEFT_MISSING = "LEFT MISSING"
RIGHT_MISSING = "RIGHT MISSING"
SPEED_MISMATCH = "SPEED DIFF"
LEFT_DIRECTION = "LEFT DIR"
RIGHT_DIRECTION = "RIGHT DIR"


@dataclass
class Button:
    stable_level: int
    candidate_level: int
    candidate_ticks: int = DEBOUNCE_TICKS
    pressed_event: bool = False

    def update(self, raw_level: int) -> None:
        self.pressed_event = False
        if raw_level != self.candidate_level:
            self.candidate_level = raw_level
            self.candidate_ticks = 1
            return
        if self.candidate_ticks < DEBOUNCE_TICKS:
            self.candidate_ticks += 1
        if (
            self.candidate_ticks >= DEBOUNCE_TICKS
            and self.stable_level != self.candidate_level
        ):
            self.stable_level = self.candidate_level
            self.pressed_event = self.stable_level == 0


@dataclass
class Monitor:
    run_samples: int = 0
    no_feedback: int = 0
    left_missing: int = 0
    right_missing: int = 0
    mismatch: int = 0
    left_direction: int = 0
    right_direction: int = 0

    def reset(self) -> None:
        self.__dict__.update(Monitor().__dict__)

    @staticmethod
    def count_if(value: int, condition: bool) -> int:
        return min(value + 1, 255) if condition else 0

    def step(self, reverse: bool, left: int, right: int) -> str:
        return self.step_targets(reverse, left, right, 1, 1)

    def step_targets(
        self,
        reverse: bool,
        left: int,
        right: int,
        left_target: int,
        right_target: int,
    ) -> str:
        left_abs = abs(left)
        right_abs = abs(right)
        left_scaled = left_abs * right_target
        right_scaled = right_abs * left_target
        maximum = max(left_scaled, right_scaled)
        difference = abs(left_scaled - right_scaled)
        left_commanded = left_target > 0
        right_commanded = right_target > 0
        left_active = left_abs >= ACTIVITY_MIN
        right_active = right_abs >= ACTIVITY_MIN
        left_wrong = left_commanded and left_active and (
            (not reverse and left < 0) or (reverse and left > 0)
        )
        right_wrong = right_commanded and right_active and (
            (not reverse and right < 0) or (reverse and right > 0)
        )
        both_commanded = left_commanded and right_commanded
        mismatch = (
            both_commanded
            and left_active
            and right_active
            and max(left_abs, right_abs) >= MISMATCH_MIN
            and difference * 100 > maximum * MISMATCH_PERCENT
        )

        if self.run_samples < GRACE_SAMPLES:
            self.run_samples += 1
            return NONE

        self.no_feedback = self.count_if(
            self.no_feedback,
            both_commanded and not left_active and not right_active,
        )
        self.left_missing = self.count_if(
            self.left_missing,
            left_commanded
            and not left_active
            and ((not right_commanded) or right_active),
        )
        self.right_missing = self.count_if(
            self.right_missing,
            right_commanded
            and not right_active
            and ((not left_commanded) or left_active),
        )
        self.left_direction = self.count_if(self.left_direction, left_wrong)
        self.right_direction = self.count_if(self.right_direction, right_wrong)
        self.mismatch = self.count_if(self.mismatch, mismatch)

        if self.no_feedback >= NO_FEEDBACK_SAMPLES:
            return NO_FEEDBACK
        if self.left_missing >= SIDE_MISSING_SAMPLES:
            return LEFT_MISSING
        if self.right_missing >= SIDE_MISSING_SAMPLES:
            return RIGHT_MISSING
        if self.left_direction >= DIRECTION_FAULT_SAMPLES:
            return LEFT_DIRECTION
        if self.right_direction >= DIRECTION_FAULT_SAMPLES:
            return RIGHT_DIRECTION
        if self.mismatch >= MISMATCH_SAMPLES:
            return SPEED_MISMATCH
        return NONE


@dataclass
class DualMotorDiag:
    sw2_initial_pressed: bool = False
    sw2: Button = field(init=False)
    sw3: Button = field(init=False)
    monitor: Monitor = field(default_factory=Monitor)
    fault: str = NONE
    reverse: bool = False
    armed: bool = False
    release_ticks: int = 0
    sample_ticks: int = 0
    previous_running: bool = False
    motor_was_running: bool = False
    running: bool = False
    standby: bool = False
    left_pwm: int = 0
    right_pwm: int = 0
    left_delta: int = 0
    right_delta: int = 0

    def __post_init__(self) -> None:
        sw2_level = 0 if self.sw2_initial_pressed else 1
        self.sw2 = Button(sw2_level, sw2_level)
        self.sw3 = Button(1, 1)

    def apply(self, run: bool) -> None:
        if not run:
            self.standby = False
            self.left_pwm = 0
            self.right_pwm = 0
            self.motor_was_running = False
            return
        if self.motor_was_running:
            return
        self.standby = False
        self.left_pwm = 0
        self.right_pwm = 0
        command = -PWM if self.reverse else PWM
        self.left_pwm = command
        self.right_pwm = command
        self.standby = True
        self.motor_was_running = True

    def tick(
        self,
        *,
        sw2_pressed: bool = False,
        sw3_pressed: bool = False,
        sw4_pressed: bool = False,
        left_delta: int = 10,
        right_delta: int = 10,
    ) -> None:
        sw2_raw = 0 if sw2_pressed else 1
        sw3_raw = 0 if sw3_pressed else 1

        if sw4_pressed and self.fault == NONE:
            self.fault = ESTOP

        self.sw2.update(sw2_raw)
        self.sw3.update(sw3_raw)

        if self.fault == NONE and self.sw2.stable_level != 0:
            self.release_ticks = min(self.release_ticks + 1, ARM_RELEASE_TICKS)
            if self.release_ticks >= ARM_RELEASE_TICKS:
                self.armed = True
        elif not self.armed:
            self.release_ticks = 0

        requested_run = (
            self.armed
            and self.fault == NONE
            and self.sw2.stable_level == 0
            and sw2_raw == 0
        )
        if not requested_run and self.fault == NONE and self.sw3.pressed_event:
            self.reverse = not self.reverse

        sample_due = self.sample_ticks == 0
        if requested_run and not self.previous_running:
            self.left_delta = 0
            self.right_delta = 0
            self.monitor.reset()
        elif sample_due:
            self.left_delta = left_delta
            self.right_delta = right_delta
            if requested_run:
                detected = self.monitor.step(
                    self.reverse, self.left_delta, self.right_delta
                )
                if detected != NONE:
                    self.fault = detected
            else:
                self.monitor.reset()

        self.running = requested_run and self.fault == NONE
        self.apply(self.running)
        self.previous_running = self.running
        self.sample_ticks = (self.sample_ticks + 1) % SAMPLE_TICKS


def repeat(model: DualMotorDiag, count: int, **kwargs: object) -> None:
    for _ in range(count):
        model.tick(**kwargs)


def arm(model: DualMotorDiag) -> None:
    repeat(model, 60)
    assert model.armed
    assert not model.running
    assert not model.standby


def start(model: DualMotorDiag, *, left: int = 10, right: int = 10) -> None:
    model.tick(sw2_pressed=True, left_delta=left, right_delta=right)
    assert not model.running
    model.tick(sw2_pressed=True, left_delta=left, right_delta=right)
    assert not model.running
    model.tick(sw2_pressed=True, left_delta=left, right_delta=right)
    assert model.running
    assert model.standby
    assert model.left_pwm == model.right_pwm == (-PWM if model.reverse else PWM)


def run_fault_case(expected: str, left: int, right: int, ticks: int = 220) -> None:
    model = DualMotorDiag()
    arm(model)
    start(model, left=left, right=right)
    repeat(
        model,
        ticks,
        sw2_pressed=True,
        left_delta=left,
        right_delta=right,
    )
    assert model.fault == expected, (expected, model.fault)
    assert not model.running
    assert not model.standby
    assert model.left_pwm == model.right_pwm == 0


def source_checks(workspace: Path) -> None:
    source = (workspace / "car_final_ccs/user/dual_motor_diag.c").read_text(
        encoding="utf-8"
    )
    guard_source = (
        workspace / "car_final_ccs/user/dual_motor_guard.c"
    ).read_text(encoding="utf-8")
    config = (workspace / "car_final_ccs/user/app_config.h").read_text(
        encoding="utf-8"
    )
    main = (workspace / "car_final_ccs/user/main.c").read_text(encoding="utf-8")
    projectspec = (workspace / "car_final_ccs/car_final_ccs.projectspec").read_text(
        encoding="utf-8"
    )

    assert "motor_safe_init_disabled();" in source
    assert "motor_set_standby(0U);" in source
    assert "motor_set_standby(1U);" in source
    assert source.count("Set_left_pwm(0);") >= 1
    assert source.count("Set_right_pwm(0);") >= 1
    assert "encoder_read_and_reset" in source
    assert "dual_motor_guard_step" in source
    assert "DUAL_MOTOR_NO_FEEDBACK_SAMPLES" in guard_source
    assert "DUAL_MOTOR_MISMATCH_PERCENT" in guard_source
    assert "#define APP_PROFILE_DUAL_MOTOR_DIAG 0" in config
    assert "#define APP_PROFILE_E2025      1" in config
    assert "dual_motor_diag_run();" in main
    assert "user/dual_motor_diag.c" in projectspec
    assert "user/dual_motor_guard.c" in projectspec


def main() -> None:
    workspace = Path(__file__).resolve().parents[1]
    results: list[dict[str, str]] = []

    held = DualMotorDiag(sw2_initial_pressed=True)
    repeat(held, 200, sw2_pressed=True)
    assert not held.armed and not held.running and not held.standby
    results.append({"case": "boot_held_sw2_never_starts", "status": "PASS"})

    model = DualMotorDiag()
    arm(model)
    results.append({"case": "released_500ms_arms", "status": "PASS"})

    start(model)
    repeat(model, 220, sw2_pressed=True, left_delta=11, right_delta=10)
    assert model.fault == NONE and model.running and model.standby
    results.append({"case": "matched_feedback_keeps_running", "status": "PASS"})

    model.tick(sw2_pressed=False, left_delta=11, right_delta=10)
    assert not model.running and not model.standby
    assert model.left_pwm == model.right_pwm == 0
    results.append({"case": "raw_sw2_release_stops_same_tick", "status": "PASS"})

    run_fault_case(NO_FEEDBACK, 0, 0)
    results.append({"case": "both_encoders_missing_latches", "status": "PASS"})
    run_fault_case(LEFT_MISSING, 0, 10)
    results.append({"case": "left_encoder_missing_latches", "status": "PASS"})
    run_fault_case(RIGHT_MISSING, 10, 0)
    results.append({"case": "right_encoder_missing_latches", "status": "PASS"})
    run_fault_case(SPEED_MISMATCH, 3, 10)
    results.append({"case": "sustained_speed_mismatch_latches", "status": "PASS"})
    run_fault_case(LEFT_DIRECTION, -10, 10)
    results.append({"case": "left_wrong_direction_latches", "status": "PASS"})
    run_fault_case(RIGHT_DIRECTION, 10, -10)
    results.append({"case": "right_wrong_direction_latches", "status": "PASS"})

    reverse = DualMotorDiag()
    arm(reverse)
    repeat(reverse, 3, sw3_pressed=True)
    assert reverse.reverse
    start(reverse, left=-10, right=-9)
    repeat(reverse, 120, sw2_pressed=True, left_delta=-10, right_delta=-9)
    assert reverse.fault == NONE and reverse.running
    results.append({"case": "reverse_negative_feedback_is_valid", "status": "PASS"})

    estop = DualMotorDiag()
    arm(estop)
    start(estop)
    estop.tick(sw2_pressed=True, sw4_pressed=True, left_delta=10, right_delta=10)
    assert estop.fault == ESTOP
    assert not estop.running and not estop.standby
    assert estop.left_pwm == estop.right_pwm == 0
    repeat(estop, 20, sw2_pressed=True, left_delta=10, right_delta=10)
    assert estop.fault == ESTOP and not estop.running
    results.append({"case": "sw4_same_tick_latched_stop", "status": "PASS"})

    source_checks(workspace)
    results.append({"case": "firmware_source_safety_markers", "status": "PASS"})

    output = workspace / "output/dual_motor_diag_sim"
    output.mkdir(parents=True, exist_ok=True)
    summary = {
        "suite": "dual_motor_diag_safety",
        "tick_ms": 10,
        "sample_ms": 100,
        "pwm": PWM,
        "hardware_verified": False,
        "cases": results,
    }
    (output / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )

    for result in results:
        print(f"PASS {result['case']}")
    print(f"PASS summary={output / 'summary.json'}")


if __name__ == "__main__":
    main()
