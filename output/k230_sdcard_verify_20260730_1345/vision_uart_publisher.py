"""Non-blocking VISION_SAMPLE publisher for the K230 YOLO11 main loop.

Copy this file and 01_协议/h2026_uart_protocol_v2.py into the same directory
on the SD card. Call update_measurement() after each YOLO result and poll()
once per main-loop iteration.
"""

import time
from machine import FPIOA, UART

from h2026_uart_protocol_v2 import (
    VISION_FLAG_CALIBRATED,
    VISION_FLAG_CAMERA_READY,
    VISION_FLAG_IN_ROI,
    VISION_FLAG_MODEL_READY,
    VISION_FLAG_SINGLE_TARGET,
    VISION_FLAG_VALID,
    pack_vision_sample,
)


class VisionUartPublisher:
    def __init__(
            self,
            tx_gpio=5,
            uart_id=UART.UART2,
            baudrate=115200,
            target_period_ms=20):
        self._fpioa = FPIOA()
        self._fpioa.set_function(tx_gpio, FPIOA.UART2_TXD)
        self._uart = UART(
            uart_id,
            baudrate=baudrate,
            bits=UART.EIGHTBITS,
            parity=UART.PARITY_NONE,
            stop=UART.STOPBITS_ONE,
            timeout=0,
        )
        self._period_ms = int(target_period_ms)
        self._last_send_ms = None
        self._sequence = 0
        self._frame_id = 0
        self._result_timestamp_ms = time.ticks_ms()
        self._x_mm = 0
        self._confidence_percent = 0
        self._flags = 0
        self._inference_ms = 0
        self.write_error_count = 0

    @property
    def frame_id(self):
        return self._frame_id

    def update_measurement(
            self,
            x_mm,
            confidence_percent,
            detected,
            calibrated,
            in_roi,
            single_target,
            model_ready=True,
            camera_ready=True,
            inference_ms=0,
            result_timestamp_ms=None):
        flags = 0
        if calibrated:
            flags |= VISION_FLAG_CALIBRATED
        if in_roi:
            flags |= VISION_FLAG_IN_ROI
        if single_target:
            flags |= VISION_FLAG_SINGLE_TARGET
        if model_ready:
            flags |= VISION_FLAG_MODEL_READY
        if camera_ready:
            flags |= VISION_FLAG_CAMERA_READY

        valid = (
            detected
            and calibrated
            and in_roi
            and single_target
            and model_ready
            and camera_ready
        )
        if valid:
            flags |= VISION_FLAG_VALID

        self._x_mm = int(x_mm) if detected else 0
        self._confidence_percent = (
            max(0, min(100, int(confidence_percent)))
            if detected else 0
        )
        self._flags = flags
        self._inference_ms = max(0, int(inference_ms))
        self._frame_id = (self._frame_id + 1) & 0xFFFF
        self._result_timestamp_ms = (
            time.ticks_ms()
            if result_timestamp_ms is None
            else int(result_timestamp_ms)
        )

    def mark_unavailable(self, model_ready, camera_ready):
        self.update_measurement(
            x_mm=0,
            confidence_percent=0,
            detected=False,
            calibrated=False,
            in_roi=False,
            single_target=False,
            model_ready=model_ready,
            camera_ready=camera_ready,
            inference_ms=0,
        )

    def poll(self, now_ms=None):
        """Send at most one packet and never catch up with a blocking burst."""
        now_ms = time.ticks_ms() if now_ms is None else int(now_ms)
        if (
                self._last_send_ms is not None
                and time.ticks_diff(now_ms, self._last_send_ms)
                < self._period_ms):
            return False

        sample_age_ms = max(
            0,
            time.ticks_diff(now_ms, self._result_timestamp_ms),
        )
        packet = pack_vision_sample(
            sequence=self._sequence,
            x_mm=self._x_mm,
            confidence_percent=self._confidence_percent,
            flags=self._flags,
            frame_id=self._frame_id,
            sample_age_ms=sample_age_ms,
            inference_ms=self._inference_ms,
        )
        written = self._uart.write(packet)
        self._sequence = (self._sequence + 1) & 0xFF
        self._last_send_ms = now_ms

        if written != len(packet):
            self.write_error_count += 1
            return False
        return True

    def deinit(self):
        if self._uart is not None:
            self._uart.deinit()
            self._uart = None
