"""H2026 UART protocol v2 shared by CanMV K230 and PC tests.

The module intentionally avoids Python-only features so the same file can run
under CanMV MicroPython. All multi-byte values are little-endian.
"""

try:
    import ustruct as struct
except ImportError:
    import struct


SOF0 = 0xAA
SOF1 = 0x55
VERSION = 0x02
MAX_PAYLOAD = 32
FRAME_OVERHEAD = 8
MAX_FRAME_SIZE = MAX_PAYLOAD + FRAME_OVERHEAD
DEFAULT_INTER_BYTE_TIMEOUT_MS = 5

MSG_VISION_SAMPLE = 0x10
MSG_B_STATUS = 0x20
MSG_A_COMMAND = 0x30

VISION_PAYLOAD_SIZE = 10
B_STATUS_PAYLOAD_SIZE = 26
A_COMMAND_PAYLOAD_SIZE = 10

# VISION_SAMPLE flags.
VISION_FLAG_VALID = 1 << 0
VISION_FLAG_CALIBRATED = 1 << 1
VISION_FLAG_IN_ROI = 1 << 2
VISION_FLAG_SINGLE_TARGET = 1 << 3
VISION_FLAG_MODEL_READY = 1 << 4
VISION_FLAG_CAMERA_READY = 1 << 5

# B_STATUS flags.
B_FLAG_BALL_VALID = 1 << 0
B_FLAG_LINE_VALID = 1 << 1
B_FLAG_ACTUATOR_REFERENCE_VALID = 1 << 2
# Compatibility name retained while v2 is being adopted.
B_FLAG_ACTUATOR_HOMED = B_FLAG_ACTUATOR_REFERENCE_VALID
B_FLAG_CONTROL_ACTIVE = 1 << 3
B_FLAG_VISION_LINK_OK = 1 << 4
B_FLAG_A_LINK_OK = 1 << 5
B_FLAG_HOME_SWITCH_ACTIVE = 1 << 6

# A_COMMAND modes.
A_MODE_SAFE_IDLE = 0
A_MODE_HOME = 1
A_MODE_BALANCE = 2
A_MODE_LEVEL_HOLD = 3
A_MODE_DIAGNOSTIC = 4

# A_COMMAND flags.
A_FLAG_ENABLE = 1 << 0
A_FLAG_ESTOP = 1 << 1
A_FLAG_CLEAR_FAULT = 1 << 2
A_FLAG_ACCEL_VALID = 1 << 3

# B state values.
B_STATE_BOOT = 0
B_STATE_HOMING = 1
B_STATE_READY = 2
B_STATE_ACTIVE = 3
B_STATE_DEGRADED = 4
B_STATE_FAULT = 5
B_STATE_ESTOP = 6

# B_STATUS fault_bits.
B_FAULT_VISION_LINK_TIMEOUT = 1 << 0
B_FAULT_BALL_LOST = 1 << 1
B_FAULT_A_COMMAND_TIMEOUT = 1 << 2
B_FAULT_REFERENCE_INVALID = 1 << 3
B_FAULT_REFERENCE_INIT_TIMEOUT = 1 << 4
# Compatibility names retained for the earlier v2 draft.
B_FAULT_NOT_HOMED = B_FAULT_REFERENCE_INVALID
B_FAULT_HOME_TIMEOUT = B_FAULT_REFERENCE_INIT_TIMEOUT
B_FAULT_ACTUATOR_SOFT_LIMIT = 1 << 5
B_FAULT_ACTUATOR_ENDSTOP = 1 << 6
B_FAULT_ENDSTOP_CONFLICT = 1 << 7
B_FAULT_LINE_INVALID = 1 << 8
B_FAULT_UART_RX_OVERFLOW = 1 << 9
B_FAULT_PROTOCOL_ERRORS = 1 << 10
B_FAULT_ESTOP_ACTIVE = 1 << 11
B_FAULT_STEPPER_POSITION_UNCERTAIN = 1 << 12


