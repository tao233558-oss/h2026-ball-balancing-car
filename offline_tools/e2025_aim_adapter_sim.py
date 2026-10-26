#!/usr/bin/env python3
"""Offline replay of the E2025 virtual pan/tilt and laser safety adapter."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path


VISION_TIMEOUT_TICKS = 10
COMMAND_QUANTUM = 250
MAX_STEPS_PER_TICK = 4
SOFT_LIMIT_STEPS = 2000
NONE = "NONE"
ESTOP = "E-STOP"


@dataclass(frozen=True)
class AimOutput:
    active: bool = False
    vision_recent: bool = False
    laser_allowed: bool = False
    pan_limit_blocked: bool = False
    tilt_limit_blocked: bool = False
    pan_steps: int = 0
    tilt_steps: int = 0
    pan_position: int = 0
    tilt_position: int = 0
    fault: str = NONE


@dataclass
class AimAdapter:
    pan_accumulator: int = 0
    tilt_accumulator: int = 0
    pan_position: int = 0
    tilt_position: int = 0
    pan_last_direction: int = 0
    tilt_last_direction: int = 0
    vision_age: int = VISION_TIMEOUT_TICKS + 1
    was_enabled: bool = False
    fault: str = NONE

    def reset_motion(self) -> None:
        self.pan_accumulator = 0
        self.tilt_accumulator = 0
        self.pan_last_direction = 0
        self.tilt_last_direction = 0

    @staticmethod
    def axis_step(
        command: int,
        accumulator: int,
        last_direction: int,
        position: int,
    ) -> tuple[int, int, int, int, bool]:
        command = max(-1000, min(1000, command))
        if command == 0:
            return 0, 0, 0, position, False
        direction = 1 if command > 0 else -1
        if last_direction != 0 and last_direction != direction:
            accumulator = 0
        last_direction = direction

        if (position >= SOFT_LIMIT_STEPS and direction > 0) or (
            position <= -SOFT_LIMIT_STEPS and direction < 0
        ):
            return 0, 0, last_direction, position, True

        accumulator += command
        steps = int(accumulator / COMMAND_QUANTUM)
        steps = max(-MAX_STEPS_PER_TICK, min(MAX_STEPS_PER_TICK, steps))
        accumulator -= steps * COMMAND_QUANTUM
        candidate = position + steps
        blocked = False
        if candidate > SOFT_LIMIT_STEPS:
            steps = SOFT_LIMIT_STEPS - position
            accumulator = 0
            blocked = True
        elif candidate < -SOFT_LIMIT_STEPS:
            steps = -SOFT_LIMIT_STEPS - position
            accumulator = 0
            blocked = True
        position += steps
        return steps, accumulator, last_direction, position, blocked

    def tick(
        self,
        *,
        aim_enable: bool,
        emergency_stop: bool = False,
        vision_fresh: bool = False,
        target_valid: bool = False,
        spot_valid: bool = False,
        laser_request: bool = False,
        pan_command: int = 0,
        tilt_command: int = 0,
    ) -> AimOutput:
        if aim_enable and emergency_stop and self.fault == NONE:
            self.fault = ESTOP

        if not aim_enable:
            self.reset_motion()
            self.vision_age = VISION_TIMEOUT_TICKS + 1
            self.was_enabled = False
            return AimOutput(
                pan_position=self.pan_position,
                tilt_position=self.tilt_position,
                fault=self.fault,
            )
        if self.fault != NONE:
            return AimOutput(
                pan_position=self.pan_position,
                tilt_position=self.tilt_position,
                fault=self.fault,
            )
        if not self.was_enabled:
            self.reset_motion()
            self.vision_age = VISION_TIMEOUT_TICKS + 1
            self.was_enabled = True

        self.vision_age = 0 if vision_fresh else min(255, self.vision_age + 1)
        recent = self.vision_age <= VISION_TIMEOUT_TICKS
        if not recent or not target_valid or not spot_valid:
            self.reset_motion()
            return AimOutput(
                vision_recent=recent,
                pan_position=self.pan_position,
                tilt_position=self.tilt_position,
            )

        (
            pan_steps,
            self.pan_accumulator,
            self.pan_last_direction,
            self.pan_position,
            pan_blocked,
        ) = self.axis_step(
            pan_command,
            self.pan_accumulator,
            self.pan_last_direction,
            self.pan_position,
        )
        (
            tilt_steps,
            self.tilt_accumulator,
            self.tilt_last_direction,
            self.tilt_position,
            tilt_blocked,
        ) = self.axis_step(
            tilt_command,
            self.tilt_accumulator,
            self.tilt_last_direction,
            self.tilt_position,
        )
        return AimOutput(
            active=True,
            vision_recent=True,
            laser_allowed=laser_request and not pan_blocked and not tilt_blocked,
            pan_limit_blocked=pan_blocked,
            tilt_limit_blocked=tilt_blocked,
            pan_steps=pan_steps,
            tilt_steps=tilt_steps,
            pan_position=self.pan_position,
            tilt_position=self.tilt_position,
        )


def source_checks(workspace: Path) -> None:
    adapter = (workspace / "car_final_ccs/user/e2025_aim_adapter.c").read_text(
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
    assert "#define E2025_ENABLE_AIM_ADAPTER 0" in config
    assert "#if E2025_ENABLE_AIM_ADAPTER" in main
    assert "e2025_status_was_aiming(&status)" in main
    assert "input->emergency_stop && aim_was_active" in main
    assert "e2025_aim_adapter_tick(" in main
    assert "stepper_init();" not in main
    assert "E2025_AIM_VISION_TIMEOUT_TICKS" in adapter
    assert "output->laser_allowed" in adapter
    assert "GPIO" not in adapter
    assert "user/e2025_aim_adapter.c" in projectspec


def main() -> None:
    workspace = Path(__file__).resolve().parents[1]
    results: list[dict[str, object]] = []

    adapter = AimAdapter()
    disabled = adapter.tick(aim_enable=False, emergency_stop=True)
    assert not disabled.active and not disabled.laser_allowed
    assert disabled.pan_steps == disabled.tilt_steps == 0
    assert disabled.fault == NONE
    results.append({"case": "disabled_is_zero_and_menu_sw4_does_not_latch", "status": "PASS"})

    stale_cache = adapter.tick(
        aim_enable=True,
        target_valid=True,
        spot_valid=True,
        laser_request=True,
        pan_command=1000,
    )
    assert not stale_cache.active and not stale_cache.laser_allowed
    assert stale_cache.pan_steps == 0
    results.append({"case": "cached_frame_cannot_start_new_session", "status": "PASS"})

    fresh = adapter.tick(
        aim_enable=True,
        vision_fresh=True,
        target_valid=True,
        spot_valid=True,
        laser_request=True,
        pan_command=1000,
        tilt_command=-1000,
    )
    assert fresh.active and fresh.vision_recent and fresh.laser_allowed
    assert (fresh.pan_steps, fresh.tilt_steps) == (4, -4)
    results.append({"case": "fresh_valid_frame_produces_bounded_steps", "status": "PASS"})

    fractional = AimAdapter()
    rows = []
    for tick in range(7):
        rows.append(
            fractional.tick(
                aim_enable=True,
                vision_fresh=tick == 0,
                target_valid=True,
                spot_valid=True,
                pan_command=40,
            )
        )
    assert all(row.pan_steps == 0 for row in rows[:6])
    assert rows[6].pan_steps == 1
    results.append({"case": "small_command_uses_fractional_step_accumulator", "status": "PASS"})

    watchdog = AimAdapter()
    watchdog.tick(
        aim_enable=True,
        vision_fresh=True,
        target_valid=True,
        spot_valid=True,
        laser_request=True,
    )
    for _ in range(VISION_TIMEOUT_TICKS):
        recent = watchdog.tick(
            aim_enable=True,
            target_valid=True,
            spot_valid=True,
            laser_request=True,
        )
    assert recent.vision_recent and recent.laser_allowed
    expired = watchdog.tick(
        aim_enable=True,
        target_valid=True,
        spot_valid=True,
        laser_request=True,
        pan_command=1000,
    )
    assert not expired.active and not expired.vision_recent
    assert not expired.laser_allowed and expired.pan_steps == 0
    results.append({"case": "vision_watchdog_stops_after_100ms_without_new_frame", "status": "PASS"})

    invalid = AimAdapter().tick(
        aim_enable=True,
        vision_fresh=True,
        target_valid=True,
        spot_valid=False,
        laser_request=True,
        pan_command=1000,
    )
    assert not invalid.active and not invalid.laser_allowed
    results.append({"case": "target_and_spot_must_both_be_valid", "status": "PASS"})

    limited = AimAdapter(pan_position=SOFT_LIMIT_STEPS - 1)
    hit = limited.tick(
        aim_enable=True,
        vision_fresh=True,
        target_valid=True,
        spot_valid=True,
        laser_request=True,
        pan_command=1000,
    )
    assert hit.pan_position == SOFT_LIMIT_STEPS and hit.pan_steps == 1
    assert hit.pan_limit_blocked and not hit.laser_allowed
    back = limited.tick(
        aim_enable=True,
        vision_fresh=True,
        target_valid=True,
        spot_valid=True,
        laser_request=True,
        pan_command=-1000,
    )
    assert back.pan_steps == -4 and not back.pan_limit_blocked
    assert back.laser_allowed
    results.append({"case": "soft_limit_blocks_outward_but_allows_return", "status": "PASS"})

    estop = AimAdapter()
    estop.tick(
        aim_enable=True,
        vision_fresh=True,
        target_valid=True,
        spot_valid=True,
    )
    stopped = estop.tick(aim_enable=True, emergency_stop=True)
    assert stopped.fault == ESTOP and not stopped.active
    latched = estop.tick(aim_enable=False)
    assert latched.fault == ESTOP
    results.append({"case": "sw4_estop_is_same_tick_and_latched", "status": "PASS"})

    source_checks(workspace)
    results.append({"case": "firmware_gate_and_no_gpio_markers", "status": "PASS"})

    output = workspace / "output/e2025_aim_adapter_sim"
    output.mkdir(parents=True, exist_ok=True)
    summary = {
        "suite": "e2025_aim_adapter",
        "tick_ms": 10,
        "vision_timeout_ms": VISION_TIMEOUT_TICKS * 10,
        "maximum_steps_per_axis_per_tick": MAX_STEPS_PER_TICK,
        "soft_limit_steps_from_power_on_origin": SOFT_LIMIT_STEPS,
        "feature_enabled_in_default_firmware": False,
        "physical_gpio_connected": False,
        "hardware_verified": False,
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
