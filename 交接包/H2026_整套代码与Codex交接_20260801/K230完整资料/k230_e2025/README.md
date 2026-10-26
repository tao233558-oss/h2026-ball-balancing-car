# 庐山派 K230 视觉程序

`main.py` 负责识别 A4 靶面、靶心和紫色激光点，并通过 UART2 向天猛星发送 15 字节视觉帧。下列三个文件必须一起复制到 K230：

- `main.py`：摄像头、识别、显示和 UART 主循环。
- `aim_protocol.py`：与 MSPM0 一致的 CRC8 和 15 字节编码。
- `target_geometry.py`：A4 透视靶心与半径 6 cm 圆周计算。

## 最终 UART2 接线

FPIOA 使用 GPIO 编号，不是 40 针排针的物理针号：

- 庐山派物理 11 / GPIO5 / UART2_TX → 天猛星 PA22 / UART2_RX。
- 天猛星 PB15 / UART2_TX → 庐山派物理 13 / GPIO6 / UART2_RX。
- 两块板必须共地，信号均为 3.3 V TTL，波特率 115200、8N1。

不要把代码中的 GPIO5/GPIO6 写成 11/13，也不要接到天猛星 PA10/PA11；PA10/PA11 属于板载 CH340。

## 模式命令

天猛星会向 K230 发送：

- `STOP\n`：停止模式，强制关闭激光许可。
- `CENTER\n`：E2～E5 靶心模式。
- `CIRCLE\n`：E6 半径 6 cm 画圆模式。

重复的 `CIRCLE` 心跳不会把正在运行的圆周相位清零。

## 默认安全状态

```python
AUTO_LASER_ENABLE = False
```

因此当前脚本不会自动打开 GPIO33 激光输出。确认激光驱动极性、限流和人员安全措施之前，不要改为 `True`。

## 无硬件验证

在工程根目录运行：

```powershell
python offline_tools\k230_e2025_end_to_end.py
```

该测试会校验 K230 编码、MSPM0 接收、E2～E6 帧流、CRC、丢帧、超时和恢复。真实摄像头曝光、色彩阈值、UART 波形和激光输出仍必须在硬件上标定。

透视几何可单独验证：

```powershell
python offline_tools\k230_geometry_sim.py
```

实机颜色和曝光标定步骤见 `car_final_ccs/docs/K230_VISION_CALIBRATION_GUIDE.md`。
