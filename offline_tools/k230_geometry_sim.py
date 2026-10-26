#!/usr/bin/env python3
"""Validate K230 pure-Python A4 geometry against OpenCV homography."""

from __future__ import annotations

import importlib.util
import json
import math
import sys
from pathlib import Path

import cv2
import numpy as np


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from vision_e2025.synthetic import make_scene


OUTPUT = ROOT / "output" / "k230_geometry_sim"


def load_geometry():
    path = ROOT / "k230_e2025" / "target_geometry.py"
    spec = importlib.util.spec_from_file_location("k230_target_geometry", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def project(matrix: np.ndarray, point: tuple[float, float]) -> tuple[float, float]:
    value = cv2.perspectiveTransform(
        np.asarray(point, dtype=np.float32).reshape(1, 1, 2), matrix)[0, 0]
    return float(value[0]), float(value[1])


def distance(a: tuple[float, float], b: tuple[float, float]) -> float:
    return math.hypot(a[0] - b[0], a[1] - b[1])


def old_circle_reference(corners, phase_byte):
    phase = 2.0 * math.pi * phase_byte / 256.0
    center = (sum(point[0] for point in corners) / 4.0,
              sum(point[1] for point in corners) / 4.0)
    horizontal = (
        ((corners[1][0] - corners[0][0]) +
         (corners[2][0] - corners[3][0])) / 2.0,
        ((corners[1][1] - corners[0][1]) +
         (corners[2][1] - corners[3][1])) / 2.0,
    )
    vertical = (
        ((corners[3][0] - corners[0][0]) +
         (corners[2][0] - corners[1][0])) / 2.0,
        ((corners[3][1] - corners[0][1]) +
         (corners[2][1] - corners[1][1])) / 2.0,
    )
    return (
        center[0] + math.cos(phase) * horizontal[0] * 60.0 / 210.0 +
        math.sin(phase) * vertical[0] * 60.0 / 297.0,
        center[1] + math.cos(phase) * horizontal[1] * 60.0 / 210.0 +
        math.sin(phase) * vertical[1] * 60.0 / 297.0,
    )


def main() -> int:
    geometry = load_geometry()
    checks: list[dict[str, str]] = []
    failures: list[str] = []
    center_errors: list[float] = []
    circle_errors: list[float] = []
    old_circle_errors: list[float] = []
    recovered_radii: list[float] = []

    def check(name: str, condition: bool) -> None:
        checks.append({"check": name, "result": "PASS" if condition else "FAIL"})
        if not condition:
            failures.append(name)

    source_a4 = np.asarray(
        ((0.0, 0.0), (210.0, 0.0), (210.0, 297.0), (0.0, 297.0)),
        dtype=np.float32)

    for seed in range(40):
        _, truth = make_scene(seed)
        assert truth.target_quad is not None and truth.target_center is not None
        ordered = np.asarray(geometry.order_corners(truth.target_quad),
                             dtype=np.float32)
        forward = cv2.getPerspectiveTransform(source_a4, ordered)
        inverse = cv2.getPerspectiveTransform(ordered, source_a4)

        expected_center = project(forward, (105.0, 148.5))
        actual_center = geometry.target_center(truth.target_quad)
        assert actual_center is not None
        center_errors.append(distance(actual_center, expected_center))

        for phase_byte in range(0, 256, 16):
            angle = 2.0 * math.pi * phase_byte / 256.0
            point_mm = (105.0 + 60.0 * math.cos(angle),
                        148.5 + 60.0 * math.sin(angle))
            expected = project(forward, point_mm)
            actual = geometry.circle_reference(truth.target_quad, phase_byte)
            assert actual is not None
            circle_errors.append(distance(actual, expected))
            old_circle_errors.append(distance(
                old_circle_reference(ordered, phase_byte), expected))
            recovered = project(inverse, actual)
            recovered_radii.append(distance(recovered, (105.0, 148.5)))

        shuffled = [truth.target_quad[index] for index in (2, 0, 3, 1)]
        shuffled_center = geometry.target_center(shuffled)
        assert shuffled_center is not None
        center_errors.append(distance(shuffled_center, expected_center))

    center_max = max(center_errors)
    circle_max = max(circle_errors)
    radius_max_error = max(abs(value - 60.0) for value in recovered_radii)
    old_circle_mean = float(np.mean(old_circle_errors))
    new_circle_mean = float(np.mean(circle_errors))

    check("projective A4 center matches OpenCV", center_max < 0.001)
    check("6 cm circle matches OpenCV homography", circle_max < 0.001)
    check("inverse-projected circle radius remains 60 mm",
          radius_max_error < 0.001)
    check("corner ordering is independent of detector order", center_max < 0.001)
    check("projective mapping improves the former affine approximation",
          new_circle_mean < old_circle_mean)

    k230_main = (ROOT / "k230_e2025/main.py").read_text(encoding="utf-8")
    geometry_source = (ROOT / "k230_e2025/target_geometry.py").read_text(
        encoding="utf-8")
    check("K230 main imports the shared projective geometry",
          "from target_geometry import" in k230_main and
          "circle_reference(corners, circle_phase)" in k230_main)
    check("red target center has a geometry fallback and rejection gate",
          "fallback = target_center(corners)" in k230_main and
          "point_distance(detected, fallback) > max_offset" in k230_main)
    check("K230 boots in STOP with automatic laser disabled",
          "run_mode = MODE_STOP" in k230_main and
          "AUTO_LASER_ENABLE = False" in k230_main)
    check("geometry module stays MicroPython-compatible",
          "numpy" not in geometry_source and "cv2" not in geometry_source and
          "dataclass" not in geometry_source)

    summary = {
        "result": "PASS" if not failures else "FAIL",
        "hardware_validation": "NOT_RUN",
        "scenes": 40,
        "circle_points": len(circle_errors),
        "projective_center_error_px_max": center_max,
        "projective_circle_error_px_max": circle_max,
        "circle_radius_error_mm_max": radius_max_error,
        "new_circle_error_px_mean": new_circle_mean,
        "former_affine_circle_error_px_mean": old_circle_mean,
        "color_thresholds": "REQUIRES_REAL_CAMERA_CALIBRATION",
        "checks": checks,
        "failures": failures,
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    report = OUTPUT / "summary.json"
    report.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
                      encoding="utf-8")
    print(f"K230 target geometry simulation: {summary['result']}")
    print("Hardware validation: NOT_RUN")
    print(f"Report: {report}")
    if failures:
        for failure in failures:
            print(f"ERROR: {failure}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
