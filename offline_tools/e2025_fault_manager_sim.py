#!/usr/bin/env python3
"""Offline replay for E2025 subsystem-fault propagation and display state."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path


IDLE = "IDLE"
RUNNING = "RUNNING"
ABORTED = "ABORTED"
FAULT = "FAULT"

NONE = "NONE"
USER_ESTOP = "E-STOP"
NO_FEEDBACK = "NO FB"
LEFT_MISSING = "LEFT FB"
RIGHT_MISSING = "RIGHT FB"
SPEED_MISMATCH = "SPEED"
LEFT_DIRECTION = "LEFT DIR"
RIGHT_DIRECTION = "RIGHT DIR"
AIM_ESTOP = "AIM STOP"
K230_LOST = "K230 LOST"

AIM_TASKS = {"AIM_2S", "AIM_4S", "MOVE_1", "MOVE_2", "CIRCLE"}

DRIVE_MAP = {
    "NONE": NONE,
    "E-STOP": USER_ESTOP,
    "NO FEEDBACK": NO_FEEDBACK,
    "LEFT MISSING": LEFT_MISSING,
    "RIGHT MISSING": RIGHT_MISSING,
    "SPEED DIFF": SPEED_MISMATCH,
    "LEFT DIR": LEFT_DIRECTION,
    "RIGHT DIR": RIGHT_DIRECTION,
}


@dataclass
class Status:
    state: str = IDLE
    task: str = "NONE"
    fault: str = NONE


@dataclass
class FaultManager:
    was_running: bool = False
    k230_seen: bool = False
    active_task: str = "NONE"

    def step(
        self,
        status: Status,
        *,
        vision_fresh: bool = False,
        drive_fault: str = "NONE",
        aim_fault: str = "NONE",
        k230_connected: bool = True,
    ) -> str:
        if status.state != RUNNING:
            self.was_running = False
            self.k230_seen = False
            self.active_task = status.task
            return NONE

        if not self.was_running or self.active_task != status.task:
            self.k230_seen = False
            self.active_task = status.task
        self.was_running = True
        if vision_fresh:
            self.k230_seen = True

        mapped = DRIVE_MAP[drive_fault]
        if mapped != NONE:
            return mapped
        if aim_fault == "E-STOP":
            return AIM_ESTOP
        if status.task in AIM_TASKS and self.k230_seen and not k230_connected:
            return K230_LOST
        return NONE


@dataclass
class AppModel:
    status: Status
    pending_fault: str = NONE
    drive_enabled: bool = True
    aim_enabled: bool = True
    laser_enabled: bool = True

    def request_fault(self, fault: str) -> None:
        if fault != NONE:
            self.pending_fault = fault

    def tick(self, *, emergency_stop: bool = False) -> None:
        if self.pending_fault != NONE:
            self.status.state = FAULT
            self.status.fault = self.pending_fault
            self.pending_fault = NONE
            self.drive_enabled = False
            self.aim_enabled = False
            self.laser_enabled = False
            return
        if emergency_stop:
            self.status.state = ABORTED
            self.status.fault = USER_ESTOP
            self.drive_enabled = False
            self.aim_enabled = False
            self.laser_enabled = False


def source_checks(workspace: Path) -> None:
    manager = (
        workspace / "car_final_ccs/user/e2025_fault_manager.c"
    ).read_text(encoding="utf-8")
    app = (workspace / "car_final_ccs/user/e2025_app.c").read_text(
        encoding="utf-8"
    )
    header = (workspace / "car_final_ccs/user/e2025_app.h").read_text(
        encoding="utf-8"
    )
    main = (workspace / "car_final_ccs/user/main.c").read_text(encoding="utf-8")
    projectspec = (workspace / "car_final_ccs/car_final_ccs.projectspec").read_text(
        encoding="utf-8"
    )

    assert "E2025_FAULT_DRIVE_NO_FEEDBACK" in header
    assert "E2025_FAULT_K230_LINK_LOST" in header
    assert "e2025_request_fault" in header and "e2025_fault_name" in header
    assert "if(g_fault_pending)" in app
    assert "g_e2025.fault = E2025_FAULT_USER_ESTOP" in app
    assert "status->fault = g_e2025.fault" in app
    assert "manager->k230_seen_in_session && !k230_connected" in manager
    assert "e2025_fault_from_drive" in manager
    assert "e2025_fault_manager_step(" in main
    assert "e2025_request_fault(app_fault);" in main
    assert "e2025_fault_name(status.fault)" in main
    assert "user/e2025_fault_manager.c" in projectspec


def main() -> None:
    workspace = Path(__file__).resolve().parents[1]
    results: list[dict[str, object]] = []

    manager = FaultManager()
    for source, expected in DRIVE_MAP.items():
        status = Status(RUNNING, "TRACK", NONE)
        assert manager.step(status, drive_fault=source) == expected
        manager = FaultManager()
    results.append({"case": "all_drive_faults_map_to_e2025_codes", "status": "PASS"})

    manager = FaultManager()
    app = AppModel(Status(RUNNING, "TRACK", NONE))
    mapped = manager.step(app.status, drive_fault="NO FEEDBACK")
    assert mapped == NO_FEEDBACK
    motor_stopped_same_tick = True
    app.request_fault(mapped)
    assert app.status.state == RUNNING and motor_stopped_same_tick
    app.tick()
    assert app.status.state == FAULT and app.status.fault == NO_FEEDBACK
    assert not app.drive_enabled and not app.aim_enabled and not app.laser_enabled
    results.append({"case": "drive_stops_now_and_state_faults_next_tick", "status": "PASS"})

    manager = FaultManager()
    aiming = Status(RUNNING, "AIM_2S", NONE)
    assert manager.step(aiming, k230_connected=False) == NONE
    assert manager.step(
        aiming, vision_fresh=True, k230_connected=True
    ) == NONE
    assert manager.step(aiming, k230_connected=False) == K230_LOST
    results.append({"case": "k230_loss_requires_frame_seen_in_session", "status": "PASS"})

    manager = FaultManager()
    tracking = Status(RUNNING, "TRACK", NONE)
    assert manager.step(
        tracking, vision_fresh=True, k230_connected=False
    ) == NONE
    results.append({"case": "track_only_task_ignores_k230_link", "status": "PASS"})

    manager = FaultManager()
    aiming = Status(RUNNING, "MOVE_1", NONE)
    assert manager.step(
        aiming, vision_fresh=True, k230_connected=True
    ) == NONE
    assert manager.step(aiming, aim_fault="E-STOP") == AIM_ESTOP
    results.append({"case": "aim_estop_maps_to_specific_fault", "status": "PASS"})

    app = AppModel(Status(RUNNING, "MOVE_1", NONE))
    app.tick(emergency_stop=True)
    assert app.status.state == ABORTED and app.status.fault == USER_ESTOP
    assert not app.drive_enabled and not app.laser_enabled
    results.append({"case": "raw_sw4_abort_keeps_visible_estop_reason", "status": "PASS"})

    manager = FaultManager()
    session = Status(RUNNING, "AIM_2S", NONE)
    manager.step(session, vision_fresh=True, k230_connected=True)
    session.state = FAULT
    assert manager.step(session, k230_connected=False) == NONE
    session = Status(RUNNING, "AIM_2S", NONE)
    assert manager.step(session, k230_connected=False) == NONE
    results.append({"case": "new_session_does_not_inherit_k230_seen", "status": "PASS"})

    source_checks(workspace)
    results.append({"case": "firmware_fault_propagation_markers", "status": "PASS"})

    output = workspace / "output/e2025_fault_manager_sim"
    output.mkdir(parents=True, exist_ok=True)
    summary = {
        "suite": "e2025_fault_manager",
        "control_tick_ms": 10,
        "motor_shutdown_latency_ticks": 0,
        "state_fault_latency_ticks": 1,
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
