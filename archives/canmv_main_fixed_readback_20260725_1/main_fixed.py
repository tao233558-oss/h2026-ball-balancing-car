"""K230 CanMV real-time steel-ball detection.

Copy this file and ``steel_ball_yolov8n_320.kmodel`` to
``/sdcard/steel_ball/``. Run this file from CanMV IDE. To auto-start, copy
the same code to ``/sdcard/main.py`` without changing MODEL_PATH.
"""

from libs.PipeLine import PipeLine
from libs.YOLO import YOLOv8
from libs.Utils import ScopedTiming
import gc


MODEL_PATH = "/sdcard/steel_ball/steel_ball_yolov8n_320_full.kmodel"
LABELS = ["steel_ball"]
MODEL_INPUT_SIZE = [320, 320]

# The installed board-side PipeLine library auto-detects the camera.
DISPLAY_MODE = "lcd"
DISPLAY_SIZE = None
RGB888P_SIZE = [640, 360]
H_MIRROR = True
V_FLIP = True

CONFIDENCE_THRESHOLD = 0.50
NMS_THRESHOLD = 0.45
MAX_BOXES = 50
PRINT_EVERY_N_FRAMES = 10


def add_status_overlay(result, osd_image, frame_index, display_size):
    count = 0
    best_score = -1.0
    best_center = None

    # Board-side libs.YOLO.YOLOv8 returns one row per target:
    # [x1, y1, x2, y2, confidence, class_id].
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

    osd_image.draw_string_advanced(
        5, 5, 24, "steel_ball: %d" % count, color=(0, 255, 0)
    )

    if best_center is not None:
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
            "best center: %d,%d" % (center_x, center_y),
            color=(255, 255, 0),
        )

    if frame_index % PRINT_EVERY_N_FRAMES == 0:
        if best_center is None:
            print("[steel_ball] count=0")
        else:
            print(
                "[steel_ball] count=%d best_center=(%d,%d) score=%.3f"
                % (count, best_center[0], best_center[1], best_score)
            )


def main():
    pipeline = None
    detector = None
    try:
        pipeline = PipeLine(
            rgb888p_size=RGB888P_SIZE,
            display_mode=DISPLAY_MODE,
            display_size=DISPLAY_SIZE,
        )
        pipeline.create(
            hmirror=H_MIRROR,
            vflip=V_FLIP,
        )
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
            with ScopedTiming("total", 1):
                frame = pipeline.get_frame()
                result = detector.run(frame)
                detector.draw_result(result, pipeline.osd_img)
                add_status_overlay(
                    result, pipeline.osd_img, frame_index, display_size
                )
                pipeline.show_image()
                frame_index += 1
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