def _clamp(value, low, high):
    return min(high, max(low, int(value)))


def _ticks_diff(now_ms, before_ms):
    try:
        import time
        return time.ticks_diff(now_ms, before_ms)
    except (ImportError, AttributeError):
        return int(now_ms) - int(before_ms)


def crc16_ccitt_false(data):
    crc = 0xFFFF
    for value in data:
        crc ^= int(value) << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def pack_frame(message_type, sequence, payload=b""):
    payload = bytes(payload)
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload exceeds 32 bytes")

    body = bytes((
        VERSION,
        int(message_type) & 0xFF,
        int(sequence) & 0xFF,
        len(payload),
    )) + payload
    crc = crc16_ccitt_false(body)
    return (
        bytes((SOF0, SOF1))
        + body
        + bytes((crc & 0xFF, (crc >> 8) & 0xFF))
    )


def pack_vision_sample(
        sequence,
        x_mm,
        confidence_percent,
        flags,
        frame_id,
        sample_age_ms,
        inference_ms):
    payload = struct.pack(
        "<hBBHHH",
        _clamp(x_mm, -32768, 32767),
        _clamp(confidence_percent, 0, 100),
        int(flags) & 0xFF,
        int(frame_id) & 0xFFFF,
        _clamp(sample_age_ms, 0, 0xFFFF),
        _clamp(inference_ms, 0, 0xFFFF),
    )
    return pack_frame(MSG_VISION_SAMPLE, sequence, payload)


def unpack_vision_sample(payload):
    if len(payload) != VISION_PAYLOAD_SIZE:
        raise ValueError("VISION_SAMPLE payload must contain 10 bytes")
    values = struct.unpack("<hBBHHH", payload)
    return {
        "x_mm": values[0],
        "confidence_percent": values[1],
        "flags": values[2],
        "frame_id": values[3],
        "sample_age_ms": values[4],
        "inference_ms": values[5],
    }


def pack_a_command(
        sequence,
        mode,
        flags,
        target_x_mm,
        vehicle_speed_ref_mm_s,
        vehicle_accel_ref_mm_s2,
        command_id):
    payload = struct.pack(
        "<BBhhhH",
        int(mode) & 0xFF,
        int(flags) & 0xFF,
        _clamp(target_x_mm, -32768, 32767),
        _clamp(vehicle_speed_ref_mm_s, -32768, 32767),
        _clamp(vehicle_accel_ref_mm_s2, -32768, 32767),
        int(command_id) & 0xFFFF,
    )
    return pack_frame(MSG_A_COMMAND, sequence, payload)


def unpack_a_command(payload):
    if len(payload) != A_COMMAND_PAYLOAD_SIZE:
        raise ValueError("A_COMMAND payload must contain 10 bytes")
    values = struct.unpack("<BBhhhH", payload)
    return {
        "mode": values[0],
        "flags": values[1],
        "target_x_mm": values[2],
        "vehicle_speed_ref_mm_s": values[3],
        "vehicle_accel_ref_mm_s2": values[4],
        "command_id": values[5],
    }


def pack_b_status(
        sequence,
        state,
        flags,
        ball_x_mm,
        ball_error_mm,
        actuator_position_steps,
        control_output_permille,
        line_error,
        line_bits,
        fault_bits,
        vision_rx_age_ms,
        ball_valid_age_ms,
        command_rx_age_ms,
        command_id_echo):
    payload = struct.pack(
        "<BBhhihhBBHHHHH",
        int(state) & 0xFF,
        int(flags) & 0xFF,
        _clamp(ball_x_mm, -32768, 32767),
        _clamp(ball_error_mm, -32768, 32767),
        _clamp(actuator_position_steps, -2147483648, 2147483647),
        _clamp(control_output_permille, -1000, 1000),
        _clamp(line_error, -32768, 32767),
        int(line_bits) & 0xFF,
        0,
        int(fault_bits) & 0xFFFF,
        _clamp(vision_rx_age_ms, 0, 0xFFFF),
        _clamp(ball_valid_age_ms, 0, 0xFFFF),
        _clamp(command_rx_age_ms, 0, 0xFFFF),
        int(command_id_echo) & 0xFFFF,
    )
    return pack_frame(MSG_B_STATUS, sequence, payload)


