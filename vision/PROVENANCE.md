# 整理版来源与变更

| 新入口 | 原始来源 | 处理 |
|---|---|---|
| `k230/main.py` | `output/k230_deploy_gpio5_v2/main.py` | 保留采集、YOLO11、LCD和RTSP；换成当前9600 ASCII接口，使用统一绝对中心点 |
| `k230/SmoothWBCRtsp.py` | `output/k230_deploy_gpio5_v2/libs/SmoothWBCRtsp.py` | 原样复制，便于直接部署；无算法重写 |
| `k230/yolo11_gq_classes.txt` | `output/k230_sdcard_backup_20260730_1330/yolo11_gq_classes.txt` | 原样复制 |
| `k230/target_selection.py` | 原入口中的最高置信度筛选逻辑 | 分离为可测试函数，增加无效框、分数及范围校验 |
| `k230/vision_ascii_uart.py` | 依据同学UART交接说明与STM32实际解析器新增 | 绝对像素[x,y]，50/100 ms节流，无目标静默 |
| 模型与训练索引 | `咸鱼24电赛h/K230识别钢球-使用说明请看README/steel_ball_k230_open_source` | 引用已上传原文件，保留许可证 |
| 视觉照片 | 用户提供的2026-07历史LCD与OLED照片 | 原图复制，不当成本次上板结果 |

整套交接快照未修改，原来的`source-manifest.csv`继续核对其历史内容。本次新增文件由后续Git提交追踪。最终TF主程序、预测模块及YOLO11权重未找回，未用旧模型冒充，也未伪造训练或板端日志。
