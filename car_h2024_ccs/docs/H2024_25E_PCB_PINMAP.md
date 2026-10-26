# H2024 固件 - 25E PCB 有效引脚表

本表是 `car_h2024_ccs` 的运行时硬件基准。它以用户实际使用的 25E PCB 为准，只列出 2024 H 题固件会初始化的接口。

| 模块 | PCB/MCU 引脚 |
|---|---|
| 左电机 PWM / AIN1 / AIN2 | PA29 / PB24 / PB20 |
| 右电机 PWM / BIN1 / BIN2 | PA30 / PB19 / PB18 |
| TB6612 STBY | PB16，高有效；复位/待机保持低 |
| 左编码器 A/B | PA12 / PA13 |
| 右编码器 A/B | PA24 / PA15 |
| 8 路循迹 D1-D8 | PA0 / PA25 / PA31 / PA28 / PA1 / PA7 / PA22 / PA16 |
| OLED SCL/SDA | PB2 / PB3，软件 I2C 开漏 |
| JY61P IMU TX/RX | MCU PA8 TX / PA9 RX，9600 8N1，交叉连接 |
| MODE 按键 | PA17，内部上拉，按下接地 |
| SET/START 按键 | PA27，内部上拉，按下接地 |
| 蜂鸣器 / 指示灯 | PB12 低有效 / PB13 高有效 |
| J-Link SWD | PA19 SWDIO / PA20 SWCLK / NRST / 3V3 VTref / GND |

H2024 不初始化：PB23、PB27、PB25、PB0、PB1、K230 串口、步进云台、TOF、蓝牙和激光接口。运行中 PA17 或 PA27 任一按键被拉低，10 ms 控制中断都会关闭 PWM 和 PB16/STBY。

当前验证边界：TI-Clang 已编译通过；实车电平、方向、传感器极性和 J-Link 烧录仍需逐项验证。
