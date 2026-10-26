"""MicroPython-compatible K230 -> MSPM0 aiming protocol encoder."""

SYNC_0 = 0xAA
SYNC_1 = 0x55
VERSION = 0x01
FRAME_SIZE = 15

FLAG_TARGET_VALID = 0x01
FLAG_SPOT_VALID = 0x02
FLAG_CIRCLE_COMPLETE = 0x04


def crc8(data):
    crc = 0
    for value in data:
        crc ^= value
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def _append_i16_le(buffer, value):
    value = int(value) & 0xFFFF
    buffer.append(value & 0xFF)
    buffer.append((value >> 8) & 0xFF)


def make_packet(sequence, flags, target_x, target_y, spot_x, spot_y,
                circle_phase=0):
    payload = [VERSION, sequence & 0xFF, flags & 0xFF]
    _append_i16_le(payload, target_x)
    _append_i16_le(payload, target_y)
    _append_i16_le(payload, spot_x)
    _append_i16_le(payload, spot_y)
    payload.append(circle_phase & 0xFF)
    return bytes([SYNC_0, SYNC_1] + payload + [crc8(payload)])
