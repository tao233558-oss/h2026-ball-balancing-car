#!/usr/bin/env python3
"""End-to-end 10 ms replay for the E2025 app and all safety adapters."""

from __future__ import annotations

import csv
import json
from dataclasses import dataclass, field
from pathlib import Path

from e2025_aim_adapter_sim import AimAdapter
from e2025_drive_adapter_sim import DriveAdapter
from e2025_fault_manager_sim import (
    ABORTED,
    FAULT,
    K230_LOST,
    NONE,
    RUNNING,
    USER_ESTOP,
    FaultManager,
    Status,
)
from e2025_offline_sim import encode_vision_frame
from e2025_track_sim import CENTER, LEFT_CORNER
from k230_link_sim import LinkModel


TICK_MS = 10
FRAME_PERIOD_TICKS = 5  # 20 FPS, intentionally slower than the control loop.
TARGET_X = 160
TARGET_Y = 120

TRACK_N = "TRACK_N"
AIM_2S = "AIM_2S"
AIM_4S = "AIM_4S"
MOVE_1 = "MOVE_1"
MOVE_2 = "MOVE_2"
CIRCLE = "CIRCLE"
DONE = "DONE"
TIMEOUT = "TIMEOUT"

LIMITS = {
    TRACK_N: 2000,
    AIM_2S: 200,
    AIM_4S: 400,
    MOVE_1: 2000,
    MOVE_2: 4000,
    CIRCLE: 2000,
}
DRIVE_TASKS = {TRACK_N, MOVE_1, MOVE_2, CIRCLE}
AIM_TASKS = {AIM_2S, AIM_4S, MOVE_1, MOVE_2, CIRCLE}


@dataclass
class AppInput:
    corner_event: bool = False
    track_pattern: int = CENTER
    vision_fresh: bool = False
    target_valid: bool = False
    spot_valid: bool = False
    circle_complete: bool = False
    emergency_stop: bool = False
    target_x: int = 0
    target_y: int = 0
    spot_x: int = 0
    spot_y: int = 0


@dataclass
class AppOutput:
    drive_enable: bool = False
    track_enable: bool = False
    aim_enable: bool = False
    laser_enable: bool = False
    pan_command: int = 0
    tilt_command: int = 0


@dataclass
class AppState:
    task: str
    requested_laps: int
    state: str = RUNNING
    fault: str = NONE
    completed_laps: int = 0
    completed_corners: int = 0
    stable_frames: int = 0
    elapsed_ticks: int = 0
    pending_fault: str = NONE
    output: AppOutput = field(default_factory=AppOutput)

    def outputs_off(self) -> None:
        self.output = AppOutput()

    def request_fault(self, fault: str) -> None:
        if fault != NONE:
            self.pending_fault = fault

    def status(self) -> Status:
        return Status(self.state, self.task, self.fault)

    def tick(self, inputs: AppInput) -> None:
        if self.pending_fault != NONE:
            self.state = FAULT
            self.fault = self.pending_fault
            self.pending_fault = NONE
            self.outputs_off()
            return
        if self.state != RUNNING:
            self.outputs_off()
            return
        if inputs.emergency_stop:
            self.state = ABORTED
            self.fault = USER_ESTOP
            self.outputs_off()
            return
        if self.elapsed_ticks >= LIMITS[self.task]:
            self.state = TIMEOUT
            self.outputs_off()
            return

        self.elapsed_ticks += 1
        self.output = AppOutput(
            drive_enable=self.task in DRIVE_TASKS,
            track_enable=self.task in DRIVE_TASKS,
        )

        if self.task in DRIVE_TASKS and inputs.corner_event:
            self.completed_corners += 1
            if self.completed_corners >= 4:
                self.completed_corners = 0
                self.completed_laps += 1

        if self.task in AIM_TASKS:
            self.output.aim_enable = True
            self.output.laser_enable = inputs.target_valid
            if not inputs.target_valid or not inputs.spot_valid:
                self.stable_frames = 0
            else:
                error_x = inputs.target_x - inputs.spot_x
                error_y = inputs.target_y - inputs.spot_y
                self.output.pan_command = max(-1000, min(1000, error_x * 4))
                self.output.tilt_command = max(-1000, min(1000, error_y * 4))
                if inputs.vision_fresh:
                    if abs(error_x) <= 6 and abs(error_y) <= 6:
                        self.stable_frames = min(3, self.stable_frames + 1)
                    else:
                        self.stable_frames = 0

        complete = False
        if self.task == TRACK_N:
            complete = self.completed_laps >= self.requested_laps
        elif self.task in {AIM_2S, AIM_4S}:
            complete = self.stable_frames >= 3
        elif self.task in {MOVE_1, MOVE_2}:
            complete = self.completed_laps >= self.requested_laps
        elif self.task == CIRCLE:
            complete = self.completed_laps >= 1 and inputs.circle_complete
        if complete:
            self.state = DONE
            self.outputs_off()


