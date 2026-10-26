# 天猛星小车只读硬件验收模式

## 这个固件做什么

该模式用于小车硬件回来后的第一次通电检查。它只读取输入，不允许电机、云台或激光动作：

- 左右电机 PWM 始终为 0。
- PB16/STBY 每 10 ms 强制拉低。
- 不输出云台 STEP/DIR。
- 不输出激光使能。
- UART0 只连接板载 CH340，115200 8N1。

固件文件：`output/firmware/car_final_ccs_hw_accept_safe.out`。

## 通电前

1. 把小车架起来，确保车轮离开桌面。
2. 暂时断开电机主电源、激光电源和云台驱动电源。
3. 保留天猛星、OLED、按键、循迹模块、编码器和 CH340 USB 连接。
4. 串口助手选择板载 CH340 对应端口，设置为 `115200, 8, None, 1`。

## 屏幕引导

### 第1步：四个按键

OLED 显示 `1.KEY SW1-SW4`。依次短按：

- SW1：PA27
- SW2：PA17
- SW3：PB23
- SW4：PB27

`SEEN:0x0F` 表示四个按键全部被稳定检测到。小于 30 ms 的单次干扰不会记为通过。

### 第2步：八路循迹

OLED 显示 `2.TRACK D1-D8`。先让全部探头看白底，再用黑色胶带依次划过 D1～D8。

`PASS:0xFF` 表示每一路都至少看见过一次白底和一次黑线，能够发现常高、常低或接线错误的问题。

当前引脚顺序：

| 通道 | 天猛星引脚 |
|---|---|
| D1 | PA0 |
| D2 | PA25 |
| D3 | PA31 |
| D4 | PA28 |
| D5 | PA1 |
| D6 | PA7 |
| D7 | PB25（真正排针图复核后由PA3修改） |
| D8 | PA16 |

### 第3步：编码器

OLED 显示 `3.TURN WHEELS`。保持电机断电，用手分别缓慢转动左右轮。

左右活动量都达到 4 个计数后通过。这里只确认脉冲能否进入芯片，编码器方向和比例需要在后续悬空电机测试中确认。

编码器接线：左轮 A/B 为 PA12/PA13，右轮 A/B 为 PA24/PA15。

## 最终显示

三项输入全部通过后，OLED 显示：

```text
INPUT TEST PASS
MOTOR/STBY OFF
K230:BLOCKED
```

`K230:BLOCKED` 不是故障，而是防止误判。当前 UART0 已被板载 CH340 使用，工程还没有为 K230 分配独立 UART，所以本模式不会把 K230 假报为通过。

串口每 500 ms 输出一行 `HWACC` 状态，可保存成验收记录。关键字段：

- `KEY_PASS=0x0F`：四个按键通过。
- `TRACK_PASS=0xFF`：八路循迹都看到白/黑变化。
- `ENC_L`、`ENC_R`：左右编码器累计活动量。
- `MOTOR=0 STBY=0`：执行器保持关闭。
- `RESULT=INPUT_PASS`：当前可验证的输入全部通过。

## 电脑端自动验收助手

工程提供 `offline_tools/hw_accept_monitor.py`，可以把上面的串口文字自动转换为实时状态和验收报告。

第一次使用实时串口前安装依赖：

```powershell
python -m pip install pyserial
```

查看电脑上的串口：

```powershell
python offline_tools\hw_accept_monitor.py --list
```

假设板载 CH340 是 COM3，运行最多 3 分钟，并在输入全部通过后自动停止：

```powershell
python offline_tools\hw_accept_monitor.py --port COM3 --duration 180 --until-pass
```

运行前必须关闭友善串口助手，两个软件不能同时占用同一个 COM 口。

没有硬件时可以用样例日志练习：

```powershell
python offline_tools\hw_accept_monitor.py --replay offline_tools\fixtures\hardware_acceptance_sample.log --until-pass --report-prefix demo
```

每次运行会在 `output/hardware_acceptance_reports` 生成三个同名文件：

- `.csv`：可以直接用 Excel 打开，包含每次采样和判断结果。
- `.json`：最终状态及逐项 SW1～SW4、D1～D8 判断。
- `.log`：原始串口文字，便于以后复查。

结论 `INPUT_PASS_K230_BLOCKED` 表示当前按键、循迹、编码器和关闭输出检查通过，但 K230 仍待独立 UART；它不代表整车全部验收完成。

## 当前不能验证的项目

没有实物时，当前只完成源代码、离线模型和编译验证。以下项目必须硬件回来后继续：

1. 万用表确认 PB16 上电和运行时确实为低电平。
2. 编码器正负方向和左右轮对应关系。
3. 电机低 PWM、方向、堵转保护和急停。
4. K230 独立 UART 的引脚与真实通信。
5. 云台 STEP/DIR、限位和机械零点。
6. 激光默认关闭电平及安全联锁。
