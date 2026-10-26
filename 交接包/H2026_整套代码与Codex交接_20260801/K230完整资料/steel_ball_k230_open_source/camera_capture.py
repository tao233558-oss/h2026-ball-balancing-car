# 立创·庐山派 K230 / K230D — 按键触发拍照保存到 SD 卡（自动适配两块板）
# 编写者：LCKFB-YZH
# 按键按下 -> LED 闪烁 -> 保存 JPEG 到 /sdcard/images/
import time, os, sys

sensor_id = 2  # 庐山派 Lite / K230D，CanMV v1.8 已验证。

from media.sensor import Sensor, CAM_CHN_ID_0
from media.display import Display
from media.media import MediaManager
import image
from machine import Pin
from machine import FPIOA

def get_board_info():
    if os.uname()[-1] == "k230_canmv_lckfb":
        return {"R": 62, "G": 20, "B": 63, "BTN": 53, "ON": 0, "OFF": 1}   # 标准版 K230
    # 修改点：原 BTN 为 64，现改为 36
    return {"R": 65, "G": 66, "B": 71, "BTN": 36, "ON": 1, "OFF": 0}       # Lite-K230D

B = get_board_info()
fpioa = FPIOA()
for p in (B["R"], B["G"], B["B"], B["BTN"]):
    fpioa.set_function(p, getattr(FPIOA, "GPIO%d" % p))

LED_R = Pin(B["R"], Pin.OUT, pull=Pin.PULL_NONE, drive=7)
LED_G = Pin(B["G"], Pin.OUT, pull=Pin.PULL_NONE, drive=7)
LED_B = Pin(B["B"], Pin.OUT, pull=Pin.PULL_NONE, drive=7)
LED_R.value(B["OFF"]); LED_G.value(B["OFF"]); LED_B.value(B["OFF"])
PHOTO_LED = LED_G

button = Pin(B["BTN"], Pin.IN, Pin.PULL_DOWN)
debounce_delay = 200
last_press_time = 0
button_last_state = 0

DISPLAY_MODE = "LCD"   # "VIRT" / "LCD" / "HDMI"
if DISPLAY_MODE == "VIRT":
    DISPLAY_WIDTH, DISPLAY_HEIGHT, FPS = 1920, 1080, 30
elif DISPLAY_MODE == "LCD":
    DISPLAY_WIDTH, DISPLAY_HEIGHT, FPS = 800, 480, 60
elif DISPLAY_MODE == "HDMI":
    DISPLAY_WIDTH, DISPLAY_HEIGHT, FPS = 1920, 1080, 30
else:
    raise ValueError("未知 DISPLAY_MODE")

def lckfb_save_jpg(img, filename, quality=95):
    compressed_data = img.compress(quality=quality)
    with open(filename, "wb") as f:
        f.write(compressed_data)
    print(f"[INFO] 已保存: {filename}")

image_folder = "/sdcard/images"
try: os.stat(image_folder)
except OSError: os.mkdir(image_folder)

image_count = 0
existing_images = [f for f in os.listdir(image_folder) if f.startswith("lckfb_") and f.endswith(".jpg")]
if existing_images:
    numbers = []
    for fname in existing_images:
        try: numbers.append(int(fname[6:11]))
        except: pass
    if numbers: image_count = max(numbers)

try:
    sensor = Sensor() if sensor_id is None else Sensor(id=sensor_id)
    sensor.reset()
    sensor.set_framesize(width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, chn=CAM_CHN_ID_0)
    sensor.set_pixformat(Sensor.RGB565, chn=CAM_CHN_ID_0)

    if DISPLAY_MODE == "VIRT":
        Display.init(Display.VIRT, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, fps=FPS)
    elif DISPLAY_MODE == "LCD":
        Display.init(Display.ST7701, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)
    elif DISPLAY_MODE == "HDMI":
        Display.init(Display.LT9611, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, to_ide=True)

    MediaManager.init()
    sensor.run()
    print("[INFO] 摄像头启动")

    fps = time.clock()

    while True:
        fps.tick()
        os.exitpoint()
        img = sensor.snapshot(chn=CAM_CHN_ID_0)

        current_time = time.ticks_ms()
        button_state = button.value()

        if button_state == 1 and button_last_state == 0:
            if current_time - last_press_time > debounce_delay:
                PHOTO_LED.value(B["ON"]); time.sleep_ms(20); PHOTO_LED.value(B["OFF"])
                image_count += 1
                filename = f"{image_folder}/lckfb_{image_count:05d}_{img.width()}x{img.height()}.jpg"
                lckfb_save_jpg(img, filename, quality=95)
                last_press_time = current_time

        button_last_state = button_state

        img.draw_string_advanced(0, 0, 32, str(image_count), color=(255, 0, 0))
        img.draw_string_advanced(0, DISPLAY_HEIGHT-32, 32, str(fps.fps()), color=(255, 0, 0))
        Display.show_image(img)

except KeyboardInterrupt:
    print("[INFO] 用户停止")
except BaseException as e:
    print(f"[ERROR] {e}")
finally:
    if 'sensor' in locals() and isinstance(sensor, Sensor):
        sensor.stop()
    Display.deinit()
    os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
    time.sleep_ms(100)
    MediaManager.deinit()
