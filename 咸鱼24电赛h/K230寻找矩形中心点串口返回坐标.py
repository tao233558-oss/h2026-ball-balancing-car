# ============================================================
# 立创·庐山派 K230D
# 黑色矩形框检测 + UART2 中心坐标发送
#
# UART2_TXD -> GPIO11
# UART2_RXD -> GPIO12
#
# 串口参数：
# 波特率：9600
# 数据位：8
# 校验位：无
# 停止位：1
#
# 串口发送格式：
# 检测到矩形：CENTER,x,y\r\n
# 未检测目标：CENTER,-1,-1\r\n
# ============================================================

from media.sensor import *
from media.display import *
from media.media import *

from machine import UART
from machine import FPIOA

import time
import os
import gc
import sys
import image
import cv_lite


# ============================================================
# 图像参数
# ============================================================

IMAGE_WIDTH = 400
IMAGE_HEIGHT = 300

IMAGE_SHAPE = [
    IMAGE_HEIGHT,
    IMAGE_WIDTH
]


# ============================================================
# UART2 参数
# ============================================================

UART_BAUDRATE = 9600

# 串口发送周期，单位 ms
# 100 ms 表示每秒发送约 10 次
UART_SEND_INTERVAL_MS = 100

# 未识别到矩形时，是否发送 CENTER,-1,-1
UART_SEND_NO_RECT = True


# ============================================================
# cv_lite 矩形检测参数
# ============================================================

CANNY_THRESHOLD_1 = 50
CANNY_THRESHOLD_2 = 150

# 多边形拟合精度
APPROX_EPSILON = 0.04

# 最小轮廓面积比例
AREA_MIN_RATIO = 0.02

# 最大角度余弦值
MAX_ANGLE_COS = 0.5

# 高斯滤波尺寸，必须为奇数
GAUSSIAN_BLUR_SIZE = 5

# 最小矩形面积，单位：像素平方
MIN_FRAME_AREA = 8000


# ============================================================
# 黑色边缘检测参数
# ============================================================

# LAB 阈值：
# L_min, L_max, A_min, A_max, B_min, B_max
BLACK_THRESHOLD = (
    1, 23,
    -8, 4,
    -6, 9
)

# 每条边的采样点数
EDGE_SAMPLE_COUNT = 5

# 每个采样点附近搜索半径
EDGE_SEARCH_RADIUS = 2

# 每条边最少黑色采样点比例
MIN_BLACK_EDGE_PERCENT = 40

# RGB 快速暗色预筛选阈值
RGB_DARK_PREFILTER = 100


# ============================================================
# 跟踪稳定参数
# ============================================================

# 每隔多少帧检测一次
DETECT_INTERVAL = 1

# 连续多少次检测失败后清除目标
HOLD_MISSED_DETECTIONS = 5

# 角点低通平滑参数
SMOOTH_DIVISOR = 3

# 跟踪状态下允许的最大中心跳变
MAX_CENTER_JUMP = 60

# 新旧矩形面积允许变化范围
MIN_TRACK_AREA_PERCENT = 60
MAX_TRACK_AREA_PERCENT = 165


# ============================================================
# 显示参数
# ============================================================

CORNER_COLOR = (255, 0, 0)
CENTER_COLOR = (255, 255, 0)
TEXT_COLOR = (255, 255, 0)

CORNER_RADIUS = 6
MARK_THICKNESS = 3

# IDE 图像压缩质量
IDE_DISPLAY_QUALITY = 50


# ============================================================
# 全局变量
# ============================================================

sensor = None
uart = None

sensor_started = False
display_initialized = False
media_initialized = False

last_corners = None

missed_detections = 0
frame_index = 0
memory_error_count = 0

last_uart_send_time = time.ticks_ms()

# rgb_to_lab 接口模式
# 0：未确定
# 1：元组模式
# 2：三个参数模式
rgb_to_lab_mode = 0


# ============================================================
# UART2 初始化
# ============================================================

def init_uart2():
    """
    初始化 UART2。

    UART2_TXD -> GPIO11
    UART2_RXD -> GPIO12
    """

    fpioa = FPIOA()

    fpioa.set_function(
        11,
        FPIOA.UART2_TXD
    )

    fpioa.set_function(
        12,
        FPIOA.UART2_RXD
    )

    uart_device = UART(
        UART.UART2,
        baudrate=UART_BAUDRATE,
        bits=UART.EIGHTBITS,
        parity=UART.PARITY_NONE,
        stop=UART.STOPBITS_ONE
    )

    print("UART2 初始化完成")
    print("UART2_TXD -> GPIO11")
    print("UART2_RXD -> GPIO12")
    print("波特率：9600")
    print("数据格式：8N1")

    return uart_device


