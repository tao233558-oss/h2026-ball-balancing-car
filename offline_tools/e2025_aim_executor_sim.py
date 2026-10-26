#!/usr/bin/env python3
"""Offline timing and safety replay for the hardware-neutral aim executor."""

from __future__ import annotations

import csv
import json
from dataclasses import asdict, dataclass
from enum import IntEnum
from pathlib import Path


TICK_US = 100
DIR_SETUP_TICKS = 2
STEP_HIGH_TICKS = 1
STEP_LOW_TICKS = 1
QUEUE_LIMIT = 16


class Phase(IntEnum):
    IDLE = 0
    DIR_SETUP = 1
    STEP_HIGH = 2
    STEP_LOW = 3


@dataclass
class Safety:
    emergency_stop: bool = False
    pan_negative_limit: bool = False
    pan_positive_limit: bool = False
    tilt_negative_limit: bool = False
    tilt_positive_limit: bool = False


@dataclass
class Axis:
    pending_steps: int = 0
    active_direction: int = 0
    phase_ticks: int = 0
    step_active: bool = False
    phase: Phase = Phase.IDLE
    emitted_position_steps: int = 0


@dataclass
class Output:
    busy: bool
    driver_awake_allowed: bool
    laser_allowed: bool
    pan_step_active: bool
    tilt_step_active: bool
    pan_limit_blocked: bool
    tilt_limit_blocked: bool
    queue_overflow: bool
    reversal_deferred: bool
    pan_direction: int
    tilt_direction: int
    pan_pending_steps: int
    tilt_pending_steps: int
    pan_emitted_position_steps: int
    tilt_emitted_position_steps: int
    fault: str


def direction(steps: int) -> int:
    return 1 if steps > 0 else (-1 if steps < 0 else 0)


