"""Synthetic camera scenes for offline detector development."""

from __future__ import annotations

from dataclasses import dataclass

import cv2
import numpy as np

from .target_model import A4_HEIGHT_MM, A4_WIDTH_MM, TARGET_CENTER_MM, render_target


@dataclass
class SyntheticGroundTruth:
    target_present: bool
    spot_present: bool
    target_center: tuple[float, float] | None
    spot_center: tuple[float, float] | None
    target_quad: np.ndarray | None


def _random_quad(rng: np.random.Generator, width: int, height: int) -> np.ndarray:
    target_height = float(rng.uniform(400.0, 610.0))
    target_width = target_height * A4_WIDTH_MM / A4_HEIGHT_MM
    margin_x = target_width * 0.75
    margin_y = target_height * 0.52
    center = np.array(
        (
            rng.uniform(margin_x, width - margin_x),
            rng.uniform(margin_y, height - margin_y),
        ),
        dtype=np.float32,
    )
    half = np.array((target_width / 2.0, target_height / 2.0), dtype=np.float32)
    corners = np.array(
        ((-half[0], -half[1]), (half[0], -half[1]), (half[0], half[1]), (-half[0], half[1])),
        dtype=np.float32,
    )
    angle = np.deg2rad(rng.uniform(-22.0, 22.0))
    rotation = np.array(
        ((np.cos(angle), -np.sin(angle)), (np.sin(angle), np.cos(angle))),
        dtype=np.float32,
    )
    corners = corners @ rotation.T
    jitter = np.column_stack(
        (
            rng.uniform(-0.07, 0.07, 4) * target_width,
            rng.uniform(-0.05, 0.05, 4) * target_height,
        )
    ).astype(np.float32)
    return corners + center + jitter


def _project(point: np.ndarray, matrix: np.ndarray) -> tuple[float, float]:
    projected = cv2.perspectiveTransform(point.reshape(1, 1, 2).astype(np.float32), matrix)[0, 0]
    return float(projected[0]), float(projected[1])


def make_scene(
    seed: int,
    *,
    size: tuple[int, int] = (1280, 720),
    target_present: bool = True,
    spot_present: bool = True,
) -> tuple[np.ndarray, SyntheticGroundTruth]:
    rng = np.random.default_rng(seed)
    width, height = size
    base = float(rng.uniform(185, 235))
    x_gradient = np.linspace(-12, 12, width, dtype=np.float32)[None, :, None]
    scene = np.full((height, width, 3), base, dtype=np.float32) + x_gradient

    for _ in range(5):
        p1 = tuple(rng.integers((0, 0), (width, height)).tolist())
        p2 = tuple(rng.integers((0, 0), (width, height)).tolist())
        gray = int(rng.integers(120, 180))
        cv2.line(scene, p1, p2, (gray, gray, gray), int(rng.integers(1, 4)), cv2.LINE_AA)

    if not target_present:
        noise = rng.normal(0.0, 5.0, scene.shape)
        scene = np.clip(scene + noise, 0, 255).astype(np.uint8)
        return scene, SyntheticGroundTruth(False, False, None, None, None)

    target = render_target(3.0)
    source_quad = np.array(
        ((0, 0), (target.shape[1] - 1, 0), (target.shape[1] - 1, target.shape[0] - 1), (0, target.shape[0] - 1)),
        dtype=np.float32,
    )
    destination_quad = _random_quad(rng, width, height)
    transform = cv2.getPerspectiveTransform(source_quad, destination_quad)
    warped_target = cv2.warpPerspective(target, transform, (width, height), flags=cv2.INTER_LINEAR)
    warped_mask = cv2.warpPerspective(
        np.full(target.shape[:2], 255, dtype=np.uint8), transform, (width, height)
    )
    scene_u8 = np.clip(scene, 0, 255).astype(np.uint8)
    scene_u8[warped_mask > 0] = warped_target[warped_mask > 0]

    target_center_px = TARGET_CENTER_MM * 3.0
    target_center = _project(target_center_px, transform)
    spot_center: tuple[float, float] | None = None

    if spot_present:
        spot_mm = TARGET_CENTER_MM + np.array(
            (rng.uniform(-72.0, 72.0), rng.uniform(-105.0, 105.0)), dtype=np.float32
        )
        spot_px = spot_mm * 3.0
        spot_center = _project(spot_px, transform)
        spot_int = tuple(np.rint(spot_center).astype(int))
        radius = int(rng.integers(6, 11))
        cv2.circle(scene_u8, spot_int, radius + 5, (170, 0, 150), -1, cv2.LINE_AA)
        cv2.circle(scene_u8, spot_int, radius, (255, 0, 220), -1, cv2.LINE_AA)
        cv2.circle(scene_u8, spot_int, max(2, radius // 3), (255, 210, 255), -1, cv2.LINE_AA)

    alpha = float(rng.uniform(0.72, 1.16))
    beta = float(rng.uniform(-8.0, 10.0))
    scene_f = scene_u8.astype(np.float32) * alpha + beta
    scene_f += rng.normal(0.0, rng.uniform(1.0, 6.0), scene_f.shape)
    scene_u8 = np.clip(scene_f, 0, 255).astype(np.uint8)
    if rng.random() < 0.35:
        scene_u8 = cv2.GaussianBlur(scene_u8, (3, 3), 0)

    return scene_u8, SyntheticGroundTruth(
        True,
        spot_present,
        target_center,
        spot_center,
        destination_quad,
    )
