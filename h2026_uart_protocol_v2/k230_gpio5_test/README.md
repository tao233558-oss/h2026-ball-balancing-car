# K230 GPIO5 → MSPM0-B 串口 V2 测试

## 接线

| K230 40Pin | 信号 | MSPM0-B |
|---|---|---|
| 物理第11脚 | GPIO5 / UART2_TXD | PB3 / UART3_RX |
| 物理第9脚 | GND | GND |

K230 使用 Type-C 单独供电，不连接 40Pin 的 5V 或 3V3。当前测试为单向
K230 → B，因此 B 板 PB2/TX 不接。固件还会将 K230 物理第13脚 GPIO6
配置成 UART2_RXD，以满足 UART 驱动初始化要求，但单向测试时第13脚不接线。

## 文件

将下面三个文件放在 K230 的同一目录：

- `uart_v2_gpio5_test.py`
- `vision_uart_publisher.py`
- `h2026_uart_protocol_v2.py`

在 CanMV IDE 中打开并运行 `uart_v2_gpio5_test.py`。它不启动相机和模型，
只发送合成的 V2 视觉数据。不要覆盖板上现有的 `/sdcard/main.py`。

## 正常现象

B 板 OLED 第一行从 `K230 WAIT V2` 变为 `K230 V2 UART OK`：

- `X` 每秒按 `-120、-60、0、+60、+120` 循环；
- `C` 为 `88`；
- `V` 为 `1`；
- `S` 持续增加；
- `OK` 持续增加；
- `ER` 保持 `0`。
