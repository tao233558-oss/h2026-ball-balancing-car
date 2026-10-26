"""Validate X-AnyLabeling JSON and YOLO labels for the full dataset."""

from __future__ import annotations

import json
from pathlib import Path

from PIL import Image


DATASET_DIR = Path("steel_ball_dataset")
YOLO_DIR = DATASET_DIR / "labels_yolo"
EXPECTED_SIZE = (1920, 1080)


def main() -> None:
    images = sorted(DATASET_DIR.glob("*.jpg"))
    if len(images) != 52:
        raise AssertionError(f"Expected 52 images, found {len(images)}")

    total_shapes = 0
    for image_path in images:
        json_path = image_path.with_suffix(".json")
        yolo_path = YOLO_DIR / f"{image_path.stem}.txt"
        if not json_path.is_file():
            raise AssertionError(f"Missing {json_path}")
        if not yolo_path.is_file():
            raise AssertionError(f"Missing {yolo_path}")
        if Image.open(image_path).size != EXPECTED_SIZE:
            raise AssertionError(f"Unexpected image size: {image_path}")

        payload = json.loads(json_path.read_text(encoding="utf-8"))
        if payload["imagePath"] != image_path.name:
            raise AssertionError(f"Bad imagePath in {json_path}")
        if (payload["imageWidth"], payload["imageHeight"]) != EXPECTED_SIZE:
            raise AssertionError(f"Bad image dimensions in {json_path}")

        shapes = payload["shapes"]
        yolo_lines = [
            line
            for line in yolo_path.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        if len(shapes) != len(yolo_lines):
            raise AssertionError(
                f"JSON/YOLO count mismatch for {image_path.name}: "
                f"{len(shapes)} != {len(yolo_lines)}"
            )

        for shape in shapes:
            if shape["label"] != "steel_ball":
                raise AssertionError(f"Unexpected label in {json_path}")
            if shape["shape_type"] != "rectangle":
                raise AssertionError(f"Unexpected shape type in {json_path}")
            (x1, y1), (x2, y2) = shape["points"]
            if not (0 <= x1 < x2 < EXPECTED_SIZE[0]):
                raise AssertionError(f"Bad X bounds in {json_path}: {shape['points']}")
            if not (0 <= y1 < y2 < EXPECTED_SIZE[1]):
                raise AssertionError(f"Bad Y bounds in {json_path}: {shape['points']}")

        for line in yolo_lines:
            values = line.split()
            if len(values) != 5 or values[0] != "0":
                raise AssertionError(f"Bad YOLO row in {yolo_path}: {line}")
            center_x, center_y, width, height = map(float, values[1:])
            if not (
                0 <= center_x <= 1
                and 0 <= center_y <= 1
                and 0 < width <= 1
                and 0 < height <= 1
            ):
                raise AssertionError(f"Out-of-range YOLO row in {yolo_path}: {line}")

        total_shapes += len(shapes)

    if total_shapes != 756:
        raise AssertionError(f"Expected 756 boxes, found {total_shapes}")
    print(f"PASS: {len(images)} images, {total_shapes} boxes, JSON/YOLO counts match")


if __name__ == "__main__":
    main()
