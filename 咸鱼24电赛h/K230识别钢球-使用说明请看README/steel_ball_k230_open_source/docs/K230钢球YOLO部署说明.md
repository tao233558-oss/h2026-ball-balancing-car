# K230 钢球 YOLOv8 训练与部署

## 当前工程状态

已经完成：

1. 52 张图片的 X-AnyLabeling 与 YOLO 标注；
2. 原始标注一致性校验；
3. 仅使用 `checked` 图片生成训练集和验证集；
4. YOLOv8n 训练、ONNX 导出、KModel 转换入口；
5. K230 CanMV 实时检测、计数和中心点输出程序。

当前已经完成正式训练，并生成、验证了：

```text
models/steel_ball_yolov8n_320.pt
models/steel_ball_yolov8n_320.onnx
```

ONNX 输入为 `1×3×320×320`，输出为 `1×5×2100`，opset 12，类别名为
`steel_ball`。当前只差使用 nncase 生成 `.kmodel`。

## 1. 数据集

训练配置：

```text
steel_ball_yolo/steel_ball.yaml
```

默认数据：

- train：32 张，120 个框；
- val：8 张，20 个框；
- review_pending：12 张，616 个框，不参与第一版训练；
- nncase calibration：20 张训练图片。

重新生成和校验：

```powershell
python tools/prepare_yolo_dataset.py
python tools/validate_steel_ball_annotations.py
python tools/validate_yolo_dataset.py
```

生成脚本拒绝向非空输出目录写入，防止新旧划分意外混合。项目移动到其他
位置后，请使用新的 `--output` 目录重新运行一次，以刷新 YAML 中的绝对路径。

## 2. 训练 YOLOv8n

建议使用 Python 3.10 或 3.11。带 NVIDIA GPU 的训练机速度会明显更快。

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install -r requirements-train.txt
python tools/train_yolov8.py --device 0
```

没有 NVIDIA GPU 时可以使用 CPU：

```powershell
python tools/train_yolov8.py --device cpu --workers 2
```

默认参数与 K230 官方 YOLOv8 检测路线一致：`yolov8n.pt`、`imgsz=320`、
`epochs=300`。正式训练在第 152 轮提前停止，最佳结果来自第 72 轮：

- Precision：0.9520
- Recall：0.9930
- mAP50：0.9926
- mAP50-95：0.7538

规范化后的正式权重位于：

```text
models/steel_ball_yolov8n_320.pt
```

训练后先检查验证指标并用未参与训练的真实照片试推理：

```powershell
yolo detect val model=runs/steel_ball/yolov8n_320/weights/best.pt data=steel_ball_yolo/steel_ball.yaml imgsz=320
yolo detect predict model=runs/steel_ball/yolov8n_320/weights/best.pt source=你的真实测试图片目录 imgsz=320 conf=0.35
```

不要只看训练集结果。钢球存在强反光，误检通常来自灯、螺钉、圆孔和高光。

## 3. 导出 ONNX

```powershell
python tools/export_yolov8_onnx.py models/steel_ball_yolov8n_320.pt
```

当前已经导出静态 batch 1、320×320、opset 12 的：

```text
models/steel_ball_yolov8n_320.onnx
```

## 4. 转换 KModel

嘉楠当前官方流程使用 nncase 2.11.0，并要求安装匹配的
`nncase-kpu`。Windows 还需要 .NET 7。

运行准备脚本。它会把 .NET 7 安装到工程内的 `.dotnet`，安装 nncase
2.11.0 和匹配的 nncase-kpu wheel，并下载官方 YOLOv8 转换器：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File tools/setup_kmodel_windows.ps1
```

准备完成后转换：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File tools/convert_k230.ps1 `
  -Python .\.venv-yolo\Scripts\python.exe `
  -OnnxPath .\models\steel_ball_yolov8n_320.onnx
```

校准目录含 20 张图片，官方脚本固定读取其中前 5 张，输入为 320×320，
`ptq_option=1`（NoClip + uint8/int16）。转换已经成功，结果为：

```text
k230_deploy/steel_ball_yolov8n_320.kmodel
```

KModel 文件大小为 6,438,200 字节，SHA-256：

```text
DD5EA6AA80280733F93043AC0691AC09625A4DCF66BA6FA47BD28E0391E2C7A6
```

使用 nncase Windows 模拟器与 ONNX 比对：

- 输出形状：两者均为 `1×5×2100`
- 余弦相似度：0.99995252
- 结果：PASS

nncase 版本必须与板上 CanMV 固件的 nncase runtime 兼容。优先使用最新稳定
CanMV 固件以及其文档指定的转换版本；出现 “invalid kmodel” 时首先检查版本，
不要先改后处理代码。

## 5. 部署到 K230

通过 CanMV IDE 复制：

```text
k230_deploy/main.py
    -> /sdcard/steel_ball/main.py

k230_deploy/steel_ball_yolov8n_320.kmodel
    -> /sdcard/steel_ball/steel_ball_yolov8n_320.kmodel
```

在 IDE 中运行 `/sdcard/steel_ball/main.py`。程序当前按已有庐山派代码配置：

- `SENSOR_ID = 2`（庐山派 Lite / K230D、CanMV v1.8 已验证）
- ST7701 LCD
- 水平镜像和垂直翻转
- 640×360 相机推理流
- 320×320 模型输入
- 置信度 0.50，NMS 0.45

屏幕会显示检测框、置信度、钢球数量和最高置信度钢球的中心点；串口每 10
帧输出一次数量、中心点和置信度。

如需上电自启，把同一脚本保存为 `/sdcard/main.py`，模型路径保持不变。

## 6. 要达到现场可用，必须补真实相机数据

现有 52 张图片主要是相机拍摄显示器上的钢球商品图，和 K230 实际面对的
钢球、工作台、光源、运动模糊之间存在明显域差异。第一版模型适合验证
“训练 → 转换 → 板端推理”链路，不能据此承诺现场精度。

使用现有 `camera_capture.py` 从最终安装位置继续采集，建议至少：

- 200 张包含真实钢球的图片；
- 50 张完全没有钢球的背景图；
- 不同距离、数量、遮挡、亮度和曝光；
- 加入灯、螺钉、圆孔、轴承等容易误检的负样本；
- train/val 必须按拍摄场景分组，避免同一连拍同时进入两边。

没有钢球的图片也要保留，其 YOLO 标签是同名空 `.txt`。用这些真实图片
重新标注、训练并在 K230 实拍验证；把误检和漏检画面继续加入下一轮训练，
通常两到三轮后才会得到稳定的现场模型。
