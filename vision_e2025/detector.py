"""Target-border, target-center, and violet laser-spot detector."""

from __future__ import annotations

from dataclasses import dataclass

import cv2
import numpy as np

from .target_model import (
    A4_HEIGHT_MM,
    A4_WIDTH_MM,
    TARGET_CENTER_MM,
    circle_reference_mm,
)


Point = tuple[float, float]


@dataclass
class DetectionResult:
    target_valid: bool = False
    spot_valid: bool = False
    target_center: Point | None = None
    spot_center: Point | None = None
    target_quad: np.ndarray | None = None
    scene_to_target: np.ndarray | None = None
    target_to_scene: np.ndarray | None = None
    center_error_px: Point | None = None
    rectified: np.ndarray | None = None
    rectified_px_per_mm: float = 2.0

    def circle_reference(self, phase: float) -> Point | None:
        if self.target_to_scene is None:
            return None
        point_mm = circle_reference_mm(phase) * self.rectified_px_per_mm
        point = cv2.perspectiveTransform(
            point_mm.reshape(1, 1, 2).astype(np.float32), self.target_to_scene
        )[0, 0]
        return float(point[0]), float(point[1])


def _order_quad(points: np.ndarray) -> np.ndarray:
    points = np.asarray(points, dtype=np.float32).reshape(4, 2)
    ordered = np.empty((4, 2), dtype=np.float32)
    sums = points.sum(axis=1)
    diffs = np.diff(points, axis=1).reshape(-1)
    ordered[0] = points[np.argmin(sums)]      # top-left
    ordered[2] = points[np.argmax(sums)]      # bottom-right
    ordered[1] = points[np.argmin(diffs)]     # top-right
    ordered[3] = points[np.argmax(diffs)]     # bottom-left
    return ordered


