"""Export a trained YOLOv8 detector to a static 320x320 ONNX model."""

from __future__ import annotations

import argparse
import os
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
ULTRALYTICS_CONFIG_DIR = PROJECT_ROOT / ".ultralytics"
MATPLOTLIB_CONFIG_DIR = PROJECT_ROOT / ".matplotlib"
ULTRALYTICS_CONFIG_DIR.mkdir(exist_ok=True)
MATPLOTLIB_CONFIG_DIR.mkdir(exist_ok=True)
os.environ.setdefault("YOLO_CONFIG_DIR", str(ULTRALYTICS_CONFIG_DIR))
os.environ.setdefault("MPLCONFIGDIR", str(MATPLOTLIB_CONFIG_DIR))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("weights", type=Path, help="Path to best.pt")
    parser.add_argument("--imgsz", type=int, default=320)
    parser.add_argument("--opset", type=int, default=12)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    weights = args.weights.resolve()
    if not weights.is_file():
        raise FileNotFoundError(weights)
    try:
        from ultralytics import YOLO
    except ImportError as exc:
        raise SystemExit(
            "Ultralytics is not installed. Run: "
            "python -m pip install -r requirements-train.txt"
        ) from exc

    model = YOLO(str(weights))
    exported = model.export(
        format="onnx",
        imgsz=args.imgsz,
        batch=1,
        dynamic=False,
        simplify=False,
        opset=args.opset,
    )
    exported_path = Path(str(exported)).resolve()
    if not exported_path.is_file():
        raise FileNotFoundError(f"ONNX export was not found: {exported_path}")
    print(f"ONNX: {exported_path}")


if __name__ == "__main__":
    main()
