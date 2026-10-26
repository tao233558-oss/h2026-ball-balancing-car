# K230 与 E2025 端到端通信说明

## 已补齐的链路

E2025 主工程现在通过专用 UART2 双向通信：

- K230→MSPM0：连续发送 15 字节视觉帧，包含靶心、光斑、有效标志和画圆相位。
- MSPM0→K230：发送 ASCII 模式命令 `STOP\n`、`CENTER\n`、`CIRCLE\n`。
- 命令在任务/状态变化时立即发送，并每 1 秒发送一次心跳。
- UART 发送只在主循环执行；10 ms 中断只设置心跳标志。

任务映射：

| E2025 状态/任务 | 发给 K230 的命令 |
|---|---|
| 空闲、完成、超时、故障、E1 纯循迹 | `STOP` |
| E2、E3、E4、E5 | `CENTER` |
| E6 运动画圆 | `CIRCLE` |

K230 只在第一次由其他模式进入 `CIRCLE` 时把相位清零。后续 1 秒心跳不会打断正在执行的圆周。

## 最终接线

FPIOA 使用 GPIO 编号，而排针使用物理针号，两者不能混用：

| 方向 | 庐山派 K230 | 天猛星 MSPM0 |
|---|---|---|
| K230→MSPM0 | 物理 11 / GPIO5 / UART2_TX | PA22 / UART2_RX |
| MSPM0→K230 | 物理 13 / GPIO6 / UART2_RX | PB15 / UART2_TX |
| 公共参考 | GND | GND |

两端必须使用 3.3 V TTL，并交叉连接 TX/RX。不要接到天猛星 PA10/PA11；它们属于板载 CH340。

## 无硬件离线回放

在工程根目录执行：

```powershell
python offline_tools\k230_e2025_end_to_end.py
```

测试会比较 K230、电脑视觉原型和 MSPM0 参考编码器的逐字节输出，并回放 E2～E6、坏 CRC、丢帧、断线和恢复。结果与二进制帧流位于：

- `output/k230_e2025_end_to_end/summary.json`
- `output/k230_e2025_end_to_end/e2_center.bin`
- `output/k230_e2025_end_to_end/e3_search_then_lock.bin`
- `output/k230_e2025_end_to_end/e4_e5_moving_center.bin`
- `output/k230_e2025_end_to_end/e6_circle.bin`

## 硬件回来后的顺序

1. 保持电机、云台和激光断开，先烧录 `car_final_ccs_e2025_k230_safe.out`。
2. 确认 UART2 是 115200、8N1，并检查 K230 物理 11→PA22、PB15→物理 13、GND 共地。
3. 在菜单切换 E1/E2/E6，观察 K230 是否依次收到 `STOP/CENTER/CIRCLE`。
4. 检查 CRC 正确帧、丢帧计数和 200 ms 掉线状态。
5. 通信稳定后再接云台；激光最后接入，并继续保持 K230 脚本中的 `AUTO_LASER_ENABLE = False`，直到确认极性和安全流程。

当前只完成代码、离线回放和 CCS 编译验证；真实 UART 电平、波形和图像阈值仍为 `NOT_RUN`。
