#!/usr/bin/env python3
"""Regression tests for hw_accept_monitor.py using only the standard library."""

from __future__ import annotations

import json
import tempfile
from pathlib import Path

from hw_accept_monitor import MonitorSession, assess_sample, parse_hwacc_line, process_lines


INCOMPLETE = (
    "HWACC KEY_RAW=0x01 KEY_PASS=0x01 TRACK_BLACK=0x18 TRACK_PASS=0x18 "
    "ENC_L=0 ENC_R=0 MOTOR=0 STBY=0 "
    "K230=BLOCKED_UART2_UNVERIFIED RESULT=TESTING"
)
COMPLETE = (
    "HWACC KEY_RAW=0x00 KEY_PASS=0x0F TRACK_BLACK=0x00 TRACK_PASS=0xFF "
    "ENC_L=12 ENC_R=9 MOTOR=0 STBY=0 "
    "K230=BLOCKED_UART2_UNVERIFIED RESULT=INPUT_PASS"
)
UNSAFE = COMPLETE.replace("MOTOR=0", "MOTOR=200")


def require(condition: bool, name: str) -> None:
    if not condition:
        raise AssertionError(name)
    print(f"PASS {name}")


def main() -> int:
    require(parse_hwacc_line("boot text") is None, "unrelated_line_ignored")
    sample = parse_hwacc_line("[12:00:00] " + COMPLETE)
    require(sample is not None and sample.key_pass == 0x0F, "timestamped_line_parsed")
    status = assess_sample(sample)
    require(status["verdict"] == "INPUT_PASS_K230_BLOCKED", "blocked_k230_not_full_pass")
    require(assess_sample(parse_hwacc_line(UNSAFE))["verdict"] == "UNSAFE_OUTPUT",
            "unsafe_output_has_priority")

    try:
        parse_hwacc_line("HWACC KEY_PASS=0x0F")
    except ValueError:
        print("PASS malformed_line_rejected")
    else:
        raise AssertionError("malformed_line_rejected")

    session = MonitorSession("test-fixture")
    process_lines(["noise\n", INCOMPLETE + "\n", COMPLETE + "\n", UNSAFE + "\n"],
                  session, until_pass=True)
    require(len(session.samples) == 2, "until_pass_stops_at_first_input_pass")
    require(session.ignored_lines == 1, "ignored_line_counted")

    with tempfile.TemporaryDirectory() as directory:
        paths = session.write_reports(Path(directory), "test")
        require(all(path.is_file() for path in paths.values()), "three_report_files_written")
        report = json.loads(paths["json"].read_text(encoding="utf-8"))
        require(report["sample_count"] == 2, "json_sample_count_correct")
        require(report["final_status"]["verdict"] == "INPUT_PASS_K230_BLOCKED",
                "json_final_verdict_correct")
        csv_text = paths["csv"].read_text(encoding="utf-8-sig")
        require("outputs_safe" in csv_text and "INPUT_PASS_K230_BLOCKED" in csv_text,
                "csv_contains_safety_and_verdict")

    print("HWACC monitor regression: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
