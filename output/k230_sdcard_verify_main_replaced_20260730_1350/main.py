import os
import sys
import time
import gc
import network

from libs.PipeLine import PipeLine
from libs.SmoothWBCRtsp import SmoothWBCRtsp
from libs.YOLO import YOLO11
from vision_uart_publisher import VisionUartPublisher


# Lushan Pi K230 + ST7701 LCD.
DISPLAY_SIZE = [800, 480]
# Keep the camera/display hardware in its supported 30 fps mode. The RTSP
# encoder below performs the requested load reduction at a stable 20 fps.
CAMERA_FPS = 30

# The model ultimately sees a 320x180 image plus letterbox padding. Capturing
# 320x180 directly avoids moving and resizing a redundant 640x360 RGB frame.
AI_FRAME_SIZE = [320, 180]
MODEL_INPUT_SIZE = [320, 320]
MODEL_PATH = "/sdcard/yolo11_gq_320.kmodel"
CLASS_NAMES_PATH = "/sdcard/yolo11_gq_classes.txt"

CONFIDENCE_THRESHOLD = 0.50
NMS_THRESHOLD = 0.45
MAX_BOXES = 10
GC_INTERVAL_FRAMES = 10
NETWORK_CHECK_INTERVAL_FRAMES = 30

# Smoothness-first H.264 settings for the direct 2.4 GHz AP connection.
STREAM_FPS = 20
STREAM_BIT_RATE = 1200
STREAM_GOP = 20

# The board creates this 2.4 GHz Wi-Fi access point.
AP_SSID = "LushanPi_RTSP"
AP_PASSWORD = "CHANGE_ME_123"  # Set a local AP password before deployment.

# The official WBCRtsp helper uses port 8554 and session "test".
RTSP_PORT = 8554
RTSP_SESSION = "test"

LOG_PATH = "/sdcard/stream_status.txt"

# UART2 is routed to the 40-pin header: physical pin 11 = GPIO5/TX.
VISION_UART_TX_GPIO = 5
VISION_UART_PERIOD_MS = 20

# Temporary link-test scale. x_mm carries the signed display-pixel offset while
# VISION_CALIBRATED is False, so MSPM0 must not close the ball-control loop yet.
VISION_X_UNITS_PER_DISPLAY_PIXEL = 1.0
VISION_CALIBRATED = False

pipeline = None
yolo = None
vision_uart = None
access_point = None
pipeline_started = False
rtsp_started = False
last_network_state = None
stream_url = None


def reset_log():
    try:
        with open(LOG_PATH, "w") as log_file:
            log_file.write("LushanPi K230 v1.6 YOLO11 + WBC RTSP log\n")
    except BaseException:
        pass


def log_line(message):
    text = str(message)
    print(text)
    try:
        with open(LOG_PATH, "a") as log_file:
            log_file.write(text + "\n")
    except BaseException:
        pass


def log_exception(prefix, error):
    log_line(prefix + ": " + repr(error))
    sys.print_exception(error)


def require_file(path):
    try:
        os.stat(path)
    except BaseException:
        raise RuntimeError("Required file not found: " + path)


def load_labels(path):
    labels = []
    with open(path, "r") as label_file:
        for line in label_file:
            label = line.strip()
            if label:
                labels.append(label)

    if not labels:
        raise RuntimeError("No class names in: " + path)
    return labels


def show_status(line_1, line_2, error=False):
    if pipeline is None or pipeline.osd_img is None:
        return

    pipeline.osd_img.clear()
    pipeline.osd_img.draw_rectangle(
        0, 0, DISPLAY_SIZE[0], 82,
        color=(0, 0, 0, 210),
        fill=True
    )
    pipeline.osd_img.draw_string_advanced(
        12, 5, 27,
        line_1,
        color=(255, 255, 255, 255)
    )
    pipeline.osd_img.draw_string_advanced(
        12, 42, 25,
        line_2,
        color=(255, 80, 80, 255) if error else (80, 255, 80, 255)
    )
    pipeline.show_image()


