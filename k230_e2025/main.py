"""CanMV-K230 / Lushan board template for the 2025 E problem.

Derived from the user's existing lessons 04, 12, 13, and 15.  This file is
syntax-checked on the PC, but its color thresholds and pins require real-board
calibration before use.
"""

import os
import time

from machine import FPIOA, Pin, UART
from media.display import Display
from media.media import MediaManager
from media.sensor import CAM_CHN_ID_0, Sensor

from aim_protocol import (
    FLAG_CIRCLE_COMPLETE,
    FLAG_SPOT_VALID,
    FLAG_TARGET_VALID,
    make_packet,
)
from target_geometry import (
    circle_reference,
    order_corners,
    point_distance,
    target_bounding_roi,
    target_center,
)


# FPIOA uses GPIO numbers, not the 40-pin header position numbers.
# Header pin 11 = GPIO5/UART2_TX; header pin 13 = GPIO6/UART2_RX.
UART_TX_GPIO = 5
UART_RX_GPIO = 6
# Header pin 40 = GPIO33. Automatic laser output stays disabled below.
LASER_GPIO_PIN = 33
UART_BAUD = 115200

FRAME_WIDTH = 1024
FRAME_HEIGHT = 768
CAMERA_ROI = (192, 64, 640, 640)

# LAB thresholds are placeholders based on the old laser-spot project.  A real
# 405 nm spot must be sampled again under the final exposure and target paper.
LASER_THRESHOLDS = [
    (20, 100, 10, 127, -128, 45),
]
# Starting value from the bundled CanMV red-blob example. It must be sampled
# again under the final camera exposure and printed target.
RED_TARGET_THRESHOLDS = [
    (0, 80, 40, 100, 10, 100),
]
BLACK_THRESHOLD = [(0, 85)]

MODE_STOP = 0
MODE_CENTER = 1
MODE_CIRCLE = 2
run_mode = MODE_STOP

# Keep false until the laser wiring, polarity, and eye-safety procedure are
# checked.  With false, this program never turns on GPIO33.
AUTO_LASER_ENABLE = False
CIRCLE_PHASE_STEP = 2

sensor = None
uart2 = None
laser_pin = None
rx_buffer = b""
sequence = 0
circle_phase = 0


def find_target(image):
    gray = image.to_grayscale(copy=True)
    binary = gray.binary(BLACK_THRESHOLD)
    rects = binary.find_rects(threshold=10000)
    best = None
    best_area = 0

    if rects is None:
        return None

    for rect in rects:
        corners = order_corners(rect.corners())
        width = 0.5 * (
            point_distance(corners[0], corners[1])
            + point_distance(corners[3], corners[2])
        )
        height = 0.5 * (
            point_distance(corners[0], corners[3])
            + point_distance(corners[1], corners[2])
        )
        area = width * height
        if width < 80 or height < 120:
            continue
        ratio = width / height
        if ratio < 0.40 or ratio > 1.10:
            continue
        if area > best_area:
            best = corners
            best_area = area
    return best


def find_target_center(image, corners):
    """Prefer the red target centroid, with projective A4 center fallback."""
    fallback = target_center(corners)
    if fallback is None:
        return None

    roi = target_bounding_roi(corners, image.width(), image.height())
    blobs = image.find_blobs(
        RED_TARGET_THRESHOLDS,
        roi=roi,
        pixels_threshold=6,
        area_threshold=6,
        merge=False,
    )
    if blobs is None or len(blobs) == 0:
        return fallback

    total_pixels = 0
    weighted_x = 0.0
    weighted_y = 0.0
    for blob in blobs:
        pixels = blob.pixels()
        total_pixels += pixels
        weighted_x += blob.cx() * pixels
        weighted_y += blob.cy() * pixels
    if total_pixels <= 0:
        return fallback

    detected = (weighted_x / total_pixels, weighted_y / total_pixels)
    # Reject unrelated red objects. Printed rings should remain close to the
    # projective A4 center even under perspective.
    max_offset = 0.18 * min(roi[2], roi[3])
    if point_distance(detected, fallback) > max_offset:
        return fallback
    return detected


def find_laser_spot(image, corners):
    roi = target_bounding_roi(corners, image.width(), image.height())
    blobs = image.find_blobs(
        LASER_THRESHOLDS,
        roi=roi,
        x_stride=2,
        y_stride=2,
        pixels_threshold=8,
        area_threshold=8,
        merge=False,
    )
    if blobs is None or len(blobs) == 0:
        return None
    max_pixels = max(16, int(roi[2] * roi[3] * 0.02))
    candidates = [blob for blob in blobs if blob.pixels() <= max_pixels]
    if len(candidates) == 0:
        return None
    blob = max(candidates, key=lambda item: item.pixels())
    return float(blob.cx()), float(blob.cy())


