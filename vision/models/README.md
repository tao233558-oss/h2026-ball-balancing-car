# 模型与依赖清单

| 文件 | 现状 | 用途 |
|---|---|---|
| `yolo11_gq_320.kmodel` | 当前归档缺失 | 新整理YOLO11入口所需，须从同学最终TF备份补齐 |
| `../k230/yolo11_gq_classes.txt` | 已有，类别GQ | 对应YOLO11标签文件，不能单独证明模型存在 |
| `steel_ball_yolov8n_320.pt` | 原始参考项目已入库 | 旧YOLOv8训练权重 |
| `steel_ball_yolov8n_320.onnx` | 原始参考项目已入库 | 旧YOLOv8导出图 |
| `steel_ball_yolov8n_320.kmodel` | 原始参考项目已入库 | 旧YOLOv8参考部署权重，不能改名充当YOLO11 |

旧模型实际位置：[原项目models目录](../../咸鱼24电赛h/K230识别钢球-使用说明请看README/steel_ball_k230_open_source/models/)。不为展示目录重复增加大文件。

已核对SHA256：

```text
PT      4f7a83b4449caae8804fa5ba906ca8637355deacf8fa665b61ec9e37e226f991
ONNX    8c171d6a13e77f95d5012d004a0286d0d41bfb6c9d37e92a681a9e715ed5ba8b
KMODEL  dd5ea6aa80280733f93043ac0691ac09625a4dcf66ba6fa47bd28e0391e2c7a6
```

`tools/verify_vision_assets.py`检查上述参考模型的哈希，并报告YOLO11模型是否存在。使用`--require-yolo11`可在部署检查中把缺权重视为失败；默认归档检查允许该明确记录的缺项。

补齐YOLO11模型时，应同时记录其SHA256、模型输入尺寸、类别顺序、转换工具/runtime版本和实际板端程序。不要仅凭文件名判断兼容性。