# ============================================================
# 几何计算函数
# ============================================================

def polygon_area2(corners):
    """
    返回四边形面积的两倍。

    使用整数运算，减少浮点运算。
    """

    area2 = 0

    for i in range(4):
        x1, y1 = corners[i]
        x2, y2 = corners[(i + 1) % 4]

        area2 += (
            x1 * y2 -
            x2 * y1
        )

    return abs(area2)


def order_corners(corners):
    """
    将四个角点固定排序为：

    0：左上
    1：右上
    2：右下
    3：左下
    """

    points = sorted(
        corners,
        key=lambda point: point[1]
    )

    top_points = sorted(
        points[:2],
        key=lambda point: point[0]
    )

    bottom_points = sorted(
        points[2:],
        key=lambda point: point[0]
    )

    top_left = top_points[0]
    top_right = top_points[1]

    bottom_left = bottom_points[0]
    bottom_right = bottom_points[1]

    return [
        top_left,
        top_right,
        bottom_right,
        bottom_left
    ]


def center_of(corners):
    """计算四个角点的平均中心坐标。"""

    center_x = 0
    center_y = 0

    for x, y in corners:
        center_x += x
        center_y += y

    center_x //= 4
    center_y //= 4

    return center_x, center_y


def corners_inside_image(corners):
    """检查角点是否全部位于图像范围内。"""

    for x, y in corners:

        if x < 0 or x >= IMAGE_WIDTH:
            return False

        if y < 0 or y >= IMAGE_HEIGHT:
            return False

    return True


# ============================================================
# RGB 转 LAB 兼容函数
# ============================================================

def rgb_to_lab_compatible(r, g, b):
    """
    兼容不同 CanMV 固件中的 rgb_to_lab 参数形式。

    可能的形式：
    image.rgb_to_lab((r, g, b))

    或：
    image.rgb_to_lab(r, g, b)
    """

    global rgb_to_lab_mode

    if rgb_to_lab_mode == 1:
        return image.rgb_to_lab(
            (r, g, b)
        )

    if rgb_to_lab_mode == 2:
        return image.rgb_to_lab(
            r, g, b
        )

    try:
        lab = image.rgb_to_lab(
            (r, g, b)
        )

        rgb_to_lab_mode = 1
        print("rgb_to_lab：元组参数模式")

        return lab

    except Exception:
        lab = image.rgb_to_lab(
            r, g, b
        )

        rgb_to_lab_mode = 2
        print("rgb_to_lab：三参数模式")

        return lab


# ============================================================
# 黑色像素检测
# ============================================================

def rgb_pixel_is_black(r, g, b):
    """判断 RGB 像素是否符合黑色 LAB 阈值。"""

    # 先快速排除明显较亮的像素
    if r > RGB_DARK_PREFILTER:
        return False

    if g > RGB_DARK_PREFILTER:
        return False

    if b > RGB_DARK_PREFILTER:
        return False

    lab = rgb_to_lab_compatible(
        r, g, b
    )

    l_value = lab[0]
    a_value = lab[1]
    b_value = lab[2]

    (
        l_min,
        l_max,
        a_min,
        a_max,
        b_min,
        b_max
    ) = BLACK_THRESHOLD

    return (
        l_min <= l_value <= l_max and
        a_min <= a_value <= a_max and
        b_min <= b_value <= b_max
    )


def pixel_is_black_near(img_np, x, y):
    """
    检查指定坐标附近是否存在黑色像素。

    RGB888 NumPy 数据排列：
    img_np[y, x, channel]
    """

    x_min = max(
        0,
        x - EDGE_SEARCH_RADIUS
    )

    x_max = min(
        IMAGE_WIDTH - 1,
        x + EDGE_SEARCH_RADIUS
    )

    y_min = max(
        0,
        y - EDGE_SEARCH_RADIUS
    )

    y_max = min(
        IMAGE_HEIGHT - 1,
        y + EDGE_SEARCH_RADIUS
    )

    for py in range(y_min, y_max + 1):
        for px in range(x_min, x_max + 1):

            r = int(
                img_np[py, px, 0]
            )

            g = int(
                img_np[py, px, 1]
            )

            b = int(
                img_np[py, px, 2]
            )

            if rgb_pixel_is_black(
                r, g, b
            ):
                return True

    return False


