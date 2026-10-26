# car_final_ccs

天猛星 MSPM0G3507 小车工程，由原 Keil ArmClang 工程迁移到 Code Composer Studio / TI-Clang。

## 导入 CCS

1. 在 CCS 中选择 `File -> Import Project(s)`。
2. 选择本目录中的 `car_final_ccs.projectspec`。
3. 确认器件为 MSPM0G3507、连接为 XDS110、编译器为 TI-Clang。
4. 安装并启用 MSPM0 SDK 2.00.01.00，或在升级 SDK 后重新运行 SysConfig 并处理版本提示。
5. 编译后检查 SysConfig 警告，再连接硬件。

## 迁移策略

- 保留原有 `user/` 业务层、`code/` PID 模块与 `ml_libs/` DriverLib 封装。
- 使用 CCS 的 TI-Clang 启动文件、链接配置和 XDS110 连接。
- 原程序的主动时钟初始化仍在 `ml_libs/ml_system.c`，保持 80 MHz 时序行为。
- `car_final_ccs.syscfg` 负责 CCS 器件、封装及生成构建产物；旧 Keil 生成的 `ti_msp_dl_config.c/.h` 未复制。
- Keil 专用的 `__FILE`、`__stdout`、`_sys_exit` 仅在 ArmCC 下编译。

## 天猛星安全引脚重映射

| 功能 | 旧引脚 | 新引脚 | 原因 |
|---|---|---|---|
| 左电机 PWM / TIMG6 CCP0 | PB6 | PA29 | PB6 被板载 W25Q64 占用 |
| 右电机 PWM / TIMG6 CCP1 | PB7 | PA30 | PB7 被板载 W25Q64 占用 |
| 巡线 D7 | PA2（中间版曾用 PA22，后曾计划 PA3） | PB25（PCB与固件冻结目标） | 真正天猛星排针没有PA3；PB25已引出；固件已同步并通过CCS编译 |
| 蜂鸣器 | PB8 | PB12 | PB8 属于板载 Flash/LCD 总线 |
| 指示灯 | PB9 | PB13 | PB9 属于板载 Flash/LCD 总线 |
| 步进电机脉冲 | PA18 | PA26 | PA18 是 BSL 启动引脚 |
| OLED + TOF 软件 I²C SCL/SDA | PB2/PB3（更早为PB6/PB7） | PB0/PB1 | 释放PB2/PB3给蓝牙UART3 |
| 蓝牙 UART TX/RX | PB4/PB5 | PB2/PB3 | PB4/PB5曾与TOF临时测试冲突 |
| K230 UART2 TX/RX | 未冻结 | PB15/PA22 | 使用独立UART2，不再与CH340共线 |

UART0 仍使用 PA10/PA11并连接板载CH340。K230最终分配到UART2：PB15_TX/PA22_RX，默认`E2025_ENABLE_K230_UART2 = 0`，硬件验证后才开启。完整PCB变更见`docs/UART_PIN_CONFLICT_AUDIT.md`。

画板请只使用 `output/pdf/Tianmengxing_MSPM0G3507_car_complete_wiring_v4.pdf`。旧 `v2.pdf`、`v3.pdf` 均已作废。v4.0 在 v3.2 基线上加入 DCC-100v3 双轴云台：PAN=PA26/PA14、TILT=PB4/PB5、两路 SLP 共用 PB17，并明确不安装机械限位。D7 仍为 PB25；实板需逐路验收。

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