def _find_target_quad(image: np.ndarray) -> np.ndarray | None:
    hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
    black = cv2.inRange(hsv, (0, 0, 0), (179, 255, 85))
    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (7, 7))
    black = cv2.morphologyEx(black, cv2.MORPH_CLOSE, kernel, iterations=2)

    contours, _ = cv2.findContours(black, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    image_area = image.shape[0] * image.shape[1]
    candidates: list[tuple[float, np.ndarray]] = []

    for contour in contours:
        area = cv2.contourArea(contour)
        if area < image_area * 0.025:
            continue
        perimeter = cv2.arcLength(contour, True)
        approx = cv2.approxPolyDP(contour, 0.025 * perimeter, True)
        if len(approx) == 4 and cv2.isContourConvex(approx):
            quad = approx.reshape(4, 2).astype(np.float32)
        else:
            quad = cv2.boxPoints(cv2.minAreaRect(contour)).astype(np.float32)

        ordered = _order_quad(quad)
        width = 0.5 * (
            np.linalg.norm(ordered[1] - ordered[0])
            + np.linalg.norm(ordered[2] - ordered[3])
        )
        height = 0.5 * (
            np.linalg.norm(ordered[3] - ordered[0])
            + np.linalg.norm(ordered[2] - ordered[1])
        )
        if width < 30 or height < 30:
            continue
        aspect = max(width / height, height / width)
        expected = A4_HEIGHT_MM / A4_WIDTH_MM
        aspect_score = max(0.0, 1.0 - abs(aspect - expected) / expected)
        score = area * (0.5 + aspect_score)
        candidates.append((score, ordered))

    return max(candidates, key=lambda item: item[0])[1] if candidates else None


def _red_center(rectified: np.ndarray) -> np.ndarray | None:
    hsv = cv2.cvtColor(rectified, cv2.COLOR_BGR2HSV)
    red_low = cv2.inRange(hsv, (0, 90, 70), (12, 255, 255))
    red_high = cv2.inRange(hsv, (168, 90, 70), (179, 255, 255))
    red = cv2.bitwise_or(red_low, red_high)
    red = cv2.morphologyEx(red, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    moments = cv2.moments(red, binaryImage=True)
    if moments["m00"] < 30:
        return None
    return np.array(
        (moments["m10"] / moments["m00"], moments["m01"] / moments["m00"]),
        dtype=np.float32,
    )


def _violet_spot(rectified: np.ndarray) -> np.ndarray | None:
    hsv = cv2.cvtColor(rectified, cv2.COLOR_BGR2HSV)
    violet = cv2.inRange(hsv, (118, 70, 100), (169, 255, 255))
    violet = cv2.morphologyEx(violet, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    violet = cv2.morphologyEx(violet, cv2.MORPH_CLOSE, np.ones((5, 5), np.uint8))

    contours, _ = cv2.findContours(violet, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    best: tuple[float, np.ndarray] | None = None
    for contour in contours:
        area = cv2.contourArea(contour)
        if area < 4 or area > rectified.shape[0] * rectified.shape[1] * 0.01:
            continue
        perimeter = cv2.arcLength(contour, True)
        circularity = 4.0 * np.pi * area / (perimeter * perimeter + 1e-6)
        score = area * (0.4 + max(0.0, circularity))
        if best is None or score > best[0]:
            best = (score, contour)

    if best is None:
        return None
    moments = cv2.moments(best[1])
    if moments["m00"] <= 0:
        return None
    return np.array(
        (moments["m10"] / moments["m00"], moments["m01"] / moments["m00"]),
        dtype=np.float32,
    )


def detect(image: np.ndarray, rectified_px_per_mm: float = 2.0) -> DetectionResult:
    result = DetectionResult()
    quad = _find_target_quad(image)
    if quad is None:
        return result

    target_width = round(A4_WIDTH_MM * rectified_px_per_mm)
    target_height = round(A4_HEIGHT_MM * rectified_px_per_mm)
    canonical_corners = np.array(
        ((0, 0), (target_width - 1, 0), (target_width - 1, target_height - 1), (0, target_height - 1)),
        dtype=np.float32,
    )
    scene_to_target = cv2.getPerspectiveTransform(quad, canonical_corners)
    target_to_scene = cv2.getPerspectiveTransform(canonical_corners, quad)
    rectified = cv2.warpPerspective(image, scene_to_target, (target_width, target_height))

    red_center = _red_center(rectified)
    if red_center is None:
        red_center = np.array((target_width / 2.0, target_height / 2.0), dtype=np.float32)

    target_center = cv2.perspectiveTransform(
        red_center.reshape(1, 1, 2), target_to_scene
    )[0, 0]
    spot_rectified = _violet_spot(rectified)

    result.target_valid = True
    result.target_center = float(target_center[0]), float(target_center[1])
    result.target_quad = quad
    result.scene_to_target = scene_to_target
    result.target_to_scene = target_to_scene
    result.rectified = rectified
    result.rectified_px_per_mm = rectified_px_per_mm

    if spot_rectified is not None:
        spot_center = cv2.perspectiveTransform(
            spot_rectified.reshape(1, 1, 2), target_to_scene
        )[0, 0]
        result.spot_valid = True
        result.spot_center = float(spot_center[0]), float(spot_center[1])
        result.center_error_px = (
            float(target_center[0] - spot_center[0]),
            float(target_center[1] - spot_center[1]),
        )
    return result


def annotate(image: np.ndarray, result: DetectionResult, circle_phase: float = 0.0) -> np.ndarray:
    output = image.copy()
    if result.target_quad is not None:
        cv2.polylines(output, [np.rint(result.target_quad).astype(np.int32)], True, (0, 220, 0), 3)
    if result.target_center is not None:
        center = tuple(np.rint(result.target_center).astype(int))
        cv2.drawMarker(output, center, (0, 0, 255), cv2.MARKER_CROSS, 24, 3)
    if result.spot_center is not None:
        spot = tuple(np.rint(result.spot_center).astype(int))
        cv2.circle(output, spot, 10, (255, 0, 255), 3, cv2.LINE_AA)
        if result.target_center is not None:
            center = tuple(np.rint(result.target_center).astype(int))
            cv2.arrowedLine(output, spot, center, (0, 220, 255), 2, cv2.LINE_AA)
    reference = result.circle_reference(circle_phase)
    if reference is not None:
        ref = tuple(np.rint(reference).astype(int))
        cv2.drawMarker(output, ref, (0, 255, 255), cv2.MARKER_TILTED_CROSS, 20, 2)
    label = f"target={int(result.target_valid)} spot={int(result.spot_valid)}"
    if result.center_error_px is not None:
        label += f" err=({result.center_error_px[0]:.1f},{result.center_error_px[1]:.1f})px"
    cv2.putText(output, label, (20, 35), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (20, 20, 20), 4, cv2.LINE_AA)
    cv2.putText(output, label, (20, 35), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 255), 2, cv2.LINE_AA)
    return output
