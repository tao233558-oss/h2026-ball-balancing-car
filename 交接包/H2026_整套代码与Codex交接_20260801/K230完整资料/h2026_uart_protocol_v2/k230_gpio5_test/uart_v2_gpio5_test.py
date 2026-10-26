"""K230 -> MSPM0-B UART protocol v2 hardware test.

Hardware route:
  K230 40-pin physical pin 11 (GPIO5 / UART2_TXD) -> MSPM0-B PB3 (UART3_RX)
  K230 40-pin physical pin 9  (GND)               -> MSPM0-B GND

The test deliberately does not use the camera or model. It sends a valid
synthetic VISION_SAMPLE every 20 ms and changes x_mm once per second.
"""

import time

from vision_uart_publisher import VisionUartPublisher


TX_GPIO = 5
TEST_X_MM = (-120, -60, 0, 60, 120)
MEASUREMENT_PERIOD_MS = 1000


def run():
    publisher = VisionUartPublisher(
        tx_gpio=TX_GPIO,
        target_period_ms=20,
    )
    test_index = 0
    last_measurement_ms = time.ticks_ms()

    publisher.update_measurement(
        x_mm=TEST_X_MM[test_index],
        confidence_percent=88,
        detected=True,
        calibrated=True,
        in_roi=True,
        single_target=True,
        model_ready=True,
        camera_ready=True,
        inference_ms=12,
        result_timestamp_ms=last_measurement_ms,
    )
    print("H2026 V2 UART test: GPIO5/UART2_TX, 115200 8N1")

    try:
        while True:
            now_ms = time.ticks_ms()
            if (
                    time.ticks_diff(now_ms, last_measurement_ms)
                    >= MEASUREMENT_PERIOD_MS):
                test_index = (test_index + 1) % len(TEST_X_MM)
                last_measurement_ms = now_ms
                publisher.update_measurement(
                    x_mm=TEST_X_MM[test_index],
                    confidence_percent=88,
                    detected=True,
                    calibrated=True,
                    in_roi=True,
                    single_target=True,
                    model_ready=True,
                    camera_ready=True,
                    inference_ms=12,
                    result_timestamp_ms=now_ms,
                )
                print(
                    "x_mm=",
                    TEST_X_MM[test_index],
                    "write_errors=",
                    publisher.write_error_count,
                )

            publisher.poll(now_ms)
            time.sleep_ms(1)
    finally:
        publisher.deinit()


run()
