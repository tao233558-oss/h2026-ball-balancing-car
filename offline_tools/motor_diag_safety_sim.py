#!/usr/bin/env python3
"""Offline safety-state replay for the hold-to-run single-wheel test."""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass
from pathlib import Path


PWM = 600
ARM_RELEASE_TICKS = 50
DEBOUNCE_TICKS = 3


@dataclass
class Button:
    stable_level: int
    candidate_level: int
    candidate_ticks: int = DEBOUNCE_TICKS
    pressed_event: bool = False

    @classmethod
    def from_raw(cls, raw: int) -> "Button":
        return cls(raw, raw)

    def update(self, raw: int) -> None:
        self.pressed_event = False
        if raw != self.candidate_level:
            self.candidate_level = raw
            self.candidate_ticks = 1
            return
        self.candidate_ticks = min(DEBOUNCE_TICKS, self.candidate_ticks + 1)
        if self.candidate_ticks >= DEBOUNCE_TICKS and self.stable_level != raw:
            self.stable_level = raw
            self.pressed_event = raw == 0


@dataclass(frozen=True)
class Output:
    armed: bool
    emergency: bool
    running: bool
    select_right: bool
    reverse: bool
    standby: int
    left_pwm: int
    right_pwm: int


class MotorDiagModel:
    def __init__(self, *, sw1: int = 1, sw2: int = 1, sw3: int = 1) -> None:
        self.sw1 = Button.from_raw(sw1)
        self.sw2 = Button.from_raw(sw2)
        self.sw3 = Button.from_raw(sw3)
        self.select_right = False
        self.reverse = False
        self.armed = False
        self.emergency = False
        self.release_ticks = 0

    def tick(self, *, sw1: int = 1, sw2: int = 1, sw3: int = 1, sw4: int = 1) -> Output:
        if sw4 == 0:
            self.emergency = True

        self.sw1.update(sw1)
        self.sw2.update(sw2)
        self.sw3.update(sw3)

        if not self.emergency and self.sw2.stable_level != 0:
            self.release_ticks = min(ARM_RELEASE_TICKS, self.release_ticks + 1)
            if self.release_ticks >= ARM_RELEASE_TICKS:
                self.armed = True
        elif not self.armed:
            self.release_ticks = 0

        # A raw high level releases the motor immediately; low must be debounced.
        running = self.armed and not self.emergency and self.sw2.stable_level == 0 and sw2 == 0
        if not running and not self.emergency and self.sw1.pressed_event:
            self.select_right = not self.select_right
        if not running and not self.emergency and self.sw3.pressed_event:
            self.reverse = not self.reverse

        command = -PWM if self.reverse else PWM
        left_pwm = command if running and not self.select_right else 0
        right_pwm = command if running and self.select_right else 0
        return Output(
            self.armed,
            self.emergency,
            running,
            self.select_right,
            self.reverse,
            int(running),
            left_pwm,
            right_pwm,
        )


def hold(model: MotorDiagModel, ticks: int, **levels: int) -> list[Output]:
    return [model.tick(**levels) for _ in range(ticks)]


def press_and_release(model: MotorDiagModel, button: str) -> None:
    levels = {button: 0}
    hold(model, DEBOUNCE_TICKS, **levels)
    hold(model, DEBOUNCE_TICKS, **{button: 1})


def verify_source(workspace: Path) -> None:
    source = (workspace / "car_final_ccs/user/motor_diag.c").read_text(encoding="utf-8")
    motor_source = (workspace / "car_final_ccs/ml_libs/ml_motor.c").read_text(encoding="utf-8")
    config = (workspace / "car_final_ccs/user/app_config.h").read_text(encoding="utf-8")
    assert "#define APP_PROFILE_MOTOR_DIAG 0" in config
    assert "motor_safe_init_disabled();" in source
    assert "if(sw4_raw == 0U)" in source
    assert "sw2_raw == 0U" in source
    assert "motor_set_standby(0U);" in source
    assert "motor_set_standby(1U);" in source
    assert "void motor_safe_init_disabled(void)" in motor_source


def run_tests(workspace: Path) -> dict[str, object]:
    verify_source(workspace)

    boot_held = MotorDiagModel(sw2=0)
    held_outputs = hold(boot_held, 100, sw2=0)
    assert all(not output.running and output.standby == 0 for output in held_outputs)

    released = hold(boot_held, 60, sw2=1)
    assert not released[ARM_RELEASE_TICKS - 1].armed
    assert released[-1].armed

    press_outputs = hold(boot_held, DEBOUNCE_TICKS, sw2=0)
    assert not press_outputs[0].running and not press_outputs[1].running
    assert press_outputs[2].running
    assert (press_outputs[2].left_pwm, press_outputs[2].right_pwm) == (PWM, 0)

    released_output = boot_held.tick(sw2=1)
    assert not released_output.running
    assert released_output.standby == 0
    assert released_output.left_pwm == 0 and released_output.right_pwm == 0

    selection = MotorDiagModel()
    hold(selection, ARM_RELEASE_TICKS)
    press_and_release(selection, "sw1")
    assert selection.select_right
    press_and_release(selection, "sw3")
    assert selection.reverse
    right_reverse = hold(selection, DEBOUNCE_TICKS, sw2=0)[-1]
    assert right_reverse.running
    assert (right_reverse.left_pwm, right_reverse.right_pwm) == (0, -PWM)

    # Direction button events are ignored while running.
    hold(selection, DEBOUNCE_TICKS, sw2=0, sw3=0)
    assert selection.reverse

    emergency_output = selection.tick(sw2=0, sw4=0)
    assert emergency_output.emergency
    assert not emergency_output.running and emergency_output.standby == 0
    assert emergency_output.left_pwm == 0 and emergency_output.right_pwm == 0
    after_release = hold(selection, 20, sw2=0, sw4=1)
    assert all(output.emergency and not output.running for output in after_release)

    summary: dict[str, object] = {
        "tick_ms": 10,
        "test_pwm": PWM,
        "arm_release_ms": ARM_RELEASE_TICKS * 10,
        "button_press_debounce_ms": DEBOUNCE_TICKS * 10,
        "checks": {
            "boot_with_run_held_never_starts": "PASS",
            "requires_500ms_release_before_arm": "PASS",
            "run_press_requires_debounce": "PASS",
            "run_release_stops_same_tick": "PASS",
            "only_selected_wheel_is_driven": "PASS",
            "direction_change_ignored_while_running": "PASS",
            "sw4_stops_same_tick_and_latches": "PASS",
            "default_profile_remains_off": "PASS",
        },
        "examples": {
            "left_forward": asdict(press_outputs[-1]),
            "right_reverse": asdict(right_reverse),
            "emergency": asdict(emergency_output),
        },
    }

    output_dir = workspace / "output/motor_diag_sim"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "summary.json").write_text(
        json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    return summary


def main() -> None:
    workspace = Path(__file__).resolve().parents[1]
    summary = run_tests(workspace)
    print(json.dumps(summary, indent=2, ensure_ascii=False))
    print("motor diagnostic safety replay: PASS")


if __name__ == "__main__":
    main()