@dataclass(frozen=True)
class Scenario:
    name: str
    task: str
    laps: int
    expected_state: str
    expected_fault: str = NONE
    no_feedback_from_tick: int | None = None
    k230_dropout_tick: int | None = None
    emergency_tick: int | None = None


@dataclass
class ScenarioResult:
    scenario: Scenario
    rows: list[dict[str, object]]
    final_state: str
    final_fault: str
    elapsed_ticks: int
    completed_laps: int
    first_laser_off_after_dropout: int | None = None
    fault_requested_tick: int | None = None


class SystemRig:
    def __init__(self, scenario: Scenario) -> None:
        requested = scenario.laps
        if scenario.task in {MOVE_1, CIRCLE}:
            requested = 1
        elif scenario.task == MOVE_2:
            requested = 2
        self.scenario = scenario
        self.app = AppState(scenario.task, requested)
        self.drive = DriveAdapter()
        self.aim = AimAdapter()
        self.link = LinkModel()
        self.fault_manager = FaultManager()
        self.sequence = 0
        self.have_vision_sequence = False
        self.last_vision_sequence = 0
        self.rows: list[dict[str, object]] = []
        self.first_laser_off_after_dropout: int | None = None
        self.fault_requested_tick: int | None = None

    def feed_k230_if_due(self, tick: int) -> None:
        if self.scenario.task not in AIM_TASKS:
            return
        if self.scenario.k230_dropout_tick is not None and tick >= self.scenario.k230_dropout_tick:
            return
        if tick % FRAME_PERIOD_TICKS != 0:
            return

        delay = 100 if self.scenario.task == AIM_4S else 0
        error = max(0, 100 - max(0, self.app.elapsed_ticks - delay) * 2)
        flags = 0x03
        if self.scenario.task == CIRCLE and self.app.elapsed_ticks >= 1200:
            flags |= 0x04
        frame = encode_vision_frame(
            self.sequence,
            flags,
            TARGET_X,
            TARGET_Y,
            TARGET_X - error,
            TARGET_Y + error // 2,
            min(255, self.app.elapsed_ticks // 5),
        )
        for byte in frame:
            self.link.push(byte)
        self.sequence = (self.sequence + 1) & 0xFF

    def make_input(self, tick: int) -> tuple[AppInput, bool]:
        self.feed_k230_if_due(tick)
        self.link.tick()
        connected = self.link.connected

        corner_period = 300 if self.scenario.task == CIRCLE else 75
        corner = (
            self.scenario.task in DRIVE_TASKS
            and self.app.elapsed_ticks > 0
            and self.app.elapsed_ticks % corner_period == 0
        )
        inputs = AppInput(
            corner_event=corner,
            track_pattern=LEFT_CORNER if corner else CENTER,
            emergency_stop=(self.scenario.emergency_tick == tick),
        )

        if connected and self.link.sample is not None:
            sequence, flags, tx, ty, sx, sy, _phase = self.link.sample
            inputs.target_valid = bool(flags & 0x01)
            inputs.spot_valid = bool(flags & 0x02)
            inputs.circle_complete = bool(flags & 0x04)
            inputs.target_x = tx
            inputs.target_y = ty
            inputs.spot_x = sx
            inputs.spot_y = sy
            if not self.have_vision_sequence or sequence != self.last_vision_sequence:
                inputs.vision_fresh = True
                self.last_vision_sequence = sequence
                self.have_vision_sequence = True
        else:
            self.have_vision_sequence = False
        return inputs, connected

    def tick(self, tick: int) -> None:
        pre_track = self.app.state == RUNNING and self.app.task in DRIVE_TASKS
        pre_aim = self.app.state == RUNNING and self.app.task in AIM_TASKS
        inputs, connected = self.make_input(tick)
        self.app.tick(inputs)

        track_request = self.app.output.track_enable or (
            inputs.emergency_stop and pre_track
        )
        no_feedback = (
            self.scenario.no_feedback_from_tick is not None
            and tick >= self.scenario.no_feedback_from_tick
        )
        if no_feedback:
            left_delta = right_delta = 0
        elif inputs.track_pattern == LEFT_CORNER:
            left_delta, right_delta = 0, 1
        else:
            left_delta = right_delta = 1
        drive_output = self.drive.tick(
            track_enable=track_request,
            emergency_stop=inputs.emergency_stop,
            pattern=inputs.track_pattern,
            left_delta=left_delta,
            right_delta=right_delta,
        )

        aim_request = self.app.output.aim_enable or (
            inputs.emergency_stop and pre_aim
        )
        aim_output = self.aim.tick(
            aim_enable=aim_request,
            emergency_stop=inputs.emergency_stop,
            vision_fresh=inputs.vision_fresh,
            target_valid=inputs.target_valid,
            spot_valid=inputs.spot_valid,
            laser_request=self.app.output.laser_enable,
            pan_command=self.app.output.pan_command,
            tilt_command=self.app.output.tilt_command,
        )

        app_fault = self.fault_manager.step(
            self.app.status(),
            vision_fresh=inputs.vision_fresh,
            drive_fault=drive_output.fault,
            aim_fault=aim_output.fault,
            k230_connected=connected,
        )
        if app_fault != NONE:
            self.app.request_fault(app_fault)
            if self.fault_requested_tick is None:
                self.fault_requested_tick = tick

        if (
            self.scenario.k230_dropout_tick is not None
            and tick >= self.scenario.k230_dropout_tick
            and not aim_output.laser_allowed
            and self.first_laser_off_after_dropout is None
        ):
            self.first_laser_off_after_dropout = tick

        assert abs(aim_output.pan_steps) <= 4
        assert abs(aim_output.tilt_steps) <= 4
        assert 0 <= drive_output.left_pwm <= 900
        assert 0 <= drive_output.right_pwm <= 900
        assert not aim_output.laser_allowed or (
            aim_output.active and connected and inputs.target_valid and inputs.spot_valid
        )

        self.rows.append(
            {
                "scenario": self.scenario.name,
                "tick_10ms": tick,
                "time_ms": tick * TICK_MS,
                "task": self.app.task,
                "state": self.app.state,
                "fault": self.app.fault,
                "laps": self.app.completed_laps,
                "corners": self.app.completed_corners,
                "pattern_hex": f"0x{inputs.track_pattern:02X}",
                "vision_fresh": int(inputs.vision_fresh),
                "k230_connected": int(connected),
                "drive_active": int(drive_output.active),
                "left_target": drive_output.left_target,
                "right_target": drive_output.right_target,
                "left_pwm": drive_output.left_pwm,
                "right_pwm": drive_output.right_pwm,
                "drive_fault": drive_output.fault,
                "aim_active": int(aim_output.active),
                "pan_steps": aim_output.pan_steps,
                "tilt_steps": aim_output.tilt_steps,
                "laser_allowed": int(aim_output.laser_allowed),
                "fault_requested": app_fault,
            }
        )

    def run(self) -> ScenarioResult:
        for tick in range(LIMITS[self.scenario.task] + 100):
            self.tick(tick)
            if self.app.state != RUNNING and self.app.pending_fault == NONE:
                break
        return ScenarioResult(
            scenario=self.scenario,
            rows=self.rows,
            final_state=self.app.state,
            final_fault=self.app.fault,
            elapsed_ticks=self.app.elapsed_ticks,
            completed_laps=self.app.completed_laps,
            first_laser_off_after_dropout=self.first_laser_off_after_dropout,
            fault_requested_tick=self.fault_requested_tick,
        )


def validate_result(result: ScenarioResult) -> dict[str, object]:
    scenario = result.scenario
    assert result.final_state == scenario.expected_state, (
        scenario.name,
        result.final_state,
    )
    assert result.final_fault == scenario.expected_fault, (
        scenario.name,
        result.final_fault,
    )
    assert result.elapsed_ticks <= LIMITS[scenario.task]
    final = result.rows[-1]
    assert final["drive_active"] == 0
    assert final["aim_active"] == 0
    assert final["laser_allowed"] == 0

    if scenario.expected_state == DONE:
        if scenario.task in DRIVE_TASKS:
            assert any(row["drive_active"] for row in result.rows[:-1])
        else:
            assert not any(row["drive_active"] for row in result.rows)
        if scenario.task in AIM_TASKS:
            assert any(row["aim_active"] for row in result.rows[:-1])
            assert any(row["laser_allowed"] for row in result.rows[:-1])
        assert not any(row["fault_requested"] != NONE for row in result.rows)

    if scenario.no_feedback_from_tick is not None:
        assert result.fault_requested_tick is not None
        request_row = result.rows[result.fault_requested_tick]
        assert request_row["drive_active"] == 0
        assert request_row["left_pwm"] == request_row["right_pwm"] == 0
        assert result.elapsed_ticks - scenario.no_feedback_from_tick <= 120

    if scenario.k230_dropout_tick is not None:
        assert result.first_laser_off_after_dropout is not None
        assert result.fault_requested_tick is not None
        assert result.first_laser_off_after_dropout - scenario.k230_dropout_tick <= 12
        assert result.fault_requested_tick - scenario.k230_dropout_tick <= 25

    if scenario.emergency_tick is not None:
        emergency_row = next(
            row for row in result.rows if row["tick_10ms"] == scenario.emergency_tick
        )
        assert emergency_row["state"] == ABORTED
        assert emergency_row["drive_active"] == 0
        assert emergency_row["aim_active"] == 0
        assert emergency_row["laser_allowed"] == 0

    return {
        "scenario": scenario.name,
        "task": scenario.task,
        "expected_state": scenario.expected_state,
        "actual_state": result.final_state,
        "expected_fault": scenario.expected_fault,
        "actual_fault": result.final_fault,
        "elapsed_ticks": result.elapsed_ticks,
        "elapsed_seconds": round(result.elapsed_ticks * TICK_MS / 1000, 2),
        "completed_laps": result.completed_laps,
        "status": "PASS",
    }


def source_checks(workspace: Path) -> None:
    app = (workspace / "car_final_ccs/user/e2025_app.c").read_text(
        encoding="utf-8"
    )
    main = (workspace / "car_final_ccs/user/main.c").read_text(encoding="utf-8")
    config = (workspace / "car_final_ccs/user/app_config.h").read_text(
        encoding="utf-8"
    )

    assert "Consecutive means consecutive camera frames" in app
    assert "if(input->vision_fresh)" in app
    assert "e2025_fault_manager_step(" in main
    assert "k230_link_tick_10ms();" in main
    assert "e2025_apply_output(" in main
    assert "#define E2025_OFFLINE_SIMULATION 1" in config
    assert "#define E2025_ENABLE_CASCADE_TRACKING 0" in config
    assert "#define E2025_ENABLE_AIM_ADAPTER 0" in config
    assert "#define E2025_ENABLE_K230_UART0_BRIDGE 0" in config


def main() -> None:
    workspace = Path(__file__).resolve().parents[1]
    scenarios = [
        Scenario("normal_track_5_laps", TRACK_N, 5, DONE),
        Scenario("normal_aim_2s_20fps", AIM_2S, 1, DONE),
        Scenario("normal_aim_4s_20fps", AIM_4S, 1, DONE),
        Scenario("normal_move_aim_1_lap", MOVE_1, 1, DONE),
        Scenario("normal_move_aim_2_laps", MOVE_2, 2, DONE),
        Scenario("normal_move_draw_circle", CIRCLE, 1, DONE),
        Scenario(
            "fault_drive_no_feedback",
            TRACK_N,
            5,
            FAULT,
            "NO FB",
            no_feedback_from_tick=100,
        ),
        Scenario(
            "fault_k230_dropout",
            MOVE_2,
            2,
            FAULT,
            K230_LOST,
            k230_dropout_tick=120,
        ),
        Scenario(
            "abort_sw4_move_aim",
            MOVE_1,
            1,
            ABORTED,
            USER_ESTOP,
            emergency_tick=80,
        ),
    ]

    results = [SystemRig(scenario).run() for scenario in scenarios]
    matrix = [validate_result(result) for result in results]
    source_checks(workspace)

    output = workspace / "output/e2025_system_integration"
    output.mkdir(parents=True, exist_ok=True)
    timeline_rows = [row for result in results for row in result.rows]
    with (output / "system_timeline.csv").open(
        "w", newline="", encoding="utf-8-sig"
    ) as handle:
        writer = csv.DictWriter(handle, fieldnames=timeline_rows[0].keys())
        writer.writeheader()
        writer.writerows(timeline_rows)
    with (output / "test_matrix.csv").open(
        "w", newline="", encoding="utf-8-sig"
    ) as handle:
        writer = csv.DictWriter(handle, fieldnames=matrix[0].keys())
        writer.writeheader()
        writer.writerows(matrix)

    summary = {
        "suite": "e2025_full_system_integration",
        "control_tick_ms": TICK_MS,
        "vision_frame_rate_fps": 1000 // (FRAME_PERIOD_TICKS * TICK_MS),
        "scenario_count": len(matrix),
        "normal_task_count": 6,
        "fault_injection_count": 3,
        "hardware_verified": False,
        "all_passed": all(row["status"] == "PASS" for row in matrix),
        "matrix": matrix,
    }
    (output / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    for row in matrix:
        print(
            f"PASS {row['scenario']}: {row['actual_state']} "
            f"{row['actual_fault']} {row['elapsed_seconds']:.2f}s"
        )
    print(f"PASS summary={output / 'summary.json'}")
    print(f"PASS timeline={output / 'system_timeline.csv'}")
    print(f"PASS matrix={output / 'test_matrix.csv'}")


if __name__ == "__main__":
    main()
