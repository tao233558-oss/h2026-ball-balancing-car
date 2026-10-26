"""MicroPython-compatible A4 target geometry for the K230 vision loop."""

import math


A4_WIDTH_MM = 210.0
A4_HEIGHT_MM = 297.0
TARGET_CENTER_X_MM = A4_WIDTH_MM / 2.0
TARGET_CENTER_Y_MM = A4_HEIGHT_MM / 2.0
CIRCLE_RADIUS_MM = 60.0


def order_corners(corners):
    """Return top-left, top-right, bottom-right, bottom-left."""
    points = [(float(point[0]), float(point[1])) for point in corners]
    top_left = min(points, key=lambda point: point[0] + point[1])
    bottom_right = max(points, key=lambda point: point[0] + point[1])
    top_right = max(points, key=lambda point: point[0] - point[1])
    bottom_left = min(points, key=lambda point: point[0] - point[1])
    return [top_left, top_right, bottom_right, bottom_left]


def point_distance(point_a, point_b):
    dx = point_a[0] - point_b[0]
    dy = point_a[1] - point_b[1]
    return math.sqrt(dx * dx + dy * dy)


def _unit_square_coefficients(corners):
    """Projective map coefficients from a unit square to an ordered quad."""
    x0, y0 = corners[0]
    x1, y1 = corners[1]
    x2, y2 = corners[2]
    x3, y3 = corners[3]
    dx1 = x1 - x2
    dx2 = x3 - x2
    dx3 = x0 - x1 + x2 - x3
    dy1 = y1 - y2
    dy2 = y3 - y2
    dy3 = y0 - y1 + y2 - y3
    denominator = dx1 * dy2 - dx2 * dy1

    if abs(denominator) < 1.0e-9:
        g = 0.0
        h = 0.0
    else:
        g = (dx3 * dy2 - dx2 * dy3) / denominator
        h = (dx1 * dy3 - dx3 * dy1) / denominator

    return (
        x1 - x0 + g * x1,
        x3 - x0 + h * x3,
        x0,
        y1 - y0 + g * y1,
        y3 - y0 + h * y3,
        y0,
        g,
        h,
    )


def map_a4_point(corners, x_mm, y_mm):
    """Map one physical A4 coordinate into the camera quadrilateral."""
    ordered = order_corners(corners)
    a, b, c, d, e, f, g, h = _unit_square_coefficients(ordered)
    u = float(x_mm) / A4_WIDTH_MM
    v = float(y_mm) / A4_HEIGHT_MM
    scale = g * u + h * v + 1.0
    if abs(scale) < 1.0e-9:
        return None
    return ((a * u + b * v + c) / scale,
            (d * u + e * v + f) / scale)


def target_center(corners):
    return map_a4_point(corners, TARGET_CENTER_X_MM, TARGET_CENTER_Y_MM)


def circle_reference(corners, phase_byte, radius_mm=CIRCLE_RADIUS_MM):
    phase = 2.0 * math.pi * (phase_byte & 0xFF) / 256.0
    x_mm = TARGET_CENTER_X_MM + math.cos(phase) * radius_mm
    y_mm = TARGET_CENTER_Y_MM + math.sin(phase) * radius_mm
    return map_a4_point(corners, x_mm, y_mm)


def target_bounding_roi(corners, image_width, image_height):
    ordered = order_corners(corners)
    x_min = max(0, int(min(point[0] for point in ordered)))
    y_min = max(0, int(min(point[1] for point in ordered)))
    x_max = min(image_width - 1, int(max(point[0] for point in ordered)))
    y_max = min(image_height - 1, int(max(point[1] for point in ordered)))
    return x_min, y_min, max(1, x_max - x_min + 1), max(1, y_max - y_min + 1)
