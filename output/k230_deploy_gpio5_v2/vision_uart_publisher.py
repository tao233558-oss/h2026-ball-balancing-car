"""Jiangxie-style text UART publisher for the K230 vision loop.

Wire format:
    [ball,X,CONF,FLAGS]\r\n

X is the signed raw x pixel offset of the detected ball center. Only the
fields required by the B-board calibration and controller are transmitted.
"""

import time
from machine import FPIOA, UART


VISION_FLAG_VALID = 1 << 0
VISION_FLAG_CALIBRATED = 1 << 1
VISION_FLAG_IN_ROI = 1 << 2
VISION_FLAG_SINGLE_TARGET = 1 << 3
VISION_FLAG_MODEL_READY = 1 << 4
VISION_FLAG_CAMERA_READY = 1 << 5


class VisionUartPublisher:
    def __init__(
            self,
            tx_gpio=5,
            rx_gpio=6,
            uart_id=UART.UART2,
            baudrate=115200,
            target_period_ms=20,
            heartbeat_period_ms=100):
        self._fpioa = FPIOA()
        self._fpioa.set_function(tx_gpio, FPIOA.UART2_TXD)
        self._fpioa.set_function(rx_gpio, FPIOA.UART2_RXD)
        self._uart = UART(
            uart_id,
            baudrate=baudrate,
            bits=UART.EIGHTBITS,
            parity=UART.PARITY_NONE,
            stop=UART.STOPBITS_ONE,
            timeout=0,
        )
        self._period_ms = int(target_period_ms)
        self._heartbeat_ms = max(
            int(heartbeat_period_ms),
            self._period_ms,
        )
        self._last_send_ms = None
        self._x_px = 0
        self._confidence_percent = 0
        self._flags = 0
        self._dirty = True
        self._frame_id = 0
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

        next_x_px = int(x_mm) if detected else 0
        next_x_px = max(-32768, min(32767, next_x_px))
        next_confidence = (
            max(0, min(100, int(confidence_percent)))
            if detected else 0
        )

        if (
                next_x_px != self._x_px
                or next_confidence != self._confidence_percent
                or flags != self._flags):
            self._dirty = True

        self._x_px = next_x_px
        self._confidence_percent = next_confidence
        self._flags = flags
        self._frame_id = (self._frame_id + 1) & 0xFFFF

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
        """Send at most one complete text packet per main-loop call."""
        now_ms = time.ticks_ms() if now_ms is None else int(now_ms)
        if self._last_send_ms is not None:
            elapsed_ms = time.ticks_diff(now_ms, self._last_send_ms)
            if elapsed_ms < self._period_ms:
                return False
            if not self._dirty and elapsed_ms < self._heartbeat_ms:
                return False

        packet = (
            "[ball,%d,%d,%d]\r\n"
            % (self._x_px, self._confidence_percent, self._flags)
        ).encode()
        written = self._uart.write(packet)

        if written != len(packet):
            self.write_error_count += 1
            return False

        self._last_send_ms = now_ms
        self._dirty = False
        return True

    def deinit(self):
        if self._uart is not None:
            self._uart.deinit()
            self._uart = None