def create_access_point():
    # Exact AP setup used by firmware v1.6 example network_wlan_ap.py.
    wlan = network.WLAN(network.AP_IF)
    wlan.config(ssid=AP_SSID, key=AP_PASSWORD)

    log_line("WiFi AP configured: " + AP_SSID)
    try:
        log_line("WiFi AP info: %s" % (wlan.info(),))
    except BaseException as error:
        log_exception("WiFi AP info query failed", error)

    try:
        log_line("WiFi AP status: %s" % (wlan.status(),))
    except BaseException as error:
        log_exception("WiFi AP status query failed", error)

    return wlan


def update_network_state(wlan):
    global last_network_state
    global stream_url

    if wlan is None:
        return

    try:
        board_ip = wlan.ifconfig()[0]
        if board_ip and board_ip != "0.0.0.0":
            if last_network_state != board_ip:
                stream_url = (
                    "rtsp://%s:%d/%s"
                    % (board_ip, RTSP_PORT, RTSP_SESSION)
                )
                log_line("VLC URL: " + stream_url)
                last_network_state = board_ip
        elif last_network_state != "WAITING":
            stream_url = None
            log_line("Waiting for WiFi AP address")
            last_network_state = "WAITING"
    except BaseException as error:
        if last_network_state != "QUERY_ERROR":
            log_exception("WiFi AP address query failed", error)
            last_network_state = "QUERY_ERROR"


def publish_vision_result(result, inference_ms):
    if vision_uart is None:
        return

    detected = False
    x_units = 0
    confidence_percent = 0
    in_roi = False
    single_target = False

    if result and len(result) >= 3 and result[0]:
        boxes = result[0]
        scores = result[2]
        target_count = len(boxes)
        single_target = target_count == 1

        best_index = 0
        for index in range(1, target_count):
            if scores[index] > scores[best_index]:
                best_index = index

        x, y, width, height = boxes[best_index]
        center_x = float(x) + float(width) * 0.5
        center_y = float(y) + float(height) * 0.5
        display_error_px = center_x - DISPLAY_SIZE[0] * 0.5

        x_units = int(round(
            display_error_px * VISION_X_UNITS_PER_DISPLAY_PIXEL
        ))
        confidence_percent = int(round(float(scores[best_index]) * 100.0))
        in_roi = (
            0.0 <= center_x < DISPLAY_SIZE[0]
            and 0.0 <= center_y < DISPLAY_SIZE[1]
        )
        detected = True

    vision_uart.update_measurement(
        x_mm=x_units,
        confidence_percent=confidence_percent,
        detected=detected,
        calibrated=VISION_CALIBRATED,
        in_roi=in_roi,
        single_target=single_target,
        model_ready=(yolo is not None),
        camera_ready=pipeline_started,
        inference_ms=inference_ms,
    )
    vision_uart.poll()