def rectangle_has_black_edges(img_np, corners):
    """
    检查矩形四条边是否具有足够比例的黑色像素。
    """

    denominator = (
        EDGE_SAMPLE_COUNT + 1
    )

    for edge_index in range(4):

        x1, y1 = corners[edge_index]

        x2, y2 = corners[
            (edge_index + 1) % 4
        ]

        black_count = 0

        for step in range(
            1,
            EDGE_SAMPLE_COUNT + 1
        ):

            x = (
                x1 * (denominator - step) +
                x2 * step
            ) // denominator

            y = (
                y1 * (denominator - step) +
                y2 * step
            ) // denominator

            if pixel_is_black_near(
                img_np,
                x,
                y
            ):
                black_count += 1

        if (
            black_count * 100 <
            EDGE_SAMPLE_COUNT *
            MIN_BLACK_EDGE_PERCENT
        ):
            return False

    return True


# ============================================================
# 矩形检测
# ============================================================

def find_best_black_rectangle(
    img,
    previous_corners
):
    """
    使用 cv_lite 检测矩形。

    没有历史目标时：
    选择面积最大的黑色矩形。

    有历史目标时：
    选择中心和面积最接近历史目标的矩形。
    """

    img_np = img.to_numpy_ref()

    rectangles = (
        cv_lite
        .rgb888_find_rectangles_with_corners(
            IMAGE_SHAPE,
            img_np,
            CANNY_THRESHOLD_1,
            CANNY_THRESHOLD_2,
            APPROX_EPSILON,
            AREA_MIN_RATIO,
            MAX_ANGLE_COS,
            GAUSSIAN_BLUR_SIZE
        )
    )

    previous_area2 = 0
    previous_center = None

    if previous_corners is not None:

        previous_area2 = polygon_area2(
            previous_corners
        )

        previous_center = center_of(
            previous_corners
        )

    best_corners = None
    best_score = None

    for rect in rectangles:

        # 返回数据至少需要包括：
        # x、y、w、h，以及四个角点
        if len(rect) < 12:
            continue

        corners = [
            (
                int(rect[4]),
                int(rect[5])
            ),
            (
                int(rect[6]),
                int(rect[7])
            ),
            (
                int(rect[8]),
                int(rect[9])
            ),
            (
                int(rect[10]),
                int(rect[11])
            )
        ]

        if not corners_inside_image(
            corners
        ):
            continue

        corners = order_corners(
            corners
        )

        area2 = polygon_area2(
            corners
        )

        # area2 是实际面积的两倍
        if area2 < MIN_FRAME_AREA * 2:
            continue

        if not rectangle_has_black_edges(
            img_np,
            corners
        ):
            continue

        # 初次寻找目标，选择面积最大的矩形
        if previous_center is None:

            score = -area2

        else:

            area_percent = (
                area2 * 100 //
                max(1, previous_area2)
            )

            center_x, center_y = center_of(
                corners
            )

            dx = (
                center_x -
                previous_center[0]
            )

            dy = (
                center_y -
                previous_center[1]
            )

            distance2 = (
                dx * dx +
                dy * dy
            )

            # 面积变化过大
            if (
                area_percent <
                MIN_TRACK_AREA_PERCENT
            ):
                continue

            if (
                area_percent >
                MAX_TRACK_AREA_PERCENT
            ):
                continue

            # 中心跳变过大
            if (
                distance2 >
                MAX_CENTER_JUMP *
                MAX_CENTER_JUMP
            ):
                continue

            score = (
                distance2 +
                abs(
                    area2 -
                    previous_area2
                ) // 20
            )

        if (
            best_score is None or
            score < best_score
        ):
            best_score = score
            best_corners = corners

    return best_corners


# ============================================================
# 角点平滑
# ============================================================

