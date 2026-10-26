"""Train the K230-targeted YOLOv8n steel-ball detector."""

from __future__ import annotations

import argparse
import os
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DATA = PROJECT_ROOT / "steel_ball_yolo" / "steel_ball.yaml"
ULTRALYTICS_CONFIG_DIR = PROJECT_ROOT / ".ultralytics"
MATPLOTLIB_CONFIG_DIR = PROJECT_ROOT / ".matplotlib"
ULTRALYTICS_CONFIG_DIR.mkdir(exist_ok=True)
MATPLOTLIB_CONFIG_DIR.mkdir(exist_ok=True)
os.environ.setdefault("YOLO_CONFIG_DIR", str(ULTRALYTICS_CONFIG_DIR))
os.environ.setdefault("MPLCONFIGDIR", str(MATPLOTLIB_CONFIG_DIR))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--data",
        type=Path,
        default=DEFAULT_DATA,
    )
    parser.add_argument("--model", default="yolov8n.pt")
    parser.add_argument("--epochs", type=int, default=300)
    parser.add_argument("--imgsz", type=int, default=320)
    parser.add_argument("--batch", type=int, default=8)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument(
        "--device",
        default=None,
        help="Examples: 0 for the first NVIDIA GPU, or cpu",
    )
    parser.add_argument("--name", default="yolov8n_320")
    parser.add_argument("--resume", type=Path)
    return parser.parse_args()


def materialize_portable_data_yaml(data_path: Path) -> Path:
    """Create an absolute runtime YAML for the checked-in portable dataset."""
    if data_path.resolve() != DEFAULT_DATA.resolve():
        return data_path

    try:
        import yaml
    except ImportError as exc:
        raise SystemExit(
            "PyYAML is required. Run: python -m pip install "
            "-r requirements-train.txt"
        ) from exc

    payload = yaml.safe_load(data_path.read_text(encoding="utf-8"))
    payload["path"] = str(data_path.parent.resolve())
    runtime_dir = PROJECT_ROOT / ".runtime"
    runtime_dir.mkdir(exist_ok=True)
    runtime_yaml = runtime_dir / "steel_ball.runtime.yaml"
    runtime_yaml.write_text(
        yaml.safe_dump(payload, sort_keys=False, allow_unicode=True),
        encoding="utf-8",
    )
    return runtime_yaml


def main() -> None:
    args = parse_args()
    try:
        from ultralytics import YOLO
    except ImportError as exc:
        raise SystemExit(
            "Ultralytics is not installed. Use Python 3.10/3.11 and run: "
            "python -m pip install -r requirements-train.txt"
        ) from exc

    if args.resume:
        model = YOLO(str(args.resume.resolve()))
        model.train(resume=True)
    else:
        data_path = args.data.resolve()
        if not data_path.is_file():
            raise FileNotFoundError(data_path)
        data_path = materialize_portable_data_yaml(data_path)
        model = YOLO(args.model)
        train_args = {
            "data": str(data_path),
            "epochs": args.epochs,
            "imgsz": args.imgsz,
            "batch": args.batch,
            "workers": args.workers,
            "project": str(PROJECT_ROOT / "runs" / "steel_ball"),
            "name": args.name,
            "seed": 230,
            "deterministic": True,
            "patience": 80,
            "close_mosaic": 15,
            "degrees": 20.0,
            "translate": 0.12,
            "scale": 0.55,
            "fliplr": 0.5,
            "flipud": 0.2,
            "hsv_h": 0.015,
            "hsv_s": 0.45,
            "hsv_v": 0.35,
        }
        if args.device is not None:
            train_args["device"] = args.device
        model.train(**train_args)

    if model.trainer is None:
        raise RuntimeError("Ultralytics training finished without a trainer state")
    save_dir = Path(model.trainer.save_dir)
    best = save_dir / "weights" / "best.pt"
    print(f"Training output: {save_dir}")
    print(f"Best weights: {best}")
    if not best.is_file():
        raise FileNotFoundError(f"Ultralytics did not create {best}")


if __name__ == "__main__":
    main()