try:
    os.exitpoint(os.EXITPOINT_ENABLE)
    reset_log()
    log_line("Program started")
    log_line("Board: " + str(os.uname()))

    require_file(MODEL_PATH)
    require_file(CLASS_NAMES_PATH)
    class_names = load_labels(CLASS_NAMES_PATH)
    log_line("YOLO11 model: " + MODEL_PATH)
    log_line("YOLO11 labels: " + str(class_names))

    try:
        vision_uart = VisionUartPublisher(
            tx_gpio=VISION_UART_TX_GPIO,
            target_period_ms=VISION_UART_PERIOD_MS
        )
        log_line(
            "H2026 UART v2: UART2 TX GPIO%d, 115200 8N1"
            % VISION_UART_TX_GPIO
        )
        log_line(
            "H2026 UART calibration: %s"
            % ("READY" if VISION_CALIBRATED else "PIXEL DIAGNOSTIC ONLY")
        )
    except BaseException as error:
        vision_uart = None
        log_exception("H2026 UART startup failed", error)

    # PipeLine keeps display and AI capture on separate hardware channels.
    pipeline = PipeLine(
        rgb888p_size=AI_FRAME_SIZE,
        display_mode="lcd",
        display_size=DISPLAY_SIZE,
        osd_layer_num=1,
        debug_mode=0
    )
    pipeline.create(fps=CAMERA_FPS, to_ide=False)
    pipeline_started = True
    log_line("Camera and ST7701 pipeline started")

    show_status(
        "WiFi: %s  PASS: %s" % (AP_SSID, AP_PASSWORD),
        "Loading RTSP and YOLO11..."
    )

    try:
        access_point = create_access_point()
        update_network_state(access_point)
    except BaseException as error:
        log_exception("WiFi AP startup failed", error)

    # Firmware v1.6 requires Display.init() (done by PipeLine.create()) before
    # enabling writeback. The optimized helper keeps WBC/VENC at a stable 20
    # fps and lowers wireless traffic while retaining the 800x480 LCD image.
    try:
        SmoothWBCRtsp.configure(
            wbc_width=480,
            wbc_height=800,
            bit_rate=STREAM_BIT_RATE,
            fps=STREAM_FPS,
            gop_len=STREAM_GOP
        )
        SmoothWBCRtsp.start()
        rtsp_started = True
        log_line(
            "WBC H264 RTSP started: %d fps, %d kbps, GOP %d"
            % (STREAM_FPS, STREAM_BIT_RATE, STREAM_GOP)
        )
        show_status(
            "WiFi: %s  PASS: %s" % (AP_SSID, AP_PASSWORD),
            "VLC: rtsp://192.168.169.1:8554/test"
        )
        time.sleep(2)
    except BaseException as error:
        log_exception("WBC RTSP startup failed", error)
        show_status(
            "RTSP ERROR - YOLO will still run",
            "Read /sdcard/stream_status.txt",
            error=True
        )

    # Official v1.6 YOLO module. Post-processing/NMS runs through the firmware's
    # optimized aidemo implementation rather than Python loops.
    yolo = YOLO11(
        task_type="detect",
        mode="video",
        kmodel_path=MODEL_PATH,
        labels=class_names,
        rgb888p_size=AI_FRAME_SIZE,
        model_input_size=MODEL_INPUT_SIZE,
        display_size=pipeline.get_display_size(),
        conf_thresh=CONFIDENCE_THRESHOLD,
        nms_thresh=NMS_THRESHOLD,
        max_boxes_num=MAX_BOXES,
        debug_mode=0
    )
    yolo.config_preprocess()
    log_line("YOLO11 initialized")

    frame_counter = 0

    while True:
        os.exitpoint()

        frame = pipeline.get_frame()
        inference_start_ms = time.ticks_ms()
        result = yolo.run(frame)
        inference_ms = max(
            0,
            time.ticks_diff(time.ticks_ms(), inference_start_ms)
        )
        publish_vision_result(result, inference_ms)
        yolo.draw_result(result, pipeline.osd_img)

        frame_counter += 1

        if frame_counter % NETWORK_CHECK_INTERVAL_FRAMES == 0:
            update_network_state(access_point)

        pipeline.show_image()

        # Periodic collection avoids a full garbage-collection pause every frame.
        if frame_counter % GC_INTERVAL_FRAMES == 0:
            gc.collect()

except KeyboardInterrupt:
    pass
except BaseException as error:
    log_exception("Main program failed", error)

    # Keep LCD and any active RTSP output alive instead of falling to grey.
    if pipeline_started:
        try:
            show_status(
                "PROGRAM ERROR - LCD held active",
                "Read /sdcard/stream_status.txt",
                error=True
            )
            while True:
                os.exitpoint()
                time.sleep(1)
        except KeyboardInterrupt:
            pass
        except BaseException:
            pass
finally:
    if vision_uart is not None:
        try:
            vision_uart.deinit()
        except BaseException as error:
            log_exception("H2026 UART cleanup failed", error)

    if yolo is not None:
        try:
            yolo.deinit()
        except BaseException as error:
            log_exception("YOLO11 cleanup failed", error)

    if rtsp_started:
        try:
            SmoothWBCRtsp.stop()
        except BaseException as error:
            log_exception("WBC RTSP cleanup failed", error)

    if pipeline_started and pipeline is not None:
        try:
            pipeline.destroy()
        except BaseException as error:
            log_exception("Pipeline cleanup failed", error)
