# H2026 双 MSPM0 串口协议冻结版 v1

本目录是 K230、主控 MSPM0-A 和机构/传感 MSPM0-B 共用的唯一串口规范。
后续不要再在视觉代码或主控代码中新增另一种 `AA55`/文本协议。

## 1. 电气参数和接线

两条链路全部固定为 **115200 baud、8 数据位、无校验、1 停止位、无流控、
3.3 V TTL、小端序**。三块板必须共地；只连接 TX、RX、GND，不把两块控制板
的 3V3/5V 电源脚直接并在一起。

| 链路 | 发送端 | 接收端 |
|---|---|---|
| K230 → B | 源山派物理 11 / GPIO5 / UART2_TXD | B 的 PB3 / UART3_RX |
| B → K230 | B 的 PB2 / UART3_TX | 源山派物理 13 / GPIO6 / UART2_RXD |
| B → A | B 的 PA8 / UART1_TX | A 的 PA22 / UART2_RX |
| A → B | A 的 PB15 / UART2_TX | B 的 PA9 / UART1_RX |
| 公共参考 | K230 GND | A GND、B GND |

说明：

- B 的 PA10/PA11 属于 UART0，但天猛星板上同时连着 CH340，正式比赛不占用。
- B 的 PA22 已接 8 路红外 D7，不能再用于 UART2_RX。
- A 的 PA22 只作为 A↔B 的 UART2_RX；8 路红外全部移到 B 后不再冲突。
- 当前钢球代码使用源山派 Lite/K230D，因此 K230 固定使用 UART2：
  `GPIO5=TX`、`GPIO6=RX`；物理 9 脚为 GND。FPIOA 使用的是 GPIO 编号，
  不是 40 针排针的物理编号。
- TX 线上可预留 33–100 Ω 串联电阻，线长尽量小于 30 cm。
- 协议串口禁止输出 `print()`、日志和菜单字符；调试信息走 USB/独立串口。

## 2. 通用帧

| 字节 | 字段 | 说明 |
|---:|---|---|
| 0 | `0xAA` | 帧头 1 |
| 1 | `0x55` | 帧头 2 |
| 2 | `version` | 当前固定 `0x01` |
| 3 | `message_type` | 消息类型 |
| 4 | `sequence` | 每发送一帧加 1，255 后回到 0 |
| 5 | `payload_length` | 0–32 |
| 6... | `payload` | 小端序有效载荷 |
| 末尾 2 字节 | `crc16` | 低字节在前 |

CRC 使用 **CRC-16/CCITT-FALSE**：多项式 `0x1021`、初值 `0xFFFF`、
不反射、无异或输出。计算范围是 `version` 至 payload 末尾，不包含帧头和
CRC 自身。标准校验串 `123456789` 的结果应为 `0x29B1`。

## 3. 消息表

### `0x10 BALL_POSITION`：K230 → B，8 字节

| 偏移 | 类型 | 名称 | 单位/含义 |
|---:|---|---|---|
| 0 | `int16` | `x_mm` | 小球相对钢管中心 O 的位置 |
| 2 | `uint8` | `confidence` | 0–100 |
| 3 | `uint8` | `flags` | bit0=有效，bit1=钢管可见 |
| 4 | `uint16` | `frame_id` | 视觉帧编号 |
| 6 | `uint16` | `processing_ms` | 本帧处理耗时 |

位置正方向统一为：**从中心 O 指向右侧可升降/步进电机端为正**，指向左侧
固定铰点为负。推荐 K230 以 50 Hz 发送；没有识别到球也要发送一帧，
但清除 `valid` 位。

### `0x20 B_STATUS`：B → A，12 字节

| 偏移 | 类型 | 名称 | 单位/含义 |
|---:|---|---|---|
| 0 | `int16` | `line_error` | 循迹误差，黑线靠 D1 侧为正 |
| 2 | `uint8` | `line_bits` | bit0=D1，...，bit7=D8；1=检测到黑线 |
| 3 | `uint8` | `flags` | bit0=球有效，bit1=循迹有效，bit2=机构归零，bit3=故障 |
| 4 | `int16` | `ball_x_mm` | 最近一次有效小球位置 |
| 6 | `int16` | `rod_angle_cdeg` | 右端抬升为正，0.01° |
| 8 | `uint16` | `fault_bits` | 故障位 |
| 10 | `uint16` | `vision_age_ms` | 距最近有效视觉帧的时间 |

B 以 100 Hz 周期发送，A 不需要逐帧应答。

### `0x30 A_COMMAND`：A → B，10 字节

| 偏移 | 类型 | 名称 | 单位/含义 |
|---:|---|---|---|
| 0 | `uint8` | `mode` | 0=安全空闲，1=回中心，2=指定位置，3=归零，4=诊断 |
| 1 | `uint8` | `flags` | bit0=运行允许，bit1=急停，bit2=重新归零 |
| 2 | `int16` | `target_x_mm` | 小球目标位置 |
| 4 | `int16` | `vehicle_speed_mm_s` | 车体前进为正 |
| 6 | `int16` | `vehicle_accel_mm_s2` | 车体前进加速度为正 |
| 8 | `uint16` | `command_timeout_ms` | 命令有效期，推荐 200 ms |

A 以 100 Hz 周期发送。急停位优先级高于 mode 和运行允许位。

### `0x01 HEARTBEAT`：任意节点，6 字节

`uint32 uptime_ms + uint8 role + uint8 state`。role：1=K230，2=A，3=B。
调试阶段建议 2 Hz 发送，闭环运行不依赖心跳确认。

## 4. 超时与安全动作

- B 超过 150 ms 没收到合法 `BALL_POSITION`：清除球有效标志，步进目标缓慢
  回到水平安全位，并在 `B_STATUS` 上报视觉超时。
- A 超过 200 ms 没收到合法 `B_STATUS`：车轮受控停车，不继续盲跑。
- B 超过 A 下发的 `command_timeout_ms` 没收到合法 `A_COMMAND`：停止步进脉冲，
  保持或缓慢回中，具体策略待限位/零点方案确定后冻结。
- CRC 错误、版本错误、长度大于 32 的帧直接丢弃；不能让错误数据刷新超时计时器。

## 5. 代码入口

- MSPM0 公共 C：`car_final_ccs/user/h2026_uart_protocol.h/.c`
- CanMV K230：`k230/h2026_uart_protocol.py`
- CanMV UART2 接入示例：`k230/uart_ball_sender_example.py`
- 主机自测：在本目录运行 `python test_protocol.py`

当前只加入“协议编解码层”，尚未切换现有诊断固件的 UART 中断，也不会在未接
好三板共地和交叉 TX/RX 前自动驱动车轮或步进电机。
