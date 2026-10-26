# 庐山派K230 - 矩形识别 + 二维云台激光追踪回中 (脱机版)
# 保存为 TF卡 /sdcard/main.py, 上电自启
# 硬件:
#   激光: GPIO33 (高电平触发)
#   俯仰舵机: PWM2, Pin46
#   偏航舵机: PWM0, Pin42
#   显示: ST7701 LCD 800×480

import time
import os
import gc

from media.sensor import *
from media.display import *
from media.media import *
from machine import FPIOA
from machine import Pin
from machine import PWM

sensor = None

# ==================== 参数 ====================
WIDTH  = 640
HEIGHT = 640
CX = WIDTH // 2
CY = HEIGHT // 2

# LAB 颜色阈值 (需根据实际黄色矩形调整)
LAB_TH = [(45, 83, -12, 66, 38, 83)]
BLOB_PX = 2000

# PID
KP = -0.0025
KI = -0.0004

# 舵机占空比 50Hz
DUTY_MIN = 2.5
DUTY_MID = 7.5
DUTY_MAX = 12.5

# 锁定
LOCK_PX = 15
LOCK_N  = 5
LOST_N  = 30

# 舵机死区
DUTY_DEADBAND = 0.15

# 性能
FIND_INTERVAL = 3   # 每N帧检测一次 (降负载, 减少堆分配)
GC_INTERVAL   = 50  # 每N帧回收一次Python堆 (提高频率防碎片化)
OSD_INTERVAL  = 10  # 每N帧更新一次OSD (减少字符串分配)
SENSOR_RESET_INTERVAL = 3000  # 每N帧重置sensor (清空底层buffer)
SLEEP_MS = 15
# ==================== 参数结束 ====================


class PID:
    def __init__(self, kp, ki, out, tgt):
        self.e = 0
        self.el = 0
        self.kp = kp
        self.ki = ki
        self.tgt = tgt
        self.out = out

    def cal(self, v):
        self.e = self.tgt - v
        self.out += self.kp * (self.e - self.el) + self.ki * self.e
        self.el = self.e
        return self.out


def safe_mmz_pct():
    """获取 mmz 使用率, 固件不支持则返回 -1"""
    try:
        info = gc.sys_mmz()
        return info[1] * 100 // info[0]
    except:
        return -1


def safe_page_pct():
    """获取 sys_page 使用率, 固件不支持则返回 -1"""
    try:
        info = gc.sys_page()
        return info[1] * 100 // info[0]
    except:
        return -1


def safe_heap_pct():
    """获取 sys_heap 使用率, 固件不支持则返回 -1"""
    try:
        info = gc.sys_heap()
        return info[1] * 100 // info[0]
    except:
        return -1