def smooth_corners(
    previous_corners,
    detected_corners
):
    """对角点进行整数低通滤波。"""

    if previous_corners is None:
        return detected_corners

    smoothed = []

    old_weight = (
        SMOOTH_DIVISOR - 1
    )

    for i in range(4):

        old_x, old_y = (
            previous_corners[i]
        )

        new_x, new_y = (
            detected_corners[i]
        )

        smooth_x = (
            old_x * old_weight +
            new_x
        ) // SMOOTH_DIVISOR

        smooth_y = (
            old_y * old_weight +
            new_y
        ) // SMOOTH_DIVISOR

        smoothed.append(
            (smooth_x, smooth_y)
        )

    return smoothed


# ============================================================
# 绘图函数
# ============================================================

def draw_detection_result(
    img,
    corners
):
    """绘制矩形、四个角点和中心点。"""

    for i in range(4):

        x1, y1 = corners[i]

        x2, y2 = corners[
            (i + 1) % 4
        ]

        img.draw_line(
            x1,
            y1,
            x2,
            y2,
            color=CORNER_COLOR,
            thickness=2
        )

    for x, y in corners:

        img.draw_circle(
            x,
            y,
            CORNER_RADIUS,
            color=CORNER_COLOR,
            thickness=MARK_THICKNESS
        )

    center_x, center_y = center_of(
        corners
    )

    img.draw_cross(
        center_x,
        center_y,
        color=CENTER_COLOR,
        size=12,
        thickness=MARK_THICKNESS
    )

    # 在中心点附近显示坐标
    coordinate_text = "%d,%d" % (
        center_x,
        center_y
    )

    text_x = center_x + 8
    text_y = center_y + 8

    # 防止文字超出画面
    if text_x > IMAGE_WIDTH - 90:
        text_x = center_x - 85

    if text_y > IMAGE_HEIGHT - 25:
        text_y = center_y - 25

    img.draw_string_advanced(
        text_x,
        text_y,
        16,
        coordinate_text,
        color=CENTER_COLOR
    )


def draw_status(
    img,
    fps_value,
    corners
):
    """绘制 FPS 和目标状态。"""

    img.draw_rectangle(
        0,
        0,
        170,
        42,
        color=(0, 0, 0),
        fill=True
    )

    img.draw_string_advanced(
        5,
        2,
        16,
        "FPS: %.1f" % fps_value,
        color=TEXT_COLOR
    )

    if corners is not None:

        center_x, center_y = center_of(
            corners
        )

        status_text = "C:%d,%d" % (
            center_x,
            center_y
        )

    else:
        status_text = "C:-1,-1"

    img.draw_string_advanced(
        5,
        21,
        16,
        status_text,
        color=TEXT_COLOR
    )


# ============================================================
# UART2 数据发送
# ============================================================

def send_center_by_uart(
    uart_device,
    corners
):
    """
    通过 UART2 发送矩形中心坐标。

    格式：
    CENTER,x,y\r\n
    """

    if uart_device is None:
        return

    try:

        if corners is not None:

            center_x, center_y = center_of(
                corners
            )

            message = (
                "CENTER,{},{}\r\n"
                .format(
                    center_x,
                    center_y
                )
            )

            uart_device.write(
                message
            )

        elif UART_SEND_NO_RECT:

            uart_device.write(
                "CENTER,-1,-1\r\n"
            )

    except Exception as e:
        print(
            "UART2 发送异常：",
            e
        )


def receive_uart_data(
    uart_device
):
    """
    非阻塞读取 UART2 数据。

    收到数据后仅打印到 IDE，
    不影响矩形检测。
    """

    if uart_device is None:
        return

    try:

        data = uart_device.read()

        if data:
            print(
                "[UART2 RX]:",
                data
            )

    except Exception as e:
        print(
            "UART2 接收异常：",
            e
        )


# ============================================================
# 主程序
# ============================================================

fps_value = 0.0
fps_frame_count = 0
fps_last_time = time.ticks_ms()


