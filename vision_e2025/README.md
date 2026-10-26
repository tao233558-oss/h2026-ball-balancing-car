# 2025 E题电脑端视觉原型

该目录用于在没有庐山派、摄像头、云台和小车时提前验证视觉闭环。它不是最终K230固件，但算法输入输出已经按照K230移植边界拆分。

## 当前功能

- 按A4尺寸建立靶纸几何模型。
- 从摄像头画面中寻找黑色A4外框。
- 透视校正后利用红色同心圆估计靶心。
- 在靶面内寻找蓝紫色激光光斑。
- 计算光斑到靶心的像素误差。
- 生成半径6cm圆上的参考点。
- 生成与天猛星`aim_protocol.c`一致的15字节数据帧。
- 合成旋转、透视、亮度、噪声和模糊测试场景。

## 运行

在总工程目录执行：

```powershell
python -m vision_e2025.generate_target_pdf
python -m vision_e2025.run_offline_demo
```

输出：

- `output/pdf/E2025_A4_vision_practice_target.pdf`
- `output/vision_e2025/contact_sheet.png`
- `output/vision_e2025/cases/`
- `output/vision_e2025/rectified/`
- `output/vision_e2025/summary.json`
- `output/vision_e2025/sample_protocol_frame.txt`

## 重要边界

合成图通过只能证明算法结构和协议正确，不能证明真实405nm激光在紫外感光纸上的颜色阈值正确。硬件回来后需要重新采集真实图片，调整曝光、黑框阈值、红色阈值和紫色阈值。

练习靶PDF打印时必须选择“实际大小/100%”，关闭“适合页面”。它只能用于摄像头视觉练习，正式测试仍需按赛题要求使用A4紫外感光纸、黑色胶带和红色记号笔。
