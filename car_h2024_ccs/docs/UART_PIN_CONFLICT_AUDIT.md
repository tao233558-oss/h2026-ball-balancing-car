# 天猛星 UART 与 PCB 最终引脚审计

## 结论

最终方案把 4 个硬件 UART 分别分给 4 个串口端点，TOF200F 改用 I²C，因此不存在 UART 实例复用或 TX/RX 引脚重叠：

| 端点 | 外设 | MCU TX | MCU RX | 速率 | 当前代码状态 |
|---|---|---|---|---:|---|
| 板载 CH340 | UART0 | PA10 | PA11 | 115200 | 已启用；只作电脑调试 |
| IMU | UART1 | PA8 | PA9 | 9600 | 已启用 |
| K230 | UART2 | PB15 | PA22 | 115200 | 已实现；开关默认关闭 |
| 蓝牙 | UART3 | PB2 | PB3 | 待按模块确认 | 仅保留引脚，尚未初始化 |
| OLED + TOF200F | 软件 I²C | PB0/SCL | PB1/SDA | 约100 kHz | OLED/TOF 均已改开漏；ST VL53L0X 驱动已编译 |

TI SDK 已核对的复用宏：UART2_TX 为 `PINCM32/PB15`，UART2_RX 为 `PINCM47/PA22`，UART3_TX/RX 为 `PINCM15/PB2` 和 `PINCM16/PB3`。

## PCB 必改项（相对于旧板）

| ECO | 旧板/中间版 | 最终接法 | 标记 |
|---|---|---|---|
| 循迹 D7 | 原板 PA2；中间版 PA22；后曾计划 PA3 | **PB25，LQFP64 脚27；天猛星排针 U7_7/U9_7** | **再次修改；固件已同步并编译通过** |
| OLED SCL | PB2 | **PB0，LQFP64 脚47** | **修改** |
| OLED SDA | PB3 | **PB1，LQFP64 脚48** | **修改** |
| 蓝牙 MCU_TX→BT_RX | 原计划 PB4 | **PB2，LQFP64 脚50** | **修改** |
| 蓝牙 BT_TX→MCU_RX | 原计划 PB5 | **PB3，LQFP64 脚51** | **修改** |
| K230 MCU_TX→庐山派物理13脚/UART2_RXD | 未冻结/旧文档曾误建议 UART0 或“IO12” | **PB15，LQFP64 脚3** | **新增专用 UART2** |
| K230 庐山派物理11脚/UART2_TXD→MCU_RX | 未冻结/旧文档曾写成“IO11” | **PA22，LQFP64 脚18** | **新增专用 UART2** |
| TOF200F SCL | 临时测试曾用 PB5/UART_RX | **PB0/SCL** | **从 UART 改 I²C** |
| TOF200F SDA | 临时测试曾用 PB4/UART_TX | **PB1/SDA** | **从 UART 改 I²C** |

不要再按旧版 PDF 中的“PA10/PA11 或 PB1 给 K230”布线；PA10/PA11 必须留给板载 CH340，PB1 已属于 OLED/TOF I²C 总线。

天猛星官方排针图确认 PA3 没有引出，不能用于插接式载板。D7 的 PCB 冻结目标改为已引出的 `PB25`；`track.c/xunji.c` 已同步为 `GPIOB/DL_GPIO_PIN_25/PB25`，并于 2026-07-16 通过 CCS TI-Clang 编译。该结论仍未替代实板逐路验证。

## 接线方向

- 庐山派物理 `11` 脚（`GPIO5/UART2_TXD`）→ MSPM0 `PA22/UART2_RX`。
- MSPM0 `PB15/UART2_TX` → 庐山派物理 `13` 脚（`GPIO6/UART2_RXD`）。
- 两板公共地建议接庐山派物理 `9` 脚 GND；9/11/13 位于同一侧且连续，便于使用三针插座。
- 蓝牙 `RX` ← MSPM0 `PB2/UART3_TX`。
- 蓝牙 `TX` → MSPM0 `PB3/UART3_RX`。
- OLED `SCL`、TOF200F `SCL`、MSPM0 `PB0` 接在同一网络。
- OLED `SDA`、TOF200F `SDA`、MSPM0 `PB1` 接在同一网络。
- TOF200F 物理排针：1=VIN（规格书3～5 V）、2=GND、3=SDA→PB1、4=SCL→PB0、5=RX、6=TX；最终运行不用5/6，建议留配置/恢复测试点。
- 所有串口模块与 MSPM0 必须共地，数字接口统一按 3.3 V TTL。

庐山派 USB-A 不能直接插到天猛星 Type-C 来代替 TTL 串口。两端 USB 口都不是这条 K230↔MSPM0 UART2 的电气接口；正式 PCB 使用庐山派物理 9/11/13 脚（GND/TXD/RXD）。注意物理 12 脚是 `GPIO47/UART2_CTS`，不是 UART2_RXD。

## TOF200F 改为 I²C

规格书确认 TOF200F 支持 UART/Modbus 和 IIC 两种模式。模块仍处于默认 UART 模式时，可向地址 1 发送以下 Modbus 帧切换到 IIC 模式：

```text
01 06 00 09 00 01 98 08
```

IIC 模式会释放模块内部 VL53L0 的 I²C 总线，后续按 VL53L0 寄存器协议访问。模块板上有 10 kΩ 上拉，但正式 PCB 仍建议给 SCL/SDA 各预留 4.7 kΩ 上拉到 3.3 V（如实测总上拉过强可 DNP）。

规格书第2页接线表对 SDA/SCL 的“数据/时钟”功能文字疑似互换；原理图按引脚名称和彩色示意图连接：3脚SDA对SDA，4脚SCL对SCL，不按那两格中文功能描述反接。

## 代码安全边界

- `E2025_ENABLE_K230_UART2 = 0`：最终专用串口默认关闭，硬件确认后才改为 1。
- `E2025_ENABLE_K230_UART0_BRIDGE = 0`：旧兼容桥继续关闭。
- 蓝牙没有协议和确定波特率，因此当前不初始化 UART3，避免臆造行为。
- TOF200F 已接入 ST VL53L0X API：检查`0xEEAA`标识、初始化、参考校准、SPAD 管理与单次测距均已进入独立测试档；尚未做实物验证。
- OLED 模拟 I²C 已改成“只拉低或释放”，并在 ACK 位释放 SDA，可与 TOF200F 共线；真实上拉、地址和波形仍需示波器验证。

## 自动审计与验证状态

运行：

```powershell
python offline_tools\uart_pin_plan_audit.py
```

输出：

- `output/uart_pin_audit/uart_plan.csv`
- `output/uart_pin_audit/pin_occupancy.csv`
- `output/uart_pin_audit/summary.json`

当前已完成默认安全配置、K230 UART2 开启配置和 TOF200F I²C 测试配置的 CCS/ticlang 编译。没有硬件在手，TOF、蓝牙和 K230 的真实波形与通信结果均为 **NOT RUN**。
