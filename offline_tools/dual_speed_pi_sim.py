#!/usr/bin/env python3
"""Closed-loop replay for the unvalidated suspended dual-speed PI profile."""

from __future__ import annotations

import csv
import json
from dataclasses import dataclass, field
from pathlib import Path

from dual_motor_diag_safety_sim import (
    ARM_RELEASE_TICKS,
    ESTOP,
    NONE,
    Button,
    Monitor,
)


TARGET = 10
FEEDFORWARD = 600
PWM_MIN = 400
PWM_MAX = 900
KP = 18
KI = 6
INTEGRATOR_LIMIT = 100
SAMPLE_TICKS = 10


@dataclass
class IntegerPI:
    integrator: int = 0
    output: int = FEEDFORWARD

    def reset(self) -> None:
        self.integrator = 0
        self.output = FEEDFORWARD

    def step(self, target: int, measured: int) -> int:
        if target <= 0:
            self.integrator = 0
            self.output = 0
            return 0
        measured = max(measured, 0)
        error = target - measured
        candidate = max(
            -INTEGRATOR_LIMIT,
            min(INTEGRATOR_LIMIT, self.integrator + error),
        )
        raw = FEEDFORWARD + KP * error + KI * candidate
        if (raw > PWM_MAX and error > 0) or (raw < PWM_MIN and error < 0):
            raw = FEEDFORWARD + KP * error + KI * self.integrator
        else:
            self.integrator = candidate
        self.output = max(PWM_MIN, min(PWM_MAX, raw))
        return self.output


@dataclass
class MotorPlant:
    left_gain: float = 0.0180
    right_gain: float = 0.0155
    response: float = 0.35
    left_speed: float = 0.0
    right_speed: float = 0.0

    def step(
        self,
        left_pwm: int,
        right_pwm: int,
        *,
        right_load: float = 1.0,
    ) -> tuple[int, int]:
        left_target = self.left_gain * left_pwm
        right_target = self.right_gain * right_pwm * right_load
        self.left_speed += self.response * (left_target - self.left_speed)
        self.right_speed += self.response * (right_target - self.right_speed)
        return round(self.left_speed), round(self.right_speed)


