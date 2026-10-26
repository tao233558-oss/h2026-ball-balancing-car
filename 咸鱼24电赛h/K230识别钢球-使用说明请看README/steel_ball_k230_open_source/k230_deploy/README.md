# K230 钢球识别部署包

## 上板文件

在 CanMV IDE 中创建 `/sdcard/steel_ball/`，然后复制：

```text
本目录/main.py
  -> /sdcard/steel_ball/main.py

本目录/steel_ball_yolov8n_320.kmodel
  -> /sdcard/steel_ball/steel_ball_yolov8n_320.kmodel
```

打开板上的 `/sdcard/steel_ball/main.py` 并点击运行。首次测试不要设置
上电自启，确认相机和屏幕工作正常后再设置。

## 当前硬件配置

- 摄像头编号：`2`（庐山派 Lite / K230D、CanMV v1.8 已验证）
- 显示屏：ST7701 LCD（`lcd`）
- 图像方向：水平镜像 + 垂直翻转
- 模型输入：`320×320`
- 推理图像：`640×360`
- 类别：`steel_ball`
- 置信度阈值：`0.50`
- NMS 阈值：`0.45`

程序使用 CanMV 官方 `YOLOv8` 类绘制检测框和置信度，同时显示钢球数量和
最高置信度钢球的中心点；串口每 10 帧输出一次同样的数据。

如果图像方向不正确，修改 `H_MIRROR` 或 `V_FLIP`。其他开发板找不到
摄像头时，可把 `SENSOR_ID` 改成 `None` 自动探测。

如需上电自启，把同一脚本另存为 `/sdcard/main.py`，不要修改
`MODEL_PATH`。

该 KModel 由 nncase 2.11.0 生成。板上固件必须使用兼容的 nncase
runtime；如果出现 `invalid kmodel`，请先升级到与 nncase 2.11.0 对应的
CanMV 固件。
