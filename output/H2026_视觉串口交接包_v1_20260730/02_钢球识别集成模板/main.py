"""K230 steel-ball detection with the frozen H2026 UART protocol.

Copy main.py, h2026_uart_protocol.py and the kmodel into
/sdcard/steel_ball/. Run from CanMV IDE before enabling auto-start.
"""

from libs.PipeLine import PipeLine
from libs.YOLO import YOLOv8
from libs.Utils import ScopedTiming
from machine import FPIOA, UART
import gc
import time

from h2026_uart_protocol import pack_ball_position


MODEL_PATH = "/sdcard/steel_ball/steel_ball_yolov8n_320.kmodel"
LABELS = ["steel_ball"]
MODEL_INPUT_SIZE = [320, 320]

DISPLAY_MODE = "lcd"
DISPLAY_SIZE = None
RGB888P_SIZE = [640, 360]

# Fix the physical image here. Do not reverse x_mm signs to fix the display.
H_MIRROR = False
V_FLIP = False

CONFIDENCE_THRESHOLD = 0.25
NMS_THRESHOLD = 0.45
MAX_BOXES = 50
PRINT_EVERY_N_FRAMES = 10
UART_SEND_EVERY_N_FRAMES = 1

# Lushan Lite/K230D physical pin 11=GPIO5 TX, pin 13=GPIO6 RX.
UART_TX_GPIO = 5
UART_RX_GPIO = 6
UART_BAUD = 115200

# Fill these values from the final fixed camera/pipe installation.
# Until CALIBRATION_READY is True, frames are sent with valid=0 for safety.
CALIBRATION_READY = False
PIPE_LEFT_X_PX = 0
PIPE_RIGHT_X_PX = RGB888P_SIZE[0] - 1
PIPE_LENGTH_MM = 250


def clamp(value, low, high):
    return min(high, max(low, value))


def pixel_x_to_pipe_mm(center_x):
    span_px = PIPE_RIGHT_X_PX - PIPE_LEFT_X_PX
    if span_px <= 0:
        return 0
    relative_px = center_x - PIPE_LEFT_X_PX
    x_from_left_mm = relative_px * PIPE_LENGTH_MM / span_px
    x_from_center_mm = x_from_left_mm - PIPE_LENGTH_MM / 2
    return int(round(clamp(
        x_from_center_mm,
        -PIPE_LENGTH_MM / 2,
        PIPE_LENGTH_MM / 2,
    )))


def find_best_ball(result):
    count = 0
    best_score = -1.0
    best_center = None

    # YOLOv8 returns [x1, y1, x2, y2, confidence, class_id].
    if result:
        count = len(result)
        for detection in result:
            x1, y1, x2, y2 = detection[:4]
            score = float(detection[4])
            if score > best_score:
                best_score = score
                best_center = (
                    int(round((x1 + x2) / 2)),
                    int(round((y1 + y2) / 2)),
                )
    return count, best_center, best_score


def draw_status(
        osd_image,
        count,
        best_center,
        best_score,
        x_mm,
        display_size):
    osd_image.draw_string_advanced(
        5, 5, 24, "steel_ball: %d" % count, color=(0, 255, 0)
    )

    if best_center is None:
        osd_image.draw_string_advanced(
            5, 34, 20, "ball invalid", color=(255, 64, 64)
        )
        return

    center_x, center_y = best_center
    display_x = center_x * display_size[0] // RGB888P_SIZE[0]
    display_y = center_y * display_size[1] // RGB888P_SIZE[1]
    osd_image.draw_cross(
        display_x,
        display_y,
        color=(255, 255, 0),
        size=12,
        thickness=3,
    )
    osd_image.draw_string_advanced(
        5,
        34,
        20,
        "px=%d,%d x=%dmm score=%d%%"
        % (center_x, center_y, x_mm, int(round(best_score * 100))),
        color=(255, 255, 0),
    )


def main():
    pipeline = None
    detector = None
    uart2 = None
    sequence = 0

    try:
        fpioa = FPIOA()
        fpioa.set_function(UART_TX_GPIO, FPIOA.UART2_TXD)
        fpioa.set_function(UART_RX_GPIO, FPIOA.UART2_RXD)
        uart2 = UART(UART.UART2, UART_BAUD)

        pipeline = PipeLine(
            rgb888p_size=RGB888P_SIZE,
            display_mode=DISPLAY_MODE,
            display_size=DISPLAY_SIZE,
        )
        pipeline.create(hmirror=H_MIRROR, vflip=V_FLIP)
        display_size = pipeline.get_display_size()

        detector = YOLOv8(
            task_type="detect",
            mode="video",
            kmodel_path=MODEL_PATH,
            labels=LABELS,
            rgb888p_size=RGB888P_SIZE,
            model_input_size=MODEL_INPUT_SIZE,
            display_size=display_size,
            conf_thresh=CONFIDENCE_THRESHOLD,
            nms_thresh=NMS_THRESHOLD,
            max_boxes_num=MAX_BOXES,
            debug_mode=0,
        )
        detector.config_preprocess()

        frame_index = 0
        while True:
            started_ms = time.ticks_ms()
            with ScopedTiming("total", 1):
                frame = pipeline.get_frame()
                result = detector.run(frame)
                detector.draw_result(result, pipeline.osd_img)

                count, best_center, best_score = find_best_ball(result)
                detected = best_center is not None
                valid = detected and CALIBRATION_READY
                x_mm = (
                    pixel_x_to_pipe_mm(best_center[0])
                    if detected else 0
                )
                draw_status(
                    pipeline.osd_img,
                    count,
                    best_center,
                    best_score,
                    x_mm,
                    display_size,
                )
                pipeline.show_image()

                if frame_index % UART_SEND_EVERY_N_FRAMES == 0:
                    processing_ms = time.ticks_diff(
                        time.ticks_ms(), started_ms)
                    packet = pack_ball_position(
                        sequence=sequence,
                        x_mm=x_mm,
                        confidence_percent=(
                            int(round(best_score * 100))
                            if detected else 0
                        ),
                        valid=valid,
                        pipe_visible=CALIBRATION_READY,
                        frame_id=frame_index,
                        processing_ms=processing_ms,
                    )
                    uart2.write(packet)
                    sequence = (sequence + 1) & 0xFF

                if frame_index % PRINT_EVERY_N_FRAMES == 0:
                    print(
                        "[steel_ball] count=%d x_mm=%d valid=%d"
                        % (count, x_mm, int(valid))
                    )

                frame_index = (frame_index + 1) & 0xFFFF
                gc.collect()
    except KeyboardInterrupt:
        print("[steel_ball] stopped by user")
    except BaseException as error:
        print("[steel_ball] error:", error)
        raise
    finally:
        if detector is not None:
            try:
                detector.deinit()
            except BaseException as cleanup_error:
                print("[steel_ball] detector cleanup error:", cleanup_error)
        if pipeline is not None and getattr(pipeline, "sensor", None) is not None:
            try:
                pipeline.destroy()
            except BaseException as cleanup_error:
                print("[steel_ball] pipeline cleanup error:", cleanup_error)
        gc.collect()


main()

