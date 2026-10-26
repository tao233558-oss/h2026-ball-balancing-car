# car_h2024_ccs

这是 2024 年全国大学生电子设计竞赛 H 题“自动行驶小车”的独立开发副本。
工程从 `car_final_ccs`（2025 年 E 题主控工程）复制而来；后续只修改本目录，原工程保持不变。

当前 H2024 固件复用用户提供的 25E PCB，但运行时只初始化 H 题需要的外设。K230、步进云台、TOF、蓝牙和激光代码仅作为历史基线保留，不会在 H2024 入口中初始化对应引脚。

## 当前 H2024 固件

- `APP_PROFILE_H2024 = 1`，启动后不会初始化 K230、步进机构、激光或 TOF。
- PCB `MODE/PA17` 选择任务，`SET/START/PA27` 启动；运行中按任意一个键都会立即停车，不使用 PB23/PB27；任务 2~4 暂时锁定。
- 任务 1 复用陀螺仪航向保持，默认 PWM 为 1200、运行 700 个 10ms 周期（约 7 秒）。
- 启动前必须收到 JY61P 的有效航向帧；运行中 IMU 超过 500ms 无新帧会停车。
- 正常完成、急停、IMU 丢失或 15 秒超时都会关闭 PWM 与 PB16 电机待机，声光提示由主循环触发。

天猛星 MSPM0G3507 小车工程，由原 Keil ArmClang 工程迁移到 Code Composer Studio / TI-Clang。

## 导入 CCS

1. 在 CCS 中选择 `File -> Import Project(s)`。
2. 选择本目录中的 `car_h2024_ccs.projectspec`。
3. 确认器件为 MSPM0G3507、编译器为 TI-Clang；实车使用 J-Link，导入后需把目标连接从旧 XDS110 配置切换为 SEGGER J-Link Emulator。
4. 安装并启用 MSPM0 SDK 2.00.01.00，或在升级 SDK 后重新运行 SysConfig 并处理版本提示。
5. 编译后检查 SysConfig 警告，再连接硬件。

## 迁移策略

- 保留原有 `user/` 业务层、`code/` PID 模块与 `ml_libs/` DriverLib 封装。
- 使用 CCS 的 TI-Clang 启动文件；调试器按实车改用 J-Link SWD。
- 原程序的主动时钟初始化仍在 `ml_libs/ml_system.c`，保持 80 MHz 时序行为。
- `car_h2024_ccs.syscfg` 负责 CCS 器件、封装及生成构建产物。
- Keil 专用的 `__FILE`、`__stdout`、`_sys_exit` 仅在 ArmCC 下编译。

## 天猛星安全引脚重映射

| 功能 | 旧引脚 | 新引脚 | 原因 |
|---|---|---|---|
| 左电机 PWM / TIMG6 CCP0 | PB6 | PA29 | PB6 被板载 W25Q64 占用 |
| 右电机 PWM / TIMG6 CCP1 | PB7 | PA30 | PB7 被板载 W25Q64 占用 |
| 巡线 D7 | 旧副本曾用 PB25 | PA22 | 与用户现用 25E PCB 的 D7/R3 网络一致 |
| 蜂鸣器 | PB8 | PB12 | PB8 属于板载 Flash/LCD 总线 |
| 指示灯 | PB9 | PB13 | PB9 属于板载 Flash/LCD 总线 |
| OLED 软件 I²C SCL/SDA | 旧副本曾用 PB0/PB1 | PB2/PB3 | 与用户现用 25E PCB 的 J5 OLED 网络一致 |
| 两个操作按键 | PA27/PA17/PB23/PB27 四键 | PA17 MODE、PA27 SET/START | PCB 只有两键；PB23/PB27 不初始化 |

H2024 入口不再调用 `uart0_init()`，因此不会为了 K230 或 CH340 初始化 PA10/PA11。PCB 上的板载 CH340 物理连接不受软件修改影响。当前接线基准为用户提供的 `Tianmengxing_MSPM0G3507_car_complete_wiring_v2(1).pdf`；其中 H 题不用的 K230、步进、TOF 等接口保持未初始化。配套裁剪版接线文档为 `output/pdf/H2024_MSPM0G3507_25E_PCB_wiring_v1_1.pdf`。

