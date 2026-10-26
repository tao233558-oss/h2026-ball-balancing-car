"""Shared H2026 UART framing for CanMV K230 and host-side tests."""

try:
    import ustruct as struct
except ImportError:
    import struct


SOF0 = 0xAA
SOF1 = 0x55
VERSION = 0x01
MAX_PAYLOAD = 32

MSG_HEARTBEAT = 0x01
MSG_BALL_POSITION = 0x10
MSG_B_STATUS = 0x20
MSG_A_COMMAND = 0x30

BALL_FLAG_VALID = 0x01
BALL_FLAG_PIPE_VISIBLE = 0x02


def crc16_ccitt_false(data):
    crc = 0xFFFF
    for value in data:
        crc ^= value << 8
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
        message_type & 0xFF,
        sequence & 0xFF,
        len(payload),
    )) + payload
    crc = crc16_ccitt_false(body)
    return bytes((SOF0, SOF1)) + body + bytes((crc & 0xFF, crc >> 8))


def pack_ball_position(
        sequence,
        x_mm,
        confidence_percent,
        valid,
        pipe_visible,
        frame_id,
        processing_ms):
    flags = 0
    if valid:
        flags |= BALL_FLAG_VALID
    if pipe_visible:
        flags |= BALL_FLAG_PIPE_VISIBLE
    payload = struct.pack(
        "<hBBHH",
        int(x_mm),
        max(0, min(100, int(confidence_percent))),
        flags,
        int(frame_id) & 0xFFFF,
        int(processing_ms) & 0xFFFF,
    )
    return pack_frame(MSG_BALL_POSITION, sequence, payload)


def unpack_b_status(payload):
    if len(payload) != 12:
        raise ValueError("B_STATUS payload must contain 12 bytes")
    values = struct.unpack("<hBBhhHH", payload)
    return {
        "line_error": values[0],
        "line_bits": values[1],
        "flags": values[2],
        "ball_x_mm": values[3],
        "rod_angle_cdeg": values[4],
        "fault_bits": values[5],
        "vision_age_ms": values[6],
    }


class FrameParser:
    def __init__(self):
        self.buffer = bytearray()
        self.expected_size = 0

    def reset(self):
        self.buffer = bytearray()
        self.expected_size = 0

    def feed_byte(self, value):
        value = int(value) & 0xFF

        if not self.buffer:
            if value == SOF0:
                self.buffer.append(value)
            return None

        if len(self.buffer) == 1:
            if value == SOF1:
                self.buffer.append(value)
            elif value != SOF0:
                self.reset()
            return None

        self.buffer.append(value)
        if len(self.buffer) == 6:
            payload_length = self.buffer[5]
            if payload_length > MAX_PAYLOAD:
                self.reset()
                return None
            self.expected_size = payload_length + 8

        if not self.expected_size or len(self.buffer) < self.expected_size:
            return None

        raw = bytes(self.buffer)
        self.reset()
        received_crc = raw[-2] | (raw[-1] << 8)
        if raw[2] != VERSION or crc16_ccitt_false(raw[2:-2]) != received_crc:
            return None
        return {
            "version": raw[2],
            "message_type": raw[3],
            "sequence": raw[4],
            "payload": raw[6:-2],
        }

    def feed(self, data):
        frames = []
        for value in data:
            frame = self.feed_byte(value)
            if frame is not None:
                frames.append(frame)
        return frames