class Executor:
    def __init__(self) -> None:
        self.pan = Axis()
        self.tilt = Axis()
        self.enabled = False
        self.laser_requested = False
        self.queue_overflow = False
        self.reversal_deferred = False
        self.fault = "NONE"

    @staticmethod
    def stop_axis(axis: Axis) -> None:
        axis.pending_steps = 0
        axis.active_direction = 0
        axis.phase_ticks = 0
        axis.step_active = False
        axis.phase = Phase.IDLE

    def abort(self) -> None:
        self.stop_axis(self.pan)
        self.stop_axis(self.tilt)
        self.enabled = False
        self.laser_requested = False

    def enqueue(self, axis: Axis, requested_steps: int) -> None:
        if requested_steps == 0:
            return
        requested_direction = direction(requested_steps)
        pending_direction = direction(axis.pending_steps)
        if (axis.phase != Phase.IDLE
                and axis.active_direction != requested_direction):
            self.reversal_deferred = True
            return
        if (axis.phase == Phase.IDLE and pending_direction != 0
                and pending_direction != requested_direction):
            axis.pending_steps = 0
        candidate = axis.pending_steps + requested_steps
        if candidate > QUEUE_LIMIT:
            candidate = QUEUE_LIMIT
            self.queue_overflow = True
        elif candidate < -QUEUE_LIMIT:
            candidate = -QUEUE_LIMIT
            self.queue_overflow = True
        axis.pending_steps = candidate

    def submit(self, enable: bool, laser_request: bool,
               pan_steps: int, tilt_steps: int) -> None:
        self.queue_overflow = False
        self.reversal_deferred = False
        if not enable or self.fault != "NONE":
            self.abort()
            return
        self.enabled = True
        self.laser_requested = laser_request
        self.enqueue(self.pan, pan_steps)
        self.enqueue(self.tilt, tilt_steps)

    @staticmethod
    def limit_blocked(axis: Axis, negative: bool, positive: bool) -> bool:
        moving_direction = axis.active_direction or direction(axis.pending_steps)
        return ((moving_direction < 0 and negative)
                or (moving_direction > 0 and positive))

    @staticmethod
    def tick_axis(axis: Axis) -> None:
        if axis.phase == Phase.IDLE:
            axis.step_active = False
            if axis.pending_steps:
                axis.active_direction = direction(axis.pending_steps)
                axis.phase = Phase.DIR_SETUP
                axis.phase_ticks = DIR_SETUP_TICKS
        elif axis.phase == Phase.DIR_SETUP:
            axis.step_active = False
            if axis.phase_ticks > 1:
                axis.phase_ticks -= 1
            else:
                axis.phase = Phase.STEP_HIGH
                axis.phase_ticks = STEP_HIGH_TICKS
                axis.step_active = True
        elif axis.phase == Phase.STEP_HIGH:
            if axis.phase_ticks > 1:
                axis.phase_ticks -= 1
            else:
                axis.phase = Phase.STEP_LOW
                axis.phase_ticks = STEP_LOW_TICKS
                axis.step_active = False
        elif axis.phase == Phase.STEP_LOW:
            axis.step_active = False
            if axis.phase_ticks > 1:
                axis.phase_ticks -= 1
            else:
                axis.pending_steps -= axis.active_direction
                axis.emitted_position_steps += axis.active_direction
                if axis.pending_steps:
                    axis.phase = Phase.STEP_HIGH
                    axis.phase_ticks = STEP_HIGH_TICKS
                    axis.step_active = True
                else:
                    axis.active_direction = 0
                    axis.phase_ticks = 0
                    axis.phase = Phase.IDLE
        else:
            Executor.stop_axis(axis)

    def tick(self, safety: Safety) -> Output:
        pan_blocked = False
        tilt_blocked = False
        driver_awake_allowed = False
        laser_allowed = False
        if safety.emergency_stop:
            self.fault = "ESTOP"
            self.abort()

        if self.enabled and self.fault == "NONE":
            driver_awake_allowed = True
            pan_blocked = self.limit_blocked(
                self.pan, safety.pan_negative_limit,
                safety.pan_positive_limit)
            tilt_blocked = self.limit_blocked(
                self.tilt, safety.tilt_negative_limit,
                safety.tilt_positive_limit)
            if pan_blocked:
                self.stop_axis(self.pan)
            else:
                self.tick_axis(self.pan)
            if tilt_blocked:
                self.stop_axis(self.tilt)
            else:
                self.tick_axis(self.tilt)

            any_limit = any((
                safety.pan_negative_limit, safety.pan_positive_limit,
                safety.tilt_negative_limit, safety.tilt_positive_limit))
            laser_allowed = (self.laser_requested and not any_limit
                             and not self.queue_overflow
                             and not self.reversal_deferred)
        else:
            self.stop_axis(self.pan)
            self.stop_axis(self.tilt)

        return Output(
            busy=(self.pan.phase != Phase.IDLE or self.pan.pending_steps != 0
                  or self.tilt.phase != Phase.IDLE
                  or self.tilt.pending_steps != 0),
            driver_awake_allowed=driver_awake_allowed,
            laser_allowed=laser_allowed,
            pan_step_active=self.pan.step_active,
            tilt_step_active=self.tilt.step_active,
            pan_limit_blocked=pan_blocked,
            tilt_limit_blocked=tilt_blocked,
            queue_overflow=self.queue_overflow,
            reversal_deferred=self.reversal_deferred,
            pan_direction=self.pan.active_direction,
            tilt_direction=self.tilt.active_direction,
            pan_pending_steps=self.pan.pending_steps,
            tilt_pending_steps=self.tilt.pending_steps,
            pan_emitted_position_steps=self.pan.emitted_position_steps,
            tilt_emitted_position_steps=self.tilt.emitted_position_steps,
            fault=self.fault,
        )


def drain(executor: Executor, safety: Safety, limit: int = 200) -> Output:
    output = executor.tick(safety)
    for _ in range(limit - 1):
        if not output.busy:
            return output
        output = executor.tick(safety)
    raise AssertionError("executor did not become idle")


def validate_sources(root: Path) -> list[str]:
    source = (root / "car_final_ccs/user/e2025_aim_executor.c").read_text(
        encoding="utf-8")
    header = (root / "car_final_ccs/user/e2025_aim_executor.h").read_text(
        encoding="utf-8")
    spec = (root / "car_final_ccs/car_final_ccs.projectspec").read_text(
        encoding="utf-8")
    main = (root / "car_final_ccs/user/main.c").read_text(encoding="utf-8")
    config = (root / "car_final_ccs/user/app_config.h").read_text(
        encoding="utf-8")

    for forbidden in ("DL_GPIO", "gpio_", "GPIOA", "GPIOB", "PA14", "PA26"):
        assert forbidden not in source
        assert forbidden not in header
    assert "E2025_AIM_EXECUTOR_TICK_US 100U" in header
    assert "E2025_AIM_DIR_SETUP_TICKS  2U" in header
    assert "user/e2025_aim_executor.c" in spec
    assert "e2025_aim_executor_tick(" not in main
    assert "#define E2025_AIM_PHYSICAL_LIMITS_PRESENT 0" in config
    return [
        "executor_has_no_gpio_or_pin_binding",
        "projectspec_includes_executor_source",
        "runtime_backend_intentionally_unbound",
        "physical_limit_switches_confirmed_absent",
    ]


