import RPi.GPIO as GPIO
import time

# 设定GPIO引脚编号模式为BOARD模式
GPIO.setmode(GPIO.BOARD)

# 定义巡线传感器连接的引脚编号
SENSOR1_PIN = 18  # 这里的引脚编号需要根据实际情况进行修改
SENSOR2_PIN = 15
SENSOR3_PIN = 26
SENSOR4_PIN = 24


# 将巡线传感器引脚设置为输入模式
GPIO.setup(SENSOR1_PIN, GPIO.IN)
GPIO.setup(SENSOR2_PIN, GPIO.IN)
GPIO.setup(SENSOR3_PIN, GPIO.IN)
GPIO.setup(SENSOR4_PIN, GPIO.IN)

try:

    while True:

        # 读取巡线传感器的电平值，如果传感器上有黑线，返回1，否则返回0
        sensor1Value = GPIO.input(SENSOR1_PIN)
        sensor2Value = GPIO.input(SENSOR2_PIN)
        sensor3Value = GPIO.input(SENSOR3_PIN)
        sensor4Value = GPIO.input(SENSOR4_PIN)

        # 打印每个传感器读取的电平值
        print(sensor1Value, sensor2Value, sensor3Value, sensor4Value)
        # 等待0.1秒（100毫秒），降低打印频率
        time.sleep(0.1)

finally:
    GPIO.cleanup()  # 程序结束后，清理所有GPIO设置