def read_command():
    global rx_buffer, run_mode, circle_phase
    data = uart2.read()
    if data is None:
        return
    rx_buffer += data
    while b"\n" in rx_buffer:
        line, rx_buffer = rx_buffer.split(b"\n", 1)
        command = line.strip().upper()
        if command == b"STOP":
            run_mode = MODE_STOP
        elif command == b"CENTER":
            run_mode = MODE_CENTER
        elif command == b"CIRCLE":
            if run_mode != MODE_CIRCLE:
                circle_phase = 0
            run_mode = MODE_CIRCLE


def set_laser(enable):
    if laser_pin is None:
        return
    laser_pin.value(1 if (AUTO_LASER_ENABLE and enable) else 0)


def draw_debug(image, corners, target, spot):
    if corners is not None:
        for index in range(4):
            point_a = corners[index]
            point_b = corners[(index + 1) % 4]
            image.draw_line(
                int(point_a[0]), int(point_a[1]), int(point_b[0]), int(point_b[1]),
                color=(0, 255, 0), thickness=3,
            )
    if target is not None:
        target_x = int(target[0])
        target_y = int(target[1])
        image.draw_line(target_x - 12, target_y, target_x + 12, target_y, color=(255, 0, 0), thickness=2)
        image.draw_line(target_x, target_y - 12, target_x, target_y + 12, color=(255, 0, 0), thickness=2)
    if spot is not None:
        image.draw_circle(int(spot[0]), int(spot[1]), 8, color=(255, 0, 255), thickness=2)


try:
    fpioa = FPIOA()
    fpioa.set_function(UART_TX_GPIO, FPIOA.UART2_TXD)
    fpioa.set_function(UART_RX_GPIO, FPIOA.UART2_RXD)
    fpioa.set_function(LASER_GPIO_PIN, FPIOA.GPIO33)
    uart2 = UART(UART.UART2, UART_BAUD)
    laser_pin = Pin(LASER_GPIO_PIN, Pin.OUT)
    laser_pin.value(0)

    sensor = Sensor(width=FRAME_WIDTH, height=FRAME_HEIGHT)
    sensor.reset()
    sensor.set_framesize(width=FRAME_WIDTH, height=FRAME_HEIGHT)
    sensor.set_pixformat(Sensor.RGB565)
    Display.init(Display.ST7701, width=800, height=480, to_ide=True)
    MediaManager.init()
    sensor.run()
    clock = time.clock()

    while True:
        clock.tick()
        os.exitpoint()
        read_command()

        frame = sensor.snapshot(chn=CAM_CHN_ID_0)
        image = frame.copy(roi=CAMERA_ROI)
        corners = find_target(image)
        spot = None
        target = None
        flags = 0
        circle_complete = False

        if corners is not None:
            if run_mode == MODE_CIRCLE:
                target = circle_reference(corners, circle_phase)
            else:
                target = find_target_center(image, corners)
            if target is not None:
                flags |= FLAG_TARGET_VALID
            spot = find_laser_spot(image, corners)
            if spot is not None:
                flags |= FLAG_SPOT_VALID

        if run_mode == MODE_CIRCLE and corners is not None:
            next_phase = circle_phase + CIRCLE_PHASE_STEP
            if next_phase >= 256:
                next_phase -= 256
                circle_complete = True
            circle_phase = next_phase
        if circle_complete:
            flags |= FLAG_CIRCLE_COMPLETE

        set_laser(run_mode != MODE_STOP and corners is not None)
        tx = int(target[0]) if target is not None else 0
        ty = int(target[1]) if target is not None else 0
        sx = int(spot[0]) if spot is not None else 0
        sy = int(spot[1]) if spot is not None else 0
        uart2.write(make_packet(sequence, flags, tx, ty, sx, sy, circle_phase))
        sequence = (sequence + 1) & 0xFF

        draw_debug(image, corners, target, spot)
        image.draw_string_advanced(8, 8, 22, "mode:{} fps:{:.1f}".format(run_mode, clock.fps()), color=(255, 255, 0))
        if image.height() > 480 or image.width() > 800:
            image.midpoint_pool(2, 2)
        image.compress_for_ide()
        Display.show_image(image, x=(800 - image.width()) // 2, y=(480 - image.height()) // 2)

except KeyboardInterrupt as error:
    print("user stop:", error)
except BaseException as error:
    print("error:", error)
finally:
    if laser_pin is not None:
        laser_pin.value(0)
    if isinstance(sensor, Sensor):
        sensor.stop()
    Display.deinit()
    os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
    time.sleep_ms(100)
    MediaManager.deinit()
    if uart2 is not None:
        uart2.deinit()
