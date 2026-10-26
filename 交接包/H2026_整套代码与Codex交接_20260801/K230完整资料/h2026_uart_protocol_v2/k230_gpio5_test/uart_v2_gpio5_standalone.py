"""Temporary standalone K230 UART2/GPIO5 protocol-v2 transmitter."""

import time

try:
    import ustruct as struct
except ImportError:
    import struct

from machine import FPIOA, UART


TX_GPIO = 5
RX_GPIO = 6
TEST_X_MM = (-120, -60, 0, 60, 120)
VISION_FLAGS_ALL_READY_VALID = 0x3F


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


def build_vision_frame(sequence, x_mm, frame_id):
    payload = struct.pack(
        "<hBBHHH",
        int(x_mm),
        88,
        VISION_FLAGS_ALL_READY_VALID,
        int(frame_id) & 0xFFFF,
        0,
        12,
    )
    body = bytes((0x02, 0x10, int(sequence) & 0xFF, len(payload))) + payload
    crc = crc16_ccitt_false(body)
    return b"\xAA\x55" + body + bytes((crc & 0xFF, (crc >> 8) & 0xFF))


fpioa = FPIOA()
fpioa.set_function(TX_GPIO, FPIOA.UART2_TXD)
fpioa.set_function(RX_GPIO, FPIOA.UART2_RXD)
uart = UART(
    UART.UART2,
    baudrate=115200,
    bits=UART.EIGHTBITS,
    parity=UART.PARITY_NONE,
    stop=UART.STOPBITS_ONE,
    timeout=0,
)

sequence = 0
frame_id = 0
test_index = 0
last_send_ms = time.ticks_ms()
last_change_ms = last_send_ms
print("H2026 V2 GPIO5 TX test started")

try:
    while True:
        now_ms = time.ticks_ms()
        if time.ticks_diff(now_ms, last_change_ms) >= 1000:
            test_index = (test_index + 1) % len(TEST_X_MM)
            frame_id = (frame_id + 1) & 0xFFFF
            last_change_ms = now_ms
            print("x_mm=", TEST_X_MM[test_index])

        if time.ticks_diff(now_ms, last_send_ms) >= 20:
            uart.write(build_vision_frame(
                sequence,
                TEST_X_MM[test_index],
                frame_id,
            ))
            sequence = (sequence + 1) & 0xFF
            last_send_ms = now_ms

        time.sleep_ms(1)
finally:
    uart.deinit()
