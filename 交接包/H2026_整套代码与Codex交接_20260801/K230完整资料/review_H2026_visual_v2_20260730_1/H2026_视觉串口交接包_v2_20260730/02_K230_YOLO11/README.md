# K230 YOLO11 接入说明

当前已经运行的程序和模型：

```text
/sdcard/main.py
/sdcard/yolo11_gq_320.kmodel
/sdcard/yolo11_gq_classes.txt
```

对应电脑侧工作文件是 `D:\TUCHUAN\main.py`。本 v2 包没有直接覆盖该运行文件，原因是 UART 的
最终 GPIO/接线仍需确认，而且正式烧写前应先完成假数据串口测试。

接入时把以下两个文件复制到 K230 脚本同一目录：

```text
01_协议/h2026_uart_protocol_v2.py
02_K230_YOLO11/vision_uart_publisher.py
```

`vision_uart_publisher.py` 使用已经确定的 `GPIO11 -> UART2_TXD`，只配置单向发送。对应庐山派
标准版带锁GH1.25串口2座的 `T` 脚；同座 `G` 接B的GND，`R`和`V`首版不接。

选择依据是庐山派官方UART文档明确推荐该带锁串口2座，且UART4的部分排针与摄像头I²C或音频复用。
没有GH配套线时，才把 `tx_gpio` 改为5并使用40Pin物理11脚。两种接法不能同时使用。参考：
[庐山派UART文档](https://wiki.lckfb.com/zh-hans/lushan-pi-k230/basic/uart.html)。

## 1. YOLO11 结果读取

检测结果必须按分组数组读取：

```python
boxes = result[0]
class_ids = result[1]
scores = result[2]

for index in range(len(boxes)):
    x1, y1, x2, y2 = boxes[index]
    class_id = class_ids[index]
    score = scores[index]
```

候选球需经过以下筛选：

- 类别是钢球；
- `score >= 0.50`，后续可根据测试再调；
- 球心处于摆杆 ROI；
- 框宽、高和宽高比合理；
- 多个候选时不能只盲选最高置信度，应结合上一帧位置连续性；
- 两个候选同样合理且无法区分时，发送 `VALID=0`。

## 2. 串口发布与推理解耦

维护两类计数：

- `sequence`：每发送一个串口包加 1；
- `frame_id`：每完成一次新 YOLO 推理才加 1。

每个最新测量还要保存：

```text
x_mm, confidence, flags, frame_id, inference_ms, result_timestamp_ms
```

发送时计算：

```python
sample_age_ms = time.ticks_diff(time.ticks_ms(), result_timestamp_ms)
```

若发送线程重复发送同一个推理结果，`sequence` 变化而 `frame_id` 不变。B 因此能区分：

- 串口仍在线但尚无新推理；
- 串口已经断线；
- 新推理到达但球无效。

正式程序可先在主循环末尾使用非阻塞的 20 ms 截止时间调度发送。若实测受 `yolo.run()` 阻塞而
低于 25 Hz，再单独验证低优先级发送线程；不要在 `Timer` 中断回调里打包、分配内存或写 UART。

主循环接入方式：

```python
from vision_uart_publisher import VisionUartPublisher

vision_uart = VisionUartPublisher(tx_gpio=11)

# 每次获得并筛选 YOLO11 结果后：
vision_uart.update_measurement(
    x_mm=x_mm,
    confidence_percent=int(score * 100),
    detected=detected,
    calibrated=calibration_ready,
    in_roi=in_roi,
    single_target=single_target,
    inference_ms=inference_ms,
)

# 每轮主循环末尾调用；不会为了补发历史周期而突发发送：
vision_uart.poll()
```

## 3. 图传性能原则

- AI 输入继续使用 320x180，模型输入 320x320 letterbox；
- 摄像头硬件 30 fps，RTSP 可保持 20 fps、约 1200 kbps；
- `gc.collect()` 每 10 帧一次，不要每帧执行；
- 不在每帧打印推理耗时；
- UART 每包最多 18 字节，负载很小，不是图传卡顿的主要来源；
- RTSP 启停、VLC 是否连接都不改变视觉串口状态机。

## 4. 标定输出

运行时 LCD 建议只保留：

```text
ball_x_mm / confidence / valid
vision fps / inference_ms / UART sample_age
RTSP URL / link state
```

标定完成后把标定参数、模型 SHA-256、固件版本和相机方向写入独立配置文件，并在程序启动时打印，
避免队友换模型或镜像后仍沿用旧参数。
