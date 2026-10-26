"""K230 UART2 link test: send known fake ball positions without camera."""

import time

from machine import FPIOA, UART

from h2026_uart_protocol import pack_ball_position


# Lushan Lite/K230D:
# header pin 11 = GPIO5/UART2_TXD
# header pin 13 = GPIO6/UART2_RXD
UART_TX_GPIO = 5
UART_RX_GPIO = 6
UART_BAUD = 115200

TEST_POSITIONS_MM = (-100, -50, 0, 50, 100)

fpioa = FPIOA()
fpioa.set_function(UART_TX_GPIO, FPIOA.UART2_TXD)
fpioa.set_function(UART_RX_GPIO, FPIOA.UART2_RXD)
uart2 = UART(UART.UART2, UART_BAUD)

sequence = 0
frame_id = 0

print("[H2026 UART] link test started: GPIO5 TX, GPIO6 RX, 115200 8N1")

while True:
    for x_mm in TEST_POSITIONS_MM:
        packet = pack_ball_position(
            sequence=sequence,
            x_mm=x_mm,
            confidence_percent=99,
            valid=True,
            pipe_visible=True,
            frame_id=frame_id,
            processing_ms=1,
        )
        uart2.write(packet)
        print("[H2026 UART] x_mm=%d seq=%d" % (x_mm, sequence))
        sequence = (sequence + 1) & 0xFF
        frame_id = (frame_id + 1) & 0xFFFF
        time.sleep_ms(500)

