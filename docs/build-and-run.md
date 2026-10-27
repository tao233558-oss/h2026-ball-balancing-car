# 编译、运行与切换

## STM32 工程

1. 在 Windows 安装 Keil MDK，归档构建使用 Arm Compiler 5.06 update6。安装 STM32F4 器件支持和 ST-Link 驱动。
2. 打开 `H2026_STM32F407/STM32F407_ProjectTemplate/project/MDK(V5)/Project.uvprojx`。
3. 选择 `Project` target，先执行 Rebuild，不先接通电机功率。
4. 默认 `app/main.c` 为 V17 滚球固件；通过 SWD 烧录时使用 ST-Link。
5. 先检查 OLED 的版本、按键和接线，再做各功能验收。仓库未假定克隆后无需器件包即可编译。

命令行示例（Keil路径按实际安装位置设置）：

```powershell
& 'E:\Keil5\UV4\UV4.exe' -r '.\H2026_STM32F407\STM32F407_ProjectTemplate\project\MDK(V5)\Project.uvprojx' -t 'Project' -o '.\rebuild.log'
```

仓库保留历史编译与烧录日志。新的构建结果应与它们区分。

## 切换为循迹

对工程做工作副本，备份 `app/main.c` 后，将以下任一文件复制覆盖为该副本的 `app/main.c`，再 Rebuild：

- `app/main_stage41_4ch_line_v20_flowing_curve.c.saved`：通用四路循迹。
- `app/main_stage49_v1_fixed_route.c.saved`：固定跑道里程分段循迹。

不要把多个历史主程序同时加进 Keil target，否则会重复定义 main。固定路线里的8220、20500、28430等阈值是当前车体的编码器累计量；改轮径、传感器位置、地图或起点后必须重标。

## 滚球操作

先完成水平参考：LEVEL状态用SW1/SW2点动，SW3建立参考。视觉有效时进入中心保持，按提示由SW1启动题3序列，SW4急停。钢球应向右x增、向左x减；取走球后须验证收帧超时。若视觉仍不断发送旧帧，先修复视觉端再测试自动控制。

## MSPM0 历史工程

`car_final_ccs` 和 `car_h2024_ccs` 使用各自的 `.projectspec` 导入 CCS，按各目录 README 安装 MSPM0 SDK/SysConfig。它们是复用和迁移阶段记录，不使用 STM32 接线表。

## K230视觉入口

视觉代码、RTSP、模型来源与部署步骤见[vision/README.md](../vision/README.md)。本次新增ASCII整理入口与STM32 V17格式匹配，未重新上板测试；YOLO11权重尚需从队友TF备份补齐。旧YOLOv8参考模型齐全，保留为独立参考，不改名替代YOLO11。

## 离线检查

```text
python tests/verify_repository.py
python h2026_uart_protocol/test_protocol.py
python tests/host_checks.py
```

host_checks需要gcc或clang，用于编译实际的可移植控制/解析源码。上述历史二进制协议测试证明旧协议打包/解析一致，不代表V17正在使用该协议。V17使用ASCII解析器。GitHub Actions只运行无需硬件的检查，不代表实车验收。offline_tools中的视觉几何仿真还需额外安装OpenCV和NumPy。
