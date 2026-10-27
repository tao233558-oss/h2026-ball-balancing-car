# 数据集、训练与模型转换资料

已完整保存的训练参考是[steel_ball_k230_open_source](../../咸鱼24电赛h/K230识别钢球-使用说明请看README/steel_ball_k230_open_source/)。它属于旧YOLOv8钢球参考工程，不是队友最终YOLO11模型的训练记录。

| 阶段 | 现有文件 |
|---|---|
| 相机采集 | `camera_capture.py` |
| 原始图像与标注 | `steel_ball_dataset/`，图片和JSON/YOLO标签 |
| 划分与检查 | `steel_ball_yolo/`、`tools/prepare_yolo_dataset.py`、两个数据校验脚本 |
| 训练 | `tools/train_yolov8.py`、`requirements-train.txt` |
| 权重和导出 | `models/*.pt`、`models/*.onnx`、`tools/export_yolov8_onnx.py` |
| K230转换与校准 | `tools/convert_k230.ps1`、`tools/setup_kmodel_windows.ps1`、`steel_ball_yolo/calibration/` |
| KModel与参考部署 | `models/*.kmodel`、`k230_deploy/` |
| 历史训练结果 | `training_results/results.csv`、PR曲线、混淆矩阵和损失图 |

参考项目自述52张原始图片，训练32张、验证8张，另有12张待复核。检查和训练时应进入该参考项目目录，按其中README执行；所需训练环境和nncase转换依赖未作为可执行环境打包。

该目录记录的较高验证指标只针对小规模参考数据，不应写成整车实拍精度、现场成功率，或本人独立训练的YOLO11成果。照片主要来自拍摄显示器中的商品图，和管道内实物钢球存在域差异。训练CSV最后一轮也不等于最佳权重对应的轮次，不应把不同轮次/权重混在同一结论里。

最终训练迭代应围绕固定相机位置采集真实管道、运动模糊、遮挡、反光及无球负样本，按拍摄场景划分数据，并在板端测漏检、误检、帧龄与控制效果。该步骤属于后续工作，不在本次GitHub整理中伪造训练或板端测试结果。

参考代码/模型保留其目录内的AGPL-3.0许可证和第三方说明；本次新增目录不改写原始许可，也不把队友训练成果计入本人独立贡献。