try:
    print("=== START ===")

    fpioa = FPIOA()

    # 激光
    fpioa.set_function(33, FPIOA.GPIO33)
    laser = Pin(33, Pin.OUT)
    laser.value(0)

    # 俯仰舵机
    fpioa.set_function(46, FPIOA.PWM2)
    pwm_y = PWM(2, 50)
    pwm_y.duty(DUTY_MID)
    pwm_y.enable(1)

    # 偏航舵机
    fpioa.set_function(42, FPIOA.PWM0)
    pwm_x = PWM(0, 50)
    pwm_x.duty(DUTY_MID)
    pwm_x.enable(1)

    # 摄像头
    sensor = Sensor(width=WIDTH, height=HEIGHT)
    sensor.reset()
    sensor.set_framesize(width=WIDTH, height=HEIGHT)
    sensor.set_pixformat(Sensor.RGB565)

    # LCD 显示
    Display.init(Display.ST7701, width=800, height=480, to_ide=True)
    MediaManager.init()
    sensor.run()

    pid_x = PID(KP, KI, DUTY_MID, CX)
    pid_y = PID(KP, KI, DUTY_MID, CY)

    # 状态
    lock_cnt = 0
    lost_cnt = 0
    last_dx = DUTY_MID
    last_dy = DUTY_MID
    laser_on = False
    bx = by = bw = bh = 0
    rx = ry = 0
    has_rect = False

    clock = time.clock()
    frame = 0

    # OSD 缓存字符串初始值 (防第一帧未定义)
    _fps_str = "fps:0.0"
    _laser_str = "LASER:OFF"
    _laser_color = (200, 200, 200)
    _err_str = ""
    _m = _p = _h = -1
    _mmz_str = _page_str = _heap_str = ""
    _mc = _pc = _hc = (200, 200, 200)

    while True:
        clock.tick()
        os.exitpoint()

        img = sensor.snapshot(chn=CAM_CHN_ID_0)
        frame += 1

        # --- 色块检测 (每 FIND_INTERVAL 帧一次) ---
        if frame % FIND_INTERVAL == 0:
            blobs = img.find_blobs(LAB_TH, False,
                                   x_stride=5, y_stride=5,
                                   pixels_threshold=BLOB_PX,
                                   margin=False)
            os.exitpoint()

            if blobs is not None and len(blobs) > 0:
                lost_cnt = 0
                best = max(blobs, key=lambda b: b.pixels())
                bx, by, bw, bh = best.rect()
                rx = bx + bw / 2
                ry = by + bh / 2
                has_rect = True

                # PID → 舵机
                dx = pid_x.cal(rx)
                dx = max(DUTY_MIN, min(DUTY_MAX, dx))
                if abs(dx - last_dx) > DUTY_DEADBAND:
                    pwm_x.duty(dx)
                    last_dx = dx

                dy = pid_y.cal(ry)
                dy = max(DUTY_MIN, min(DUTY_MAX, dy))
                if abs(dy - last_dy) > DUTY_DEADBAND:
                    pwm_y.duty(dy)
                    last_dy = dy

                # 锁定 → 激光
                if abs(rx - CX) < LOCK_PX and abs(ry - CY) < LOCK_PX:
                    lock_cnt += 1
                else:
                    lock_cnt = max(0, lock_cnt - 1)

                want = (lock_cnt >= LOCK_N)
                if want != laser_on:
                    laser.value(1 if want else 0)
                    laser_on = want

            else:
                lost_cnt += 1
                has_rect = False
                if lost_cnt >= LOST_N and laser_on:
                    laser.value(0)
                    laser_on = False
                    lock_cnt = 0

            # 释放 blobs → 归还 sys_page + mmz
            blobs = None

        # --- 绘制 ---
        if has_rect:
            img.draw_rectangle(bx, by, bw, bh, color=(0, 255, 0), thickness=3)
            ix = int(rx)
            iy = int(ry)
            img.draw_line(ix - 15, iy, ix + 15, iy, color=(255, 0, 0), thickness=2)
            img.draw_line(ix, iy - 15, ix, iy + 15, color=(255, 0, 0), thickness=2)
            img.draw_circle(ix, iy, 5, color=(255, 0, 0), thickness=2, fill=False)

        # --- OSD: 每帧绘制(不闪), 仅每 OSD_INTERVAL 帧重新计算字符串(减少分配) ---
        if frame % OSD_INTERVAL == 0:
            # 缓存字符串，避免每帧 format() 分配
            _fps_str = "fps:{:.1f}".format(clock.fps())
            _laser_str = "LASER:ON" if laser_on else "LASER:OFF"
            _laser_color = (255, 0, 0) if laser_on else (200, 200, 200)
            if has_rect:
                _err_str = "err:{:.0f},{:.0f}".format(rx - CX, ry - CY)
            _m = safe_mmz_pct()
            _p = safe_page_pct()
            _h = safe_heap_pct()
            if _m >= 0:
                _mc = (0,255,0) if _m < 80 else (255,255,0) if _m < 95 else (255,0,0)
                _mmz_str = "mmz:{}%".format(_m)
            if _p >= 0:
                _pc = (0,255,0) if _p < 80 else (255,255,0) if _p < 95 else (255,0,0)
                _page_str = "page:{}%".format(_p)
            if _h >= 0:
                _hc = (0,255,0) if _h < 80 else (255,255,0) if _h < 95 else (255,0,0)
                _heap_str = "heap:{}%".format(_h)

        # 每帧绘制 (用缓存的字符串, 不闪)
        img.draw_string_advanced(10, 10, 24, _fps_str, color=(255, 255, 0))
        img.draw_string_advanced(10, 40, 24, _laser_str, color=_laser_color)
        if has_rect:
            img.draw_string_advanced(10, 70, 24, _err_str, color=(255, 255, 255))
        if _m >= 0:
            img.draw_string_advanced(10, 100, 24, _mmz_str, color=_mc)
        if _p >= 0:
            img.draw_string_advanced(10, 130, 24, _page_str, color=_pc)
        if _h >= 0:
            img.draw_string_advanced(10, 160, 24, _heap_str, color=_hc)

        # 十字线
        img.draw_line(CX - 20, CY, CX + 20, CY, color=(0, 0, 255), thickness=1)
        img.draw_line(CX, CY - 20, CX, CY + 20, color=(0, 0, 255), thickness=1)

        # --- LCD 显示 ---
        img.midpoint_pool(2, 2)
        Display.show_image(img, x=(800 - 320) // 2, y=(480 - 320) // 2)
        os.exitpoint()

        # --- 定期回收 Python 堆 (防碎片化) ---
        if frame % GC_INTERVAL == 0:
            os.exitpoint()  # 回收前喂狗
            gc.collect()

        time.sleep_ms(SLEEP_MS)

except KeyboardInterrupt as e:
    print("stop:", e)
except BaseException as e:
    print("err:", e)
finally:
    if isinstance(sensor, Sensor):
        sensor.stop()
    laser.value(0)
    Display.deinit()
    os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
    time.sleep_ms(100)
    MediaManager.deinit()
    pwm_x.deinit()
    pwm_y.deinit()
