# MSPM0-A / MSPM0-B 接入要求

## 1. 工程接入边界

本目录的 `h2026_uart_protocol_v2.c/.h` 是纯协议代码，不依赖具体 UART 实例。

已经确认 A、B 都是 MSPM0G3507，8路循迹红外接在B，摆杆使用42步进电机。具体驱动、PID和
机构参考由单片机队友负责；本目录只约束通信字段和接收边界。

拿到实际 CCS 工程后：

1. 查找工程 `.syscfg`、入口 `main.c` 和生成的 `ti_msp_dl_config.h`；
2. 在 `.syscfg` 中配置 UART、GPIO、Timer 和中断；
3. 重新生成后读取真实的 `*_INST`、`*_IRQn`、GPIO 宏；
4. 把协议文件加入工程；
5. 在 UART RX ISR 中只把字节放入环形缓冲区；
6. 在 1 ms 调度中从缓冲区取字节，调用 `h2026_parser_feed()`；
7. 不手工修改生成的 `ti_msp_dl_config.c/.h`。

## 2. B 的推荐任务

| 周期/触发 | 工作 |
|---|---|
| UART RX ISR | 收字节进环形缓冲区，溢出置故障 |
| 步进 Timer ISR | 产生 STEP 脉冲，不计算 PID |
| 1 ms | 喂协议解析器、更新各链路 age、状态机 |
| 10 ms | 球位滤波、滚球 PID/PD、限幅限速 |
| 10 ms | 读取/整理已经接在 B 上的 8 路红外 |
| 10 ms | 向 A 排队发送一次 `B_STATUS` |

解析 K230 数据时：

- 合法 `VISION_SAMPLE` 刷新 `vision_rx_age_ms`；
- 只有 `VALID=1`、`sample_age_ms<=120` 且 `frame_id` 是新值，才更新球位滤波器；
- `VALID=0` 的合法包说明链路正常但球不可用；
- 重复 `frame_id` 不重复计算速度和微分；
- CRC、版本、长度或消息类型错误不刷新任何安全计时。

## 3. A 的推荐任务

| 周期/触发 | 工作 |
|---|---|
| 车辆编码器/速度环 | 使用现有硬件周期，建议 500 Hz 或更高 |
| 5-10 ms | 循迹误差处理和转向/差速目标 |
| 10 ms | 发送 `A_COMMAND` |
| 1 ms | 解析 `B_STATUS`、更新通信 age 和任务状态 |

A 只有在以下条件都成立时才允许进入行驶：

- B 状态为 `READY` 或 `ACTIVE`；
- B 按自己的机构方案上报 `ACTUATOR_REFERENCE_VALID=1`；
- `BALL_VALID=1`；
- `fault_bits=0`；
- 球在目标附近稳定至少 300 ms。

B 状态超过 200 ms 未更新时，A 应使用斜坡降低车辆速度目标，不能继续盲跑。

## 4. 可选的参考建立方式

当前球槽两端只有机械挡片，没有电气零位开关。若单片机队友以后增加开关，可参考：

1. 上电时输出禁止；
2. 收到 A 的 `HOME + ENABLE`；
3. 以低速向零位方向移动；
4. 去抖确认开关触发；
5. 反向退出少量步数，再低速二次接近；
6. 触发后设置 `actuator_position_steps=0`；
7. 移动标定的 `HOME_TO_LEVEL_STEPS`；
8. 进入 `READY`。

整个过程必须有最大步数和最大时间。超出后置 `REFERENCE_INIT_TIMEOUT`，禁止继续运动。

若B采用人工水平初始化或其他方案，只需在位置参考可信后置
`ACTUATOR_REFERENCE_VALID`；K230不参与这项判断。

## 5. 仍需实际工程决定的参数

以下值不要写死在公共协议层：

- UART 实例和 GPIO；
- CPU/Timer 时钟；
- STEP 脉冲频率范围；
- DIR 正方向；
- 归零方向和有效电平；
- `HOME_TO_LEVEL_STEPS`；
- 软件最小/最大步数；
- PID 参数、滤波参数；
- 电机驱动器使能极性。

这些参数应放在 B 工程的应用配置文件中，并用台架试验记录确定。
