# 2025 E题离线软件基线

## 当前安全状态

配置文件：`user/app_config.h`

- 默认入口：`APP_PROFILE_E2025 = 1`
- 离线模拟：`E2025_OFFLINE_SIMULATION = 1`
- 循迹闭环适配器：`E2025_ENABLE_CASCADE_TRACKING = 0`
- 云台/激光安全适配器：`E2025_ENABLE_AIM_ADAPTER = 0`
- K230 UART0兼容桥：`E2025_ENABLE_K230_UART0_BRIDGE = 0`
- K230专用UART2：`E2025_ENABLE_K230_UART2 = 0`（PB15_TX/PA22_RX，待实物验证）
- 电机输出：强制为0
- 云台输出：仅生成逻辑命令，不连接STEP/DIR
- 激光输出：仅生成逻辑使能，不连接GPIO
- LED和TOF：保留为独立测试入口
- 循迹诊断：`APP_PROFILE_TRACK_DIAG = 0`，单独的安全诊断固件已生成
- 编码器诊断：`APP_PROFILE_ENCODER_DIAG = 0`，仅允许手动旋转车轮
- 电机诊断：`APP_PROFILE_MOTOR_DIAG = 0`，按住SW2才允许单轮PWM=600
- 双轮诊断：`APP_PROFILE_DUAL_MOTOR_DIAG = 0`，按住SW2才允许双轮相同PWM=600
- 双轮闭环诊断：`APP_PROFILE_DUAL_SPEED_DIAG = 0`，目标计数尚未实车标定

硬件回来并完成接线复核之前，不允许把离线模拟改为0。

八路循迹逐路检查方法见`docs/TRACK_DIAG_GUIDE.md`。诊断模式会把左右PWM设为0并将
PB16/STBY拉低，预编译固件位于`output/firmware/car_final_ccs_track_diag.out`。

编码器手动旋转检查见`docs/ENCODER_DIAG_GUIDE.md`，预编译固件位于
`output/firmware/car_final_ccs_encoder_diag.out`。

低PWM单轮测试见`docs/MOTOR_DIAG_GUIDE.md`。该模式会产生真实电机动作，必须车轮悬空；
预编译固件位于`output/firmware/car_final_ccs_motor_diag.out`。

双轮悬空同速测试见`docs/DUAL_MOTOR_DIAG_GUIDE.md`。该模式会监控左右编码器，检测到无反馈、
单侧掉线、方向错误、持续速度差或SW4急停后会锁存故障并拉低PB16/STBY；预编译固件位于
`output/firmware/car_final_ccs_dual_motor_diag.out`。

双轮编码器速度闭环准备见`docs/DUAL_SPEED_PI_GUIDE.md`。当前目标计数和PI参数只通过离线模型，
预编译固件使用`unvalidated`标记；完成双轮开环计数测量前不允许烧录运行。

## 六种任务

| 菜单 | 功能 | 时间上限 | 软件完成条件 |
|---|---|---:|---|
| E1 TRACK N | 循迹1～5圈 | 20s | 检测到设定圈数 |
| E2 AIM 2S | 静止快速瞄准 | 2s | 连续3帧像素误差在阈值内 |
| E3 AIM 4S | 搜索后瞄准 | 4s | 找到目标并连续3帧锁定 |
| E4 MOVE 1 | 运动瞄准1圈 | 20s | 完成1圈 |
| E5 MOVE 2 | 运动瞄准2圈 | 40s | 完成2圈 |
| E6 CIRCLE | 运动中画圆 | 20s | 小车1圈且K230报告画圆完成 |

像素阈值目前只是软件占位值，不能直接证明光斑物理误差小于2cm。硬件回来后必须根据相机距离和靶纸标定。

## K230二进制帧

固定15字节，小端序，115200 8N1：

| 字节 | 内容 |
|---:|---|
| 0 | `0xAA` |
| 1 | `0x55` |
| 2 | 协议版本，当前为`0x01` |
| 3 | 帧序号，每帧递增 |
| 4 | 标志：bit0靶点有效，bit1激光点有效，bit2画圆完成 |
| 5～6 | 靶点X，`int16` |
| 7～8 | 靶点Y，`int16` |
| 9～10 | 激光点X，`int16` |
| 11～12 | 激光点Y，`int16` |
| 13 | 画圆相位，0～255 |
| 14 | CRC8，计算范围为字节2～13，多项式`0x07` |

协议解析位于`user/aim_protocol.c`，链路快照和丢帧统计位于`user/k230_link.c`。默认UART0只服务板载CH340；K230专用UART2已分配为PB15_TX/PA22_RX，但功能开关保持关闭，等待真实接线验证。

## 离线测试

运行：

```powershell
python offline_tools\e2025_offline_sim.py
python offline_tools\e2025_track_sim.py
python offline_tools\motor_diag_safety_sim.py
python offline_tools\dual_motor_diag_safety_sim.py
python offline_tools\dual_speed_pi_sim.py
python offline_tools\track_cascade_sim.py
python offline_tools\e2025_drive_adapter_sim.py
python offline_tools\e2025_aim_adapter_sim.py
python offline_tools\k230_link_sim.py
python offline_tools\e2025_fault_manager_sim.py
python offline_tools\e2025_system_integration_sim.py
python offline_tools\hardware_acceptance_sim.py
python offline_tools\hw_accept_monitor_test.py
python offline_tools\uart_pin_plan_audit.py
```

第一项测试覆盖六个正常任务、视觉丢失超时、急停以及K230协议CRC损坏。

第二项测试覆盖八路循迹模块的低电平压线转换、左右纠偏方向、丢线搜索、
拐角噪声过滤、长拐角抖动防重复计数以及5圈20秒限制。回放明细输出到：

- `output/track_sim/summary.json`
- `output/track_sim/five_lap_replay.csv`

后三项依次验证单轮低PWM安全外壳、双轮编码器故障保护和双轮整数PI闭环。PI模型会回放
左右电机差异和右轮20%负载扰动，结果位于：

- `output/dual_motor_diag_sim/summary.json`
- `output/dual_speed_pi_sim/summary.json`
- `output/dual_speed_pi_sim/closed_loop_replay.csv`
- `output/track_cascade_sim/summary.json`
- `output/track_cascade_sim/joint_replay.csv`

循迹级联控制的结构、目标速度映射以及暂不接入正式电机输出的原因，见
`docs/TRACK_CASCADE_GUIDE.md`。

E2025循迹内部统一规定bit0～bit7对应D1～D8，bit为1表示传感器压到黑线。
原模块电平为白底=1、黑线=0，因此`xunji.c`只在新的加权循迹入口做低有效转换；
旧`track2/track3/track4`仍保留原始电平查表含义。

拐角事件按10ms控制周期过滤：连续20ms检测到D1/D2或D7/D8才计数，离开拐角
连续50ms后才重新允许下一次计数。

编译：

```powershell
python .agents\skills\mspm0kit-tianmengxing\scripts\build.py car_final_ccs --yes
```

## 硬件回来后的待确认项

1. 左右电机方向和编码器正负号。
2. 八路循迹极性以及拐角事件是否会重复触发。
3. K230 UART2（PB15/PA22）的方向、电平和115200通信。
4. 水平、垂直两轴STEP/DIR及限位。
5. 激光使能引脚、默认关闭电平和独立电源开关。
6. 像素误差到靶面厘米误差的标定。
7. 小车一圈与画圆一圈的相位同步。
8. 连续多次2s、4s、1圈、2圈实测成功率。