@dataclass
class SpeedDiagModel:
    sw2: Button = field(default_factory=lambda: Button(1, 1))
    sw3: Button = field(default_factory=lambda: Button(1, 1))
    guard: Monitor = field(default_factory=Monitor)
    left_pi: IntegerPI = field(default_factory=IntegerPI)
    right_pi: IntegerPI = field(default_factory=IntegerPI)
    plant: MotorPlant = field(default_factory=MotorPlant)
    fault: str = NONE
    reverse: bool = False
    armed: bool = False
    release_ticks: int = 0
    sample_ticks: int = 0
    previous_running: bool = False
    running: bool = False
    standby: bool = False
    left_pwm: int = 0
    right_pwm: int = 0
    left_delta: int = 0
    right_delta: int = 0

    def tick(
        self,
        *,
        sw2_pressed: bool = False,
        sw3_pressed: bool = False,
        sw4_pressed: bool = False,
        right_load: float = 1.0,
        measurement_override: tuple[int, int] | None = None,
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

        requested = (
            self.armed
            and self.fault == NONE
            and self.sw2.stable_level == 0
            and sw2_raw == 0
        )
        if not requested and self.fault == NONE and self.sw3.pressed_event:
            self.reverse = not self.reverse

        sample_due = self.sample_ticks == 0
        if requested and not self.previous_running:
            self.left_delta = 0
            self.right_delta = 0
            self.left_pwm = FEEDFORWARD
            self.right_pwm = FEEDFORWARD
            self.guard.reset()
            self.left_pi.reset()
            self.right_pi.reset()
        elif sample_due and requested:
            measured = (
                measurement_override
                if measurement_override is not None
                else self.plant.step(
                    self.left_pwm, self.right_pwm, right_load=right_load
                )
            )
            direction = -1 if self.reverse else 1
            self.left_delta = direction * measured[0]
            self.right_delta = direction * measured[1]
            detected = self.guard.step(
                self.reverse, self.left_delta, self.right_delta
            )
            if detected != NONE:
                self.fault = detected
            else:
                self.left_pwm = self.left_pi.step(TARGET, abs(self.left_delta))
                self.right_pwm = self.right_pi.step(TARGET, abs(self.right_delta))
        elif sample_due:
            self.guard.reset()
            self.left_pi.reset()
            self.right_pi.reset()

        self.running = requested and self.fault == NONE
        if self.running:
            self.standby = True
        else:
            self.standby = False
            self.left_pwm = 0
            self.right_pwm = 0
        self.previous_running = self.running
        self.sample_ticks = (self.sample_ticks + 1) % SAMPLE_TICKS


def repeat(model: SpeedDiagModel, ticks: int, **kwargs: object) -> None:
    for _ in range(ticks):
        model.tick(**kwargs)


def arm_and_start(model: SpeedDiagModel) -> None:
    repeat(model, 60)
    assert model.armed and not model.running
    model.tick(sw2_pressed=True)
    assert not model.running
    model.tick(sw2_pressed=True)
    assert not model.running
    model.tick(sw2_pressed=True)
    assert model.running and model.standby
    assert model.left_pwm == model.right_pwm == FEEDFORWARD


def source_checks(workspace: Path) -> None:
    source = (workspace / "car_final_ccs/user/dual_speed_diag.c").read_text(
        encoding="utf-8"
    )
    pi_source = (workspace / "car_final_ccs/user/wheel_speed_pi.c").read_text(
        encoding="utf-8"
    )
    config = (workspace / "car_final_ccs/user/app_config.h").read_text(
        encoding="utf-8"
    )
    main_source = (workspace / "car_final_ccs/user/main.c").read_text(
        encoding="utf-8"
    )
    projectspec = (workspace / "car_final_ccs/car_final_ccs.projectspec").read_text(
        encoding="utf-8"
    )

    assert "motor_safe_init_disabled();" in source
    assert "motor_set_standby(0U);" in source
    assert "motor_set_standby(1U);" in source
    assert "dual_motor_guard_step" in source
    assert "wheel_speed_pi_step" in source
    assert "encoder_read_and_reset" in source
    assert "DUAL_SPEED_MAXIMUM_PWM         900" in source
    assert "Conditional integration prevents wind-up" in pi_source
    assert "#define APP_PROFILE_DUAL_SPEED_DIAG 0" in config
    assert "#define APP_PROFILE_E2025      1" in config
    assert "dual_speed_diag_run();" in main_source
    assert "user/dual_speed_diag.c" in projectspec
    assert "user/wheel_speed_pi.c" in projectspec


def main() -> None:
    workspace = Path(__file__).resolve().parents[1]
    results: list[dict[str, object]] = []

    pi = IntegerPI()
    for _ in range(100):
        assert pi.step(TARGET, 0) <= PWM_MAX
    assert pi.output == PWM_MAX
    assert abs(pi.integrator) <= INTEGRATOR_LIMIT
    assert pi.step(0, 0) == 0 and pi.integrator == 0
    results.append({"case": "pi_clamp_anti_windup_and_reset", "status": "PASS"})

    model = SpeedDiagModel()
    arm_and_start(model)
    rows: list[dict[str, object]] = []
    for tick in range(500):
        model.tick(sw2_pressed=True)
        if model.sample_ticks == 1:
            rows.append(
                {
                    "sample": len(rows),
                    "phase": "asymmetric",
                    "left_counts": model.left_delta,
                    "right_counts": model.right_delta,
                    "left_pwm": model.left_pwm,
                    "right_pwm": model.right_pwm,
                    "fault": model.fault,
                }
            )
    assert model.fault == NONE and model.running
    assert 9 <= abs(model.left_delta) <= 11
    assert 9 <= abs(model.right_delta) <= 11
    assert model.right_pwm > model.left_pwm
    results.append(
        {
            "case": "asymmetric_motors_converge",
            "status": "PASS",
            "left_counts": model.left_delta,
            "right_counts": model.right_delta,
            "left_pwm": model.left_pwm,
            "right_pwm": model.right_pwm,
        }
    )

    right_pwm_before_load = model.right_pwm
    for _ in range(600):
        model.tick(sw2_pressed=True, right_load=0.8)
        if model.sample_ticks == 1:
            rows.append(
                {
                    "sample": len(rows),
                    "phase": "right_load_80pct",
                    "left_counts": model.left_delta,
                    "right_counts": model.right_delta,
                    "left_pwm": model.left_pwm,
                    "right_pwm": model.right_pwm,
                    "fault": model.fault,
                }
            )
    assert model.fault == NONE and model.running
    assert 9 <= abs(model.right_delta) <= 11
    assert model.right_pwm > right_pwm_before_load
    results.append(
        {
            "case": "right_load_step_recovers",
            "status": "PASS",
            "right_counts": model.right_delta,
            "right_pwm_before": right_pwm_before_load,
            "right_pwm_after": model.right_pwm,
        }
    )

    model.tick(sw2_pressed=False)
    assert not model.running and not model.standby
    assert model.left_pwm == model.right_pwm == 0
    results.append({"case": "raw_sw2_release_stops_same_tick", "status": "PASS"})

    estop = SpeedDiagModel()
    arm_and_start(estop)
    estop.tick(sw2_pressed=True, sw4_pressed=True)
    assert estop.fault == ESTOP
    assert not estop.running and not estop.standby
    assert estop.left_pwm == estop.right_pwm == 0
    repeat(estop, 20, sw2_pressed=True)
    assert estop.fault == ESTOP and not estop.running
    results.append({"case": "sw4_same_tick_latched_stop", "status": "PASS"})

    no_feedback = SpeedDiagModel()
    arm_and_start(no_feedback)
    repeat(
        no_feedback,
        220,
        sw2_pressed=True,
        measurement_override=(0, 0),
    )
    assert no_feedback.fault == "NO FEEDBACK"
    assert not no_feedback.running and not no_feedback.standby
    results.append({"case": "no_feedback_stops_closed_loop", "status": "PASS"})

    reverse = SpeedDiagModel()
    repeat(reverse, 60)
    repeat(reverse, 3, sw3_pressed=True)
    assert reverse.reverse
    arm_and_start(reverse)
    repeat(reverse, 300, sw2_pressed=True)
    assert reverse.fault == NONE and reverse.running
    assert reverse.left_delta < 0 and reverse.right_delta < 0
    results.append({"case": "reverse_feedback_sign_valid", "status": "PASS"})

    source_checks(workspace)
    results.append({"case": "firmware_source_safety_markers", "status": "PASS"})

    output = workspace / "output/dual_speed_pi_sim"
    output.mkdir(parents=True, exist_ok=True)
    with (output / "closed_loop_replay.csv").open(
        "w", newline="", encoding="utf-8-sig"
    ) as handle:
        writer = csv.DictWriter(handle, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)

    summary = {
        "suite": "dual_speed_pi",
        "sample_ms": 100,
        "target_counts_per_sample": TARGET,
        "pi": {"kp": KP, "ki": KI, "integrator_limit": INTEGRATOR_LIMIT},
        "pwm": {
            "feedforward": FEEDFORWARD,
            "minimum": PWM_MIN,
            "maximum": PWM_MAX,
        },
        "hardware_verified": False,
        "target_is_placeholder": True,
        "cases": results,
    }
    (output / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )

    for result in results:
        print(f"PASS {result['case']}")
    print(f"PASS summary={output / 'summary.json'}")
    print(f"PASS replay={output / 'closed_loop_replay.csv'}")


if __name__ == "__main__":
    main()