def unpack_b_status(payload):
    if len(payload) != B_STATUS_PAYLOAD_SIZE:
        raise ValueError("B_STATUS payload must contain 26 bytes")
    values = struct.unpack("<BBhhihhBBHHHHH", payload)
    return {
        "state": values[0],
        "flags": values[1],
        "ball_x_mm": values[2],
        "ball_error_mm": values[3],
        "actuator_position_steps": values[4],
        "control_output_permille": values[5],
        "line_error": values[6],
        "line_bits": values[7],
        "fault_bits": values[9],
        "vision_rx_age_ms": values[10],
        "ball_valid_age_ms": values[11],
        "command_rx_age_ms": values[12],
        "command_id_echo": values[13],
    }


class FrameParser:
    """Noise-tolerant streaming parser with sliding resynchronization."""

    def __init__(self, inter_byte_timeout_ms=DEFAULT_INTER_BYTE_TIMEOUT_MS):
        self.buffer = bytearray()
        self.inter_byte_timeout_ms = int(inter_byte_timeout_ms)
        self.last_byte_ms = None
        self.bad_frame_count = 0

    def reset(self):
        self.buffer = bytearray()
        self.last_byte_ms = None

    def _discard_until_header(self):
        while len(self.buffer) >= 2:
            if self.buffer[0] == SOF0 and self.buffer[1] == SOF1:
                return
            del self.buffer[0]

        if self.buffer and self.buffer[0] != SOF0:
            self.buffer = bytearray()

    def _extract_one(self):
        while True:
            self._discard_until_header()
            if len(self.buffer) < 6:
                return None

            payload_length = self.buffer[5]
            if payload_length > MAX_PAYLOAD:
                del self.buffer[0]
                self.bad_frame_count += 1
                continue

            expected_size = payload_length + FRAME_OVERHEAD
            if len(self.buffer) < expected_size:
                return None

            raw = bytes(self.buffer[:expected_size])
            received_crc = raw[-2] | (raw[-1] << 8)
            valid = (
                raw[2] == VERSION
                and crc16_ccitt_false(raw[2:-2]) == received_crc
            )
            if not valid:
                # Shift by one byte, then search again. This recovers valid
                # frames embedded after a plausible but false long header.
                del self.buffer[0]
                self.bad_frame_count += 1
                continue

            del self.buffer[:expected_size]
            return {
                "version": raw[2],
                "message_type": raw[3],
                "sequence": raw[4],
                "payload": raw[6:-2],
            }

    def feed_byte(self, value, now_ms=None):
        if (
                now_ms is not None
                and self.buffer
                and self.last_byte_ms is not None
                and _ticks_diff(now_ms, self.last_byte_ms)
                > self.inter_byte_timeout_ms):
            self.reset()

        if now_ms is not None:
            self.last_byte_ms = int(now_ms)

        self.buffer.append(int(value) & 0xFF)
        if len(self.buffer) > MAX_FRAME_SIZE * 2:
            del self.buffer[:-MAX_FRAME_SIZE]
            self.bad_frame_count += 1
        return self._extract_one()

    def feed(self, data, now_ms=None):
        frames = []
        for value in data:
            frame = self.feed_byte(value, now_ms=now_ms)
            if frame is not None:
                frames.append(frame)

        # A single append can leave another complete frame buffered after
        # resynchronization, so drain it before returning.
        while True:
            frame = self._extract_one()
            if frame is None:
                break
            frames.append(frame)
        return frames
