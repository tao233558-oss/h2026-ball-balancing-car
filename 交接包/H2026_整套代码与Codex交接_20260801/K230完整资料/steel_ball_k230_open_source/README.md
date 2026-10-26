# K230/K230D YOLOv8 钢球识别

这是一个从数据标注、YOLOv8n 训练、ONNX/KModel 转换到 CanMV 实时部署的
完整示例。当前部署程序已在以下环境验证摄像头初始化：

- 开发板：庐山派 Lite / K230D
- 固件：CanMV v1.8
- 摄像头编号：`SENSOR_ID = 2`
- 显示：ST7701 LCD
- 模型输入：320×320

## 最快上板运行

仓库已经包含转换完成的 KModel，不需要先训练。

在 CanMV IDE 中创建：

```text
/sdcard/steel_ball/
```

复制：

```text
k230_deploy/main.py
  -> /sdcard/steel_ball/main.py

k230_deploy/steel_ball_yolov8n_320.kmodel
  -> /sdcard/steel_ball/steel_ball_yolov8n_320.kmodel
```

打开板上的 `/sdcard/steel_ball/main.py` 并运行。屏幕会显示检测框、置信度、
钢球数量和最高置信度钢球的中心点；串口每 10 帧输出一次：

```text
[steel_ball] count=1 best_center=(320,180) score=0.832
```

其他开发板找不到摄像头时，可将 `SENSOR_ID` 改为 `None` 自动探测，或尝试
实际存在的摄像头编号。

## 目录结构

```text
.
├─ steel_ball_dataset/       原始图片、X-AnyLabeling JSON、YOLO 标签
├─ steel_ball_yolo/          可直接训练的 train/val/review_pending 划分
├─ tools/                    数据校验、训练、导出、KModel 转换和验证脚本
├─ models/                   PT、ONNX、KModel
├─ k230_deploy/              CanMV 部署程序和 KModel
├─ training_results/         训练曲线、混淆矩阵和 results.csv
├─ docs/                     详细中文部署说明
├─ camera_capture.py         K230 按键拍照采集脚本
└─ requirements-train.txt
```

未包含 X-AnyLabeling 安装程序、Python 虚拟环境、训练缓存、第三方转换工具和
调试中间图。标注时请自行安装 X-AnyLabeling，并打开
`steel_ball_dataset/`。

## 数据集

- 类别：`steel_ball`，类别编号 `0`
- 原始图片：52 张，1920×1080
- 总标注框：756 个
- 正式训练：32 张，120 个框
- 验证：8 张，20 个框
- 待人工复核：12 张，616 个框，不参与当前训练
- nncase 校准图片：20 张

待复核图片位于 `steel_ball_yolo/images/review_pending/`。这些图片包含密集、
遮挡或模糊钢球，公开贡献前应使用 X-AnyLabeling 再次核对。

校验数据：

```powershell
python tools/validate_steel_ball_annotations.py
python tools/validate_yolo_dataset.py
```

## 重新训练

Windows PowerShell：

```powershell
python -m venv .venv-yolo
.\.venv-yolo\Scripts\python.exe -m pip install -r requirements-train.txt

.\.venv-yolo\Scripts\python.exe tools/train_yolov8.py `
  --model yolov8n.pt `
  --device cpu `
  --workers 0
```

带 NVIDIA CUDA 的训练机可把 `--device cpu` 改为 `--device 0`。

训练结果默认写入：

```text
runs/steel_ball/yolov8n_320/weights/best.pt
```

当前正式模型的验证集结果：

- Precision：0.9520
- Recall：0.9930
- mAP50：0.9926
- mAP50-95：0.7538

验证集只有 8 张图片，这些指标不能代替真实现场测试。

## 导出 ONNX

```powershell
.\.venv-yolo\Scripts\python.exe tools/export_yolov8_onnx.py `
  runs/steel_ball/yolov8n_320/weights/best.pt
```

K230 使用静态 batch 1、320×320、opset 12 ONNX。

## 转换 KModel

Windows 环境准备：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File tools/setup_kmodel_windows.ps1
```

转换：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File tools/convert_k230.ps1 `
  -Python .\.venv-yolo\Scripts\python.exe `
  -OnnxPath .\models\steel_ball_yolov8n_320.onnx
```

转换使用 nncase 2.11.0 和 K230 PTQ。生成结果自动复制到
`k230_deploy/steel_ball_yolov8n_320.kmodel`。

主机侧验证：

```powershell
.\.venv-yolo\Scripts\python.exe -B tools/validate_kmodel.py `
  models/steel_ball_yolov8n_320.onnx `
  k230_deploy/steel_ball_yolov8n_320.kmodel `
  steel_ball_yolo/calibration/000000.jpg
```

当前 ONNX/KModel 输出余弦相似度为 `0.99995252`，验证结果 PASS。

## 已知限制

现有图片主要是相机拍摄显示器上的钢球商品图，与真实生产现场存在域差异。
若要稳定识别实体钢球，建议用最终安装位置的 K230 摄像头补充：

- 至少 200 张包含真实钢球的图片；
- 至少 50 张无钢球背景图；
- 灯、螺钉、轴承、圆孔和强反光等负样本；
- 不同距离、数量、遮挡、曝光和运动模糊。

使用 `camera_capture.py` 采集，重新标注并迭代训练。

## 许可证与发布前检查

本仓库代码和模型按 `LICENSE` 中的 GNU AGPL-3.0 发布。Ultralytics 官方说明
其训练代码和训练得到的模型默认受 AGPL-3.0 约束；闭源或商业用途应自行确认
是否需要 Ultralytics Enterprise License。

数据集图片的可再分发权利取决于图片来源。将仓库发布到 GitHub 前，请确认你
拥有全部图片或已获得公开再分发授权。更多说明见
`THIRD_PARTY_NOTICES.md`。

