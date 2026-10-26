"""Minimal CanMV K230 UART2 sender for the frozen H2026 protocol.

Copy this file and h2026_uart_protocol.py beside the deployed steel-ball
main.py. The vision program should call send_ball() once per processed frame.
Pixel-to-mm calibration remains part of the vision/pipe geometry calibration.
"""

from machine import FPIOA, UART

from h2026_uart_protocol import pack_ball_position


# Lushan Lite/K230D: header pin 11 = GPIO5 TX, pin 13 = GPIO6 RX.
UART_TX_GPIO = 5
UART_RX_GPIO = 6
UART_BAUD = 115200

_sequence = 0

fpioa = FPIOA()
fpioa.set_function(UART_TX_GPIO, FPIOA.UART2_TXD)
fpioa.set_function(UART_RX_GPIO, FPIOA.UART2_RXD)
uart2 = UART(UART.UART2, UART_BAUD)


def send_ball(
        x_mm,
        confidence_percent,
        valid,
        pipe_visible,
        frame_id,
        processing_ms):
    global _sequence
    packet = pack_ball_position(
        sequence=_sequence,
        x_mm=x_mm,
        confidence_percent=confidence_percent,
        valid=valid,
        pipe_visible=pipe_visible,
        frame_id=frame_id,
        processing_ms=processing_ms,
    )
    uart2.write(packet)
    _sequence = (_sequence + 1) & 0xFF
