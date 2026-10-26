"""Physical model and canonical rendering of the E-problem target."""

from __future__ import annotations

import cv2
import numpy as np


A4_WIDTH_MM = 210.0
A4_HEIGHT_MM = 297.0
TARGET_CENTER_MM = np.array((A4_WIDTH_MM / 2.0, A4_HEIGHT_MM / 2.0), dtype=np.float32)
CIRCLE_RADII_MM = (20.0, 40.0, 60.0, 80.0, 100.0)
BLACK_TAPE_WIDTH_MM = 18.0
RED_LINE_WIDTH_MM = 1.0
CENTER_DOT_DIAMETER_MM = 1.0


def canonical_size(px_per_mm: float = 3.0) -> tuple[int, int]:
    return round(A4_WIDTH_MM * px_per_mm), round(A4_HEIGHT_MM * px_per_mm)


def mm_to_pixel(point_mm: tuple[float, float] | np.ndarray, px_per_mm: float) -> np.ndarray:
    return np.asarray(point_mm, dtype=np.float32) * float(px_per_mm)


def render_target(px_per_mm: float = 3.0) -> np.ndarray:
    """Render the literal target geometry to a BGR image."""
    width, height = canonical_size(px_per_mm)
    image = np.full((height, width, 3), 255, dtype=np.uint8)

    tape = max(1, round(BLACK_TAPE_WIDTH_MM * px_per_mm))
    image[:tape, :] = 0
    image[-tape:, :] = 0
    image[:, :tape] = 0
    image[:, -tape:] = 0

    center = tuple(np.rint(mm_to_pixel(TARGET_CENTER_MM, px_per_mm)).astype(int))
    red = (0, 0, 220)
    thickness = max(1, round(RED_LINE_WIDTH_MM * px_per_mm))
    for radius_mm in CIRCLE_RADII_MM:
        cv2.circle(
            image,
            center,
            round(radius_mm * px_per_mm),
            red,
            thickness,
            lineType=cv2.LINE_AA,
        )

    cv2.circle(
        image,
        center,
        max(1, round(CENTER_DOT_DIAMETER_MM * px_per_mm / 2.0)),
        red,
        -1,
        lineType=cv2.LINE_AA,
    )
    return image


def circle_reference_mm(phase: float, radius_mm: float = 60.0) -> np.ndarray:
    """Desired point on the 6 cm circle; phase is in turns [0, 1)."""
    angle = 2.0 * np.pi * float(phase)
    offset = np.array((np.cos(angle), np.sin(angle)), dtype=np.float32) * radius_mm
    return TARGET_CENTER_MM + offset