def run(root: Path) -> tuple[dict[str, object], list[dict[str, object]]]:
    checks = validate_sources(root)
    waveform: list[dict[str, object]] = []

    executor = Executor()
    executor.submit(True, True, 2, 0)
    outputs = []
    for tick in range(1, 8):
        output = executor.tick(Safety())
        outputs.append(output)
        waveform.append({"tick": tick, **asdict(output)})
    assert [int(item.pan_step_active) for item in outputs] == [0, 0, 1, 0, 1, 0, 0]
    assert outputs[-1].pan_emitted_position_steps == 2
    assert outputs[-1].busy is False
    assert all(item.driver_awake_allowed for item in outputs)
    assert all(item.laser_allowed for item in outputs)
    checks.append("two_step_waveform_and_200us_dir_setup")

    dual = Executor()
    dual.submit(True, False, 3, -2)
    dual_out = drain(dual, Safety())
    assert dual_out.pan_emitted_position_steps == 3
    assert dual_out.tilt_emitted_position_steps == -2
    checks.append("independent_signed_dual_axis_motion")

    limited = Executor()
    limited.submit(True, True, 2, 0)
    limit_safety = Safety(pan_positive_limit=True)
    limited_out = limited.tick(limit_safety)
    assert limited_out.pan_limit_blocked
    assert limited_out.pan_pending_steps == 0
    assert not limited_out.laser_allowed
    limited.submit(True, True, -1, 0)
    limited_out = drain(limited, limit_safety)
    assert limited_out.pan_emitted_position_steps == -1
    assert not limited_out.laser_allowed
    checks.append("directional_limit_block_and_retreat")

    estop = Executor()
    estop.submit(True, True, 4, 4)
    estop_out = estop.tick(Safety(emergency_stop=True))
    assert estop_out.fault == "ESTOP"
    assert (not estop_out.busy and not estop_out.driver_awake_allowed
            and not estop_out.laser_allowed)
    estop.submit(True, True, 1, 1)
    estop_out = estop.tick(Safety())
    assert estop_out.fault == "ESTOP" and not estop_out.busy
    checks.append("latched_estop_fail_closed")

    reverse = Executor()
    reverse.submit(True, True, 2, 0)
    for _ in range(3):
        reverse.tick(Safety())
    assert reverse.pan.step_active
    reverse.submit(True, True, -1, 0)
    reverse_out = reverse.tick(Safety())
    assert reverse_out.reversal_deferred
    assert not reverse_out.laser_allowed
    assert reverse_out.pan_pending_steps == 2
    checks.append("direction_change_deferred_during_active_pulse")

    overflow = Executor()
    overflow.submit(True, True, 100, -100)
    overflow_out = overflow.tick(Safety())
    assert overflow_out.queue_overflow
    assert overflow_out.pan_pending_steps == QUEUE_LIMIT
    assert overflow_out.tilt_pending_steps == -QUEUE_LIMIT
    assert not overflow_out.laser_allowed
    checks.append("bounded_queue_and_laser_interlock")

    disabled = Executor()
    disabled.submit(True, True, 4, 4)
    disabled.tick(Safety())
    disabled.submit(False, True, 4, 4)
    disabled_out = disabled.tick(Safety())
    assert (not disabled_out.busy and not disabled_out.driver_awake_allowed
            and not disabled_out.laser_allowed)
    checks.append("disable_aborts_motion_driver_awake_and_laser")

    summary = {
        "suite": "e2025_aim_executor",
        "status": "PASS",
        "tick_us": TICK_US,
        "dir_setup_us": TICK_US * DIR_SETUP_TICKS,
        "step_high_us": TICK_US * STEP_HIGH_TICKS,
        "step_low_us": TICK_US * STEP_LOW_TICKS,
        "queue_limit_steps_per_axis": QUEUE_LIMIT,
        "checks": checks,
        "hardware_gpio_bound": False,
        "physical_limit_switches_present": False,
        "position_reference": "power_on_relative_zero_only",
        "binding_blockers": [
            "latest_pcb_confirmation_for_recommended_pb4_pb5_pb17",
            "laser_enable_pin_and_active_polarity",
            "100us_timer_irq_selection",
        ],
        "accepted_risks": [
            "no_automatic_homing_or_absolute_mechanical_reference",
            "relative_soft_limits_can_drift_after_missed_steps_or_obstruction",
        ],
    }
    return summary, waveform


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    summary, waveform = run(root)
    output_dir = root / "output/e2025_aim_executor_sim"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8")
    with (output_dir / "two_step_waveform.csv").open(
            "w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=waveform[0].keys())
        writer.writeheader()
        writer.writerows(waveform)
    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
