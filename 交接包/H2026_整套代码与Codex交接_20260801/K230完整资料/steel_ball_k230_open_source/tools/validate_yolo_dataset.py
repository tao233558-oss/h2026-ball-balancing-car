"""Validate the prepared steel-ball YOLO dataset without Ultralytics."""

from __future__ import annotations

import csv
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DATASET = PROJECT_ROOT / "steel_ball_yolo"
EXPECTED_SIZE = (1920, 1080)


def validate_split(split: str) -> tuple[set[str], int]:
    image_dir = DATASET / "images" / split
    label_dir = DATASET / "labels" / split
    images = sorted(image_dir.glob("*.jpg"))
    labels = sorted(label_dir.glob("*.txt"))
    image_stems = {path.stem for path in images}
    label_stems = {path.stem for path in labels}
    if image_stems != label_stems:
        raise AssertionError(
            f"{split}: image/label mismatch: "
            f"images_only={image_stems - label_stems}, "
            f"labels_only={label_stems - image_stems}"
        )

    boxes = 0
    for image_path in images:
        with Image.open(image_path) as image:
            if image.size != EXPECTED_SIZE:
                raise AssertionError(
                    f"{image_path}: expected {EXPECTED_SIZE}, got {image.size}"
                )
            image.verify()

        label_path = label_dir / f"{image_path.stem}.txt"
        rows = [line for line in label_path.read_text().splitlines() if line.strip()]
        if not rows:
            raise AssertionError(f"{label_path}: empty label file")
        for line_number, row in enumerate(rows, start=1):
            fields = row.split()
            if len(fields) != 5:
                raise AssertionError(f"{label_path}:{line_number}: expected 5 fields")
            class_id = int(fields[0])
            x_center, y_center, width, height = map(float, fields[1:])
            if class_id != 0:
                raise AssertionError(
                    f"{label_path}:{line_number}: unexpected class {class_id}"
                )
            if not (
                0 <= x_center <= 1
                and 0 <= y_center <= 1
                and 0 < width <= 1
                and 0 < height <= 1
            ):
                raise AssertionError(
                    f"{label_path}:{line_number}: coordinates out of range"
                )
        boxes += len(rows)
    return image_stems, boxes


def main() -> None:
    expected_counts: dict[str, tuple[int, int]] = {}
    with (DATASET / "split_manifest.csv").open(
        "r", encoding="utf-8-sig", newline=""
    ) as handle:
        rows = list(csv.DictReader(handle))
    for split in ("train", "val", "review_pending"):
        split_rows = [row for row in rows if row["split"] == split]
        expected_counts[split] = (
            len(split_rows),
            sum(int(row["box_count"]) for row in split_rows),
        )

    all_stems: dict[str, set[str]] = {}
    actual_counts: dict[str, tuple[int, int]] = {}
    for split in ("train", "val", "review_pending"):
        stems, boxes = validate_split(split)
        all_stems[split] = stems
        actual_counts[split] = (len(stems), boxes)
        if actual_counts[split] != expected_counts[split]:
            raise AssertionError(
                f"{split}: manifest={expected_counts[split]}, "
                f"files={actual_counts[split]}"
            )

    for left, right in (
        ("train", "val"),
        ("train", "review_pending"),
        ("val", "review_pending"),
    ):
        overlap = all_stems[left] & all_stems[right]
        if overlap:
            raise AssertionError(f"{left}/{right} overlap: {sorted(overlap)}")

    calibration_images = sorted((DATASET / "calibration").glob("*.jpg"))
    if len(calibration_images) != 20:
        raise AssertionError(
            f"Expected 20 calibration images, got {len(calibration_images)}"
        )
    if not {path.stem for path in calibration_images} <= all_stems["train"]:
        raise AssertionError("Calibration images must be a subset of train")

    yaml_text = (DATASET / "steel_ball.yaml").read_text(encoding="utf-8")
    for required in (
        "train: images/train",
        "val: images/val",
        "0: steel_ball",
    ):
        if required not in yaml_text:
            raise AssertionError(f"Missing YAML entry: {required}")

    print(
        "PASS: "
        + ", ".join(
            f"{split}={images} images/{boxes} boxes"
            for split, (images, boxes) in actual_counts.items()
        )
        + f", calibration={len(calibration_images)} images"
    )


if __name__ == "__main__":
    main()