try:
    # --------------------------------------------------------
    # 初始化 UART2
    # --------------------------------------------------------

    uart = init_uart2()

    # --------------------------------------------------------
    # 初始化摄像头
    # --------------------------------------------------------

    sensor = Sensor()
    sensor.reset()

    # 水平镜像和垂直翻转同时开启，
    # 相当于图像旋转 180°
    sensor.set_hmirror(True)
    sensor.set_vflip(True)

    sensor.set_framesize(
        width=IMAGE_WIDTH,
        height=IMAGE_HEIGHT
    )

    sensor.set_pixformat(
        Sensor.RGB888
    )

    # --------------------------------------------------------
    # 初始化 IDE 显示
    # --------------------------------------------------------

    Display.init(
        Display.VIRT,
        width=IMAGE_WIDTH,
        height=IMAGE_HEIGHT,
        to_ide=True,
        quality=IDE_DISPLAY_QUALITY
    )

    display_initialized = True

    # --------------------------------------------------------
    # 初始化媒体管理器
    # --------------------------------------------------------

    MediaManager.init()
    media_initialized = True

    # --------------------------------------------------------
    # 启动摄像头
    # --------------------------------------------------------

    sensor.run()
    sensor_started = True

    print("摄像头初始化完成")
    print("开始检测黑色矩形")
    print("串口发送格式：CENTER,x,y")

    # --------------------------------------------------------
    # 主循环
    # --------------------------------------------------------

    while True:

        os.exitpoint()

        img = sensor.snapshot()

        # ----------------------------------------------------
        # FPS 计算
        # ----------------------------------------------------

        fps_frame_count += 1

        current_time = time.ticks_ms()

        elapsed_time = time.ticks_diff(
            current_time,
            fps_last_time
        )

        if elapsed_time >= 1000:

            fps_value = (
                fps_frame_count *
                1000.0 /
                elapsed_time
            )

            fps_frame_count = 0
            fps_last_time = current_time

        # ----------------------------------------------------
        # 隔帧检测矩形
        # ----------------------------------------------------

        if (
            frame_index %
            DETECT_INTERVAL == 0
        ):

            detected_corners = None

            try:

                detected_corners = (
                    find_best_black_rectangle(
                        img,
                        last_corners
                    )
                )

            except MemoryError:

                memory_error_count += 1

                if (
                    memory_error_count <= 3 or
                    memory_error_count % 30 == 0
                ):
                    print(
                        "矩形检测内存不足，"
                        "已跳过本帧，次数：",
                        memory_error_count
                    )

                gc.collect()

            # 检测成功
            if detected_corners is not None:

                last_corners = smooth_corners(
                    last_corners,
                    detected_corners
                )

                missed_detections = 0

            # 检测失败
            else:

                missed_detections += 1

            # 连续多次未识别到目标
            if (
                missed_detections >
                HOLD_MISSED_DETECTIONS
            ):

                last_corners = None
                missed_detections = 0

        # ----------------------------------------------------
        # UART2 周期发送中心坐标
        # ----------------------------------------------------

        now = time.ticks_ms()

        if (
            time.ticks_diff(
                now,
                last_uart_send_time
            ) >= UART_SEND_INTERVAL_MS
        ):

            send_center_by_uart(
                uart,
                last_corners
            )

            last_uart_send_time = now

        # ----------------------------------------------------
        # UART2 非阻塞接收
        # ----------------------------------------------------

        receive_uart_data(
            uart
        )

        # ----------------------------------------------------
        # 绘制检测结果
        # ----------------------------------------------------

        if last_corners is not None:

            draw_detection_result(
                img,
                last_corners
            )

        # 最后绘制文字，避免文字干扰检测
        draw_status(
            img,
            fps_value,
            last_corners
        )

        # ----------------------------------------------------
        # 显示图像
        # ----------------------------------------------------

        Display.show_image(
            img
        )

        frame_index += 1

        # 不要每帧都执行垃圾回收，
        # 否则画面会出现周期性卡顿
        if frame_index % 30 == 0:
            gc.collect()


except KeyboardInterrupt as e:

    print(
        "用户停止程序：",
        e
    )


except BaseException as e:

    print("程序发生异常：")
    print(
        "异常内容：",
        e
    )

    try:
        sys.print_exception(e)
    except Exception:
        pass


finally:

    print("正在释放资源")

    if (
        sensor_started and
        sensor is not None
    ):
        try:
            sensor.stop()
        except Exception as e:
            print(
                "停止摄像头异常：",
                e
            )

    if display_initialized:
        try:
            Display.deinit()
        except Exception as e:
            print(
                "释放显示资源异常：",
                e
            )

    try:
        os.exitpoint(
            os.EXITPOINT_ENABLE_SLEEP
        )
    except Exception:
        pass

    time.sleep_ms(100)

    if media_initialized:
        try:
            MediaManager.deinit()
        except Exception as e:
            print(
                "释放媒体资源异常：",
                e
            )

    uart = None
    gc.collect()

    print("程序已结束")