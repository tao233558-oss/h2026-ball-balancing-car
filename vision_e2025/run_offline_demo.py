"""Generate scenes, run vision detection, and verify protocol compatibility."""

from __future__ import annotations

import json
from pathlib import Path

import cv2
import numpy as np

from .detector import annotate, detect
from .protocol import (
    FLAG_CIRCLE_COMPLETE,
    FLAG_SPOT_VALID,
    FLAG_TARGET_VALID,
    VisionPacket,
    decode,
    encode,
    hex_bytes,
)
from .synthetic import make_scene
from .target_model import render_target


def _distance(a: tuple[float, float], b: tuple[float, float]) -> float:
    return float(np.linalg.norm(np.asarray(a) - np.asarray(b)))


def _write_image(path: Path, image: np.ndarray) -> None:
    """cv2.imwrite does not reliably support non-ASCII Windows paths."""
    suffix = path.suffix.lower() or ".png"
    ok, encoded = cv2.imencode(suffix, image)
    if not ok:
        raise RuntimeError(f"failed to encode image: {path}")
    encoded.tofile(str(path))


def _make_contact_sheet(images: list[np.ndarray], cell_size: tuple[int, int] = (480, 270)) -> np.ndarray:
    cells = [cv2.resize(image, cell_size, interpolation=cv2.INTER_AREA) for image in images]
    while len(cells) < 6:
        cells.append(np.full((cell_size[1], cell_size[0], 3), 245, dtype=np.uint8))
    return np.vstack((np.hstack(cells[:3]), np.hstack(cells[3:6])))


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    output = root / "output" / "vision_e2025"
    cases_dir = output / "cases"
    rectified_dir = output / "rectified"
    cases_dir.mkdir(parents=True, exist_ok=True)
    rectified_dir.mkdir(parents=True, exist_ok=True)

    _write_image(output / "canonical_target.png", render_target(3.0))

    center_errors: list[float] = []
    spot_errors: list[float] = []
    target_detected = 0
    spot_detected = 0
    annotated_samples: list[np.ndarray] = []
    sample_frame = b""

    case_count = 40
    for seed in range(case_count):
        scene, truth = make_scene(seed)
        result = detect(scene)
        if result.target_valid:
            target_detected += 1
        if result.spot_valid:
            spot_detected += 1

        if result.target_center is not None and truth.target_center is not None:
            center_errors.append(_distance(result.target_center, truth.target_center))
        if result.spot_center is not None and truth.spot_center is not None:
            spot_errors.append(_distance(result.spot_center, truth.spot_center))

        flags = 0
        if result.target_valid:
            flags |= FLAG_TARGET_VALID
        if result.spot_valid:
            flags |= FLAG_SPOT_VALID
        if seed == case_count - 1:
            flags |= FLAG_CIRCLE_COMPLETE

        target = result.target_center or (0.0, 0.0)
        spot = result.spot_center or (0.0, 0.0)
        packet = VisionPacket(
            sequence=seed,
            flags=flags,
            target_x=round(target[0]),
            target_y=round(target[1]),
            spot_x=round(spot[0]),
            spot_y=round(spot[1]),
            circle_phase=round(seed * 255 / max(1, case_count - 1)),
        )
        frame = encode(packet)
        assert decode(frame) == packet
        if seed == 0:
            sample_frame = frame

        annotated = annotate(scene, result, circle_phase=seed / case_count)
        if seed < 8:
            _write_image(cases_dir / f"case_{seed:02d}.png", annotated)
            if result.rectified is not None:
                _write_image(rectified_dir / f"case_{seed:02d}.png", result.rectified)
        if seed < 6:
            annotated_samples.append(annotated)

    no_target_scene, _ = make_scene(1001, target_present=False, spot_present=False)
    no_target_result = detect(no_target_scene)
    assert not no_target_result.target_valid

    no_spot_scene, _ = make_scene(1002, target_present=True, spot_present=False)
    no_spot_result = detect(no_spot_scene)
    assert no_spot_result.target_valid and not no_spot_result.spot_valid

    target_rate = target_detected / case_count
    spot_rate = spot_detected / case_count
    center_mean = float(np.mean(center_errors)) if center_errors else float("inf")
    center_max = float(np.max(center_errors)) if center_errors else float("inf")
    spot_mean = float(np.mean(spot_errors)) if spot_errors else float("inf")
    spot_max = float(np.max(spot_errors)) if spot_errors else float("inf")

    summary = {
        "cases": case_count,
        "target_detection_rate": target_rate,
        "spot_detection_rate": spot_rate,
        "target_center_error_px_mean": center_mean,
        "target_center_error_px_max": center_max,
        "laser_spot_error_px_mean": spot_mean,
        "laser_spot_error_px_max": spot_max,
        "no_target_rejected": True,
        "missing_spot_rejected": True,
        "sample_protocol_frame": hex_bytes(sample_frame),
    }
    (output / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    (output / "sample_protocol_frame.txt").write_text(
        hex_bytes(sample_frame) + "\n", encoding="ascii"
    )
    _write_image(output / "contact_sheet.png", _make_contact_sheet(annotated_samples))

    print(json.dumps(summary, ensure_ascii=False, indent=2))
    if target_rate < 0.95 or spot_rate < 0.95:
        raise SystemExit("detection rate below 95%")
    if center_mean > 8.0 or spot_mean > 6.0:
        raise SystemExit("mean position error above offline threshold")
    print("offline vision validation: PASS")


if __name__ == "__main__":
    main()
