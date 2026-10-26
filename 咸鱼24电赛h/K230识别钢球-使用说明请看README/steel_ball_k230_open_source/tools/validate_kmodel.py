#!/usr/bin/env python3
"""Compare an nncase KModel output with its source ONNX output."""

from __future__ import annotations

import argparse
import os
import site
import sys
from pathlib import Path

import numpy as np
import onnxruntime as ort
from PIL import Image


_DLL_DIRECTORY_HANDLES = []
if sys.platform == "win32":
    local_dotnet = Path(__file__).resolve().parents[1] / ".dotnet"
    if (local_dotnet / "dotnet.exe").is_file():
        os.environ["DOTNET_ROOT"] = str(local_dotnet)
        os.environ["PATH"] = (
            str(local_dotnet) + os.pathsep + os.environ.get("PATH", "")
        )
    for directory in site.getsitepackages():
        os.environ["PATH"] = directory + os.pathsep + os.environ.get("PATH", "")
        os.environ.setdefault("NNCASE_PLUGIN_PATH", directory)
        if hasattr(os, "add_dll_directory"):
            _DLL_DIRECTORY_HANDLES.append(os.add_dll_directory(directory))

import nncase  # noqa: E402


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("onnx", type=Path)
    parser.add_argument("kmodel", type=Path)
    parser.add_argument("image", type=Path)
    parser.add_argument("--imgsz", type=int, default=320)
    parser.add_argument("--minimum-cosine", type=float, default=0.98)
    return parser.parse_args()


def cosine_similarity(left: np.ndarray, right: np.ndarray) -> float:
    left = left.astype(np.float64, copy=False).reshape(-1)
    right = right.astype(np.float64, copy=False).reshape(-1)
    denominator = np.linalg.norm(left) * np.linalg.norm(right)
    if denominator == 0:
        return 1.0 if np.array_equal(left, right) else 0.0
    return float(np.dot(left, right) / denominator)


def main() -> int:
    args = parse_args()

    image = Image.open(args.image).convert("RGB")
    image = image.resize((args.imgsz, args.imgsz), Image.Resampling.BILINEAR)
    kmodel_input = np.asarray(image, dtype=np.uint8)
    kmodel_input = np.transpose(kmodel_input, (2, 0, 1))[None, ...]
    onnx_input = kmodel_input.astype(np.float32) / 255.0

    session = ort.InferenceSession(
        str(args.onnx), providers=["CPUExecutionProvider"]
    )
    onnx_output = session.run(
        None, {session.get_inputs()[0].name: onnx_input}
    )[0]

    simulator = nncase.Simulator()
    simulator.load_model(args.kmodel.read_bytes())
    input_dtype = simulator.get_input_desc(0).dtype
    simulator.set_input_tensor(
        0,
        nncase.RuntimeTensor.from_numpy(
            kmodel_input.astype(input_dtype, copy=False)
        ),
    )
    simulator.run()
    kmodel_output = simulator.get_output_tensor(0).to_numpy()

    print(f"ONNX output:   shape={onnx_output.shape}, dtype={onnx_output.dtype}")
    print(
        f"KModel output: shape={kmodel_output.shape}, "
        f"dtype={kmodel_output.dtype}"
    )

    if onnx_output.shape != kmodel_output.shape:
        print("FAIL: output shapes do not match")
        return 1
    if not np.isfinite(kmodel_output).all():
        print("FAIL: KModel output contains NaN or infinity")
        return 1

    difference = np.abs(
        onnx_output.astype(np.float64) - kmodel_output.astype(np.float64)
    )
    cosine = cosine_similarity(onnx_output, kmodel_output)
    print(f"Cosine similarity: {cosine:.8f}")
    print(f"Mean absolute error: {difference.mean():.8f}")
    print(f"Maximum absolute error: {difference.max():.8f}")

    if cosine < args.minimum_cosine:
        print(
            f"FAIL: cosine similarity is below "
            f"{args.minimum_cosine:.4f}"
        )
        return 1

    print("PASS: KModel output matches the ONNX output")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
