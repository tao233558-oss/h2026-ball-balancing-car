"""Audit available model artifacts without implying missing YOLO11 is present."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "咸鱼24电赛h/K230识别钢球-使用说明请看README/steel_ball_k230_open_source/models"
EXPECTED = {
    "steel_ball_yolov8n_320.pt": "4f7a83b4449caae8804fa5ba906ca8637355deacf8fa665b61ec9e37e226f991",
    "steel_ball_yolov8n_320.onnx": "8c171d6a13e77f95d5012d004a0286d0d41bfb6c9d37e92a681a9e715ed5ba8b",
    "steel_ball_yolov8n_320.kmodel": "dd5ea6aa80280733f93043ac0691ac09625a4dcf66ba6fa47bd28e0391e2c7a6",
}


def audit():
    records = []
    for filename, expected in EXPECTED.items():
        path = REFERENCE / filename
        actual = hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None
        records.append({"path": path.relative_to(ROOT).as_posix(), "present": actual is not None,
                        "sha256": actual, "matches_reference": actual == expected})
    yolo11 = ROOT / "vision/models/yolo11_gq_320.kmodel"
    return {"reference_yolov8": records, "yolo11": {
        "path": yolo11.relative_to(ROOT).as_posix(), "present": yolo11.is_file(),
        "sha256": hashlib.sha256(yolo11.read_bytes()).hexdigest() if yolo11.is_file() else None,
        "board_verified": False,
    }}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--require-yolo11", action="store_true")
    args = parser.parse_args()
    result = audit()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    if not all(item["matches_reference"] for item in result["reference_yolov8"]):
        return 1
    if args.require_yolo11 and not result["yolo11"]["present"]:
        print("YOLO11 model missing: restore the matching model from the teammate's TF backup.")
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