## 验证边界

当前迁移首先验证源代码、SysConfig 和编译链接。烧录成功不等于电机方向、传感器极性和实车运动已经验证；首次上电应架空车轮并限制电机功率。

## 2025 E题离线基线

`user/app_config.h` 是固件入口选择文件。当前默认配置为：

- `APP_PROFILE_E2025 = 1`：进入2025 E题六任务菜单。
- `APP_PROFILE_HW_ACCEPT = 0`：只读硬件验收入口，专用安全固件已单独生成。
- `E2025_OFFLINE_SIMULATION = 1`：使用模拟循迹和视觉数据。
- `E2025_ENABLE_CASCADE_TRACKING = 0`：循迹闭环接入保留但默认关闭。
- `E2025_ENABLE_AIM_ADAPTER = 0`：云台/激光纯软件安全适配器默认关闭。
- `E2025_ENABLE_K230_UART0_BRIDGE = 0`：UART0保留给板载CH340，不接K230解析器。
- `E2025_ENABLE_K230_UART2 = 0`：K230专用UART2已实现，但在实物接线验证前保持关闭。
- 离线模拟模式始终把电机、云台和激光输出强制关闭。
- LED和TOF测试仍保留为独立入口，不再覆盖完整主程序；TOF 已接入正式 VL53L0X I²C 驱动并生成独立固件，操作见`docs/TOF200F_I2C_TEST_GUIDE.md`。

小车硬件回来后，首次通电优先使用
`output/firmware/car_final_ccs_hw_accept_safe.out`。它会在电机PWM和PB16/STBY保持关闭的情况下，
依次检查四个按键、八路循迹和左右编码器；操作步骤见
`docs/HARDWARE_ACCEPTANCE_GUIDE.md`。K230虽然已分配UART2，但尚未实物验证，验收固件仍显示`BLOCKED`，不会假报通过。
电脑端可运行`python offline_tools\hw_accept_monitor.py --port COM3 --duration 180 --until-pass`
自动显示验收进度并保存CSV、JSON和原始串口日志；实际端口号以`--list`结果为准。

六种任务分别为循迹N圈、2秒静止瞄准、4秒搜索瞄准、运动瞄准1圈、运动瞄准2圈和运动画圆。状态机代码位于`user/e2025_app.c`，模拟输入位于`user/e2025_sim.c`。

顶层故障管理器位于`user/e2025_fault_manager.c`，会把编码器保护、云台急停和K230已连接后掉线映射到E2025状态与OLED故障文字。详细时序见`docs/E2025_FAULT_MANAGER_GUIDE.md`。

在实车回到手上之前不要把`E2025_OFFLINE_SIMULATION`改为0。切换到真实硬件模式前，必须重新确认两轴STEP/DIR、激光使能、K230 UART2、TOF/OLED I²C、蓝牙UART3、圈数检测以及电机方向。

双轴瞄准的硬件无关脉冲执行器已经加入 CCS 工程并通过 TI-Clang 编译，但尚未绑定 GPIO 或定时器。最终接线表目前只有 PA14=DIR、PA26=STEP/PUL 一套接口；第二轴、四个物理限位、激光使能与有效电平仍待定义。完整状态与接入条件见`docs/E2025_AIM_EXECUTOR_GUIDE.md`。

DCC-100v3 双路驱动资料已经完成交叉审核。厂家示例的 PA28/PA31/PB24/PB20/PA7 和 PA12/PA13/PA14/PA15/PA16 与本车大量冲突，不能照搬。当前无冲突推荐为第一路 PA26/PA14、第二路 PB4/PB5、两路 SLP 共用 PB17；RST 上拉、DCY 悬空自动衰减。详见`docs/DCC100V3_INTEGRATION_AUDIT.md`。

DCC-100v3 双轴低速诊断档已经加入，但默认 `APP_PROFILE_AIM_DIAG = 0`，正常 E2025 固件不会初始化 PA26/PA14/PB4/PB5/PB17。专用诊断版每次按键只输出 8 个低速脉冲，结束后立即让 PB17 进入休眠，激光始终不接入。操作见 `docs/DCC100V3_DIAG_GUIDE.md`。
