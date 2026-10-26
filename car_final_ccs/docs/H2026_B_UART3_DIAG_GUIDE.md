# H2026 MSPM0-B UART3 接收诊断

该固件只验证 K230 → MSPM0-B 的统一二进制协议，不驱动车轮或步进电机。
启动时 PA7、PA12、PA13 全部拉低，DCC 步进驱动保持休眠。

## 接线

| K230 源山派 Lite/K230D | MSPM0-B |
|---|---|
| 物理 11 / GPIO5 / UART2_TXD | PB3 / UART3_RX |
| 物理 13 / GPIO6 / UART2_RXD | PB2 / UART3_TX |
| 物理 9 / GND | GND |

OLED 保持副控板接线：PA0=SDA、PA1=SCL。电气参数为 115200、8N1、
3.3 V TTL，TX/RX 交叉连接。

## 测试步骤

1. 只给 K230、MSPM0-B 和 OLED 上电，步进驱动可暂时断开。
2. 给 MSPM0-B 烧录
   `output/firmware/mspm0g3507_h2026_b_uart3_diag.out`。
3. K230 运行视觉交接包 `01_UART单独测试/main.py`。
4. OLED 应循环显示 `X=-100、-50、0、+50、+100`。
5. `OK` 应持续增加，`ER` 应保持 0，`S` 是 0–255 循环的帧序号。

## OLED 字段

- `X`：收到的小球位置，单位 mm。
- `C`：置信度 0–100。
- `V`：valid 标志，假坐标测试固定为 1。
- `S`：帧序号。
- `A`：距最近一帧的大致时间，单位 ms，最大显示 999。
- `OK`：正确 BALL_POSITION 帧数。
- `ER`：CRC、版本、长度或载荷错误帧数。

未收到数据时第一行显示 `K230 WAIT AA55`；收到首个合法帧后显示
`K230 UART3 OK`。

当前验证状态：源代码和 TI-Clang 编译已通过；真实 K230/MSPM0-B 串口波形和
长时间误码率尚未上板验证。
