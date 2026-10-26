"""Build a deterministic, reviewed YOLO dataset for steel-ball detection.

The original X-AnyLabeling files stay untouched. Only annotations marked
``checked`` in ``annotation_summary.csv`` enter train/val. Images marked
``review_required`` are copied to a separate review_pending area.
"""

from __future__ import annotations

import argparse
import csv
import random
import shutil
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = PROJECT_ROOT / "steel_ball_dataset"
DEFAULT_OUTPUT = PROJECT_ROOT / "steel_ball_yolo"


@dataclass(frozen=True)
class Item:
    image: str
    box_count: int
    status: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--val-ratio", type=float, default=0.20)
    parser.add_argument("--seed", type=int, default=230)
    parser.add_argument("--calibration-count", type=int, default=20)
    return parser.parse_args()


def load_items(summary_path: Path) -> list[Item]:
    items: list[Item] = []
    with summary_path.open("r", encoding="utf-8-sig", newline="") as handle:
        for row in csv.DictReader(handle):
            if row["image"] == "TOTAL":
                continue
            items.append(
                Item(
                    image=row["image"],
                    box_count=int(row["box_count"]),
                    status=row["status"],
                )
            )
    return items


def box_stratum(box_count: int) -> str:
    if box_count == 1:
        return "single"
    if box_count <= 4:
        return "few"
    if box_count <= 9:
        return "medium"
    return "many"


def stratified_split(
    checked: list[Item], val_ratio: float, seed: int
) -> tuple[list[Item], list[Item]]:
    if not 0 < val_ratio < 1:
        raise ValueError("--val-ratio must be between 0 and 1")

    groups: dict[str, list[Item]] = defaultdict(list)
    for item in checked:
        groups[box_stratum(item.box_count)].append(item)

    rng = random.Random(seed)
    for group in groups.values():
        group.sort(key=lambda item: item.image)
        rng.shuffle(group)

    target_val = max(1, round(len(checked) * val_ratio))
    allocations = {
        key: int(len(group) * val_ratio) for key, group in groups.items()
    }
    remainder_order = sorted(
        groups,
        key=lambda key: (
            (len(groups[key]) * val_ratio) - allocations[key],
            len(groups[key]),
            key,
        ),
        reverse=True,
    )
    while sum(allocations.values()) < target_val:
        for key in remainder_order:
            if allocations[key] < len(groups[key]):
                allocations[key] += 1
                break

    val: list[Item] = []
    train: list[Item] = []
    for key, group in groups.items():
        val.extend(group[: allocations[key]])
        train.extend(group[allocations[key] :])
    return sorted(train, key=lambda item: item.image), sorted(
        val, key=lambda item: item.image
    )


def ensure_new_output(output: Path) -> None:
    if output.exists() and any(output.iterdir()):
        raise FileExistsError(
            f"{output} is not empty. Use a new --output directory so an existing "
            "dataset is never silently mixed or overwritten."
        )
    output.mkdir(parents=True, exist_ok=True)


def copy_item(source: Path, output: Path, item: Item, split: str) -> None:
    image_source = source / item.image
    label_source = source / "labels_yolo" / f"{Path(item.image).stem}.txt"
    if not image_source.is_file():
        raise FileNotFoundError(image_source)
    if not label_source.is_file():
        raise FileNotFoundError(label_source)

    image_target = output / "images" / split / item.image
    label_target = output / "labels" / split / label_source.name
    image_target.parent.mkdir(parents=True, exist_ok=True)
    label_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(image_source, image_target)
    shutil.copy2(label_source, label_target)


def write_manifest(
    output: Path,
    train: list[Item],
    val: list[Item],
    review_pending: list[Item],
) -> None:
    split_by_name = {
        **{item.image: "train" for item in train},
        **{item.image: "val" for item in val},
        **{item.image: "review_pending" for item in review_pending},
    }
    rows = sorted(train + val + review_pending, key=lambda item: item.image)
    with (output / "split_manifest.csv").open(
        "w", encoding="utf-8", newline=""
    ) as handle:
        writer = csv.writer(handle)
        writer.writerow(["image", "box_count", "source_status", "split"])
        for item in rows:
            writer.writerow(
                [item.image, item.box_count, item.status, split_by_name[item.image]]
            )


def write_dataset_yaml(output: Path) -> None:
    portable_root = output.resolve().as_posix()
    content = (
        f"path: {portable_root}\n"
        "train: images/train\n"
        "val: images/val\n"
        "test:\n"
        "\n"
        "names:\n"
        "  0: steel_ball\n"
    )
    (output / "steel_ball.yaml").write_text(content, encoding="utf-8")


def write_readme(
    output: Path,
    train: list[Item],
    val: list[Item],
    review_pending: list[Item],
) -> None:
    text = f"""# Steel-ball YOLO dataset

- Class: `steel_ball` (class id `0`)
- Train: {len(train)} reviewed images / {sum(x.box_count for x in train)} boxes
- Val: {len(val)} reviewed images / {sum(x.box_count for x in val)} boxes
- Review pending (excluded): {len(review_pending)} images / {sum(x.box_count for x in review_pending)} boxes
- Split seed: `230`

Only source rows marked `checked` are used for training and validation.
The `review_pending` directory is deliberately excluded until those dense or
blurred annotations have been manually reviewed.

`calibration/` contains representative training images for nncase PTQ.
Re-run `tools/prepare_yolo_dataset.py` after moving the project because
`steel_ball.yaml` stores an absolute dataset path.
"""
    (output / "README.md").write_text(text, encoding="utf-8")


def main() -> None:
    args = parse_args()
    source = args.source.resolve()
    output = args.output.resolve()
    items = load_items(source / "annotation_summary.csv")
    checked = [item for item in items if item.status == "checked"]
    review_pending = [
        item for item in items if item.status == "review_required"
    ]
    if len(items) != 52 or len(checked) != 40 or len(review_pending) != 12:
        raise AssertionError(
            "Unexpected summary: expected 52 total, 40 checked, 12 review_required"
        )

    train, val = stratified_split(checked, args.val_ratio, args.seed)
    ensure_new_output(output)

    for item in train:
        copy_item(source, output, item, "train")
    for item in val:
        copy_item(source, output, item, "val")
    for item in review_pending:
        copy_item(source, output, item, "review_pending")

    calibration = train.copy()
    random.Random(args.seed + 1).shuffle(calibration)
    calibration = calibration[: min(args.calibration_count, len(calibration))]
    calibration_dir = output / "calibration"
    calibration_dir.mkdir(parents=True, exist_ok=True)
    for item in sorted(calibration, key=lambda entry: entry.image):
        shutil.copy2(source / item.image, calibration_dir / item.image)

    write_manifest(output, train, val, review_pending)
    write_dataset_yaml(output)
    write_readme(output, train, val, review_pending)

    print(
        "PASS: "
        f"train={len(train)} images/{sum(x.box_count for x in train)} boxes, "
        f"val={len(val)} images/{sum(x.box_count for x in val)} boxes, "
        f"review_pending={len(review_pending)} images, "
        f"calibration={len(calibration)} images"
    )


if __name__ == "__main__":
    main()
