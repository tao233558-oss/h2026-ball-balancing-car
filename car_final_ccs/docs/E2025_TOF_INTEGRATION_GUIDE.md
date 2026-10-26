# E2025 主工程 TOF200F 集成说明

## 当前结论

TOF200F 已以只读遥测方式接入 E2025 主工程，尚不参与循迹、瞄准、停车或故障判定。这样可以先验证传感器和整车程序能否长期同时运行，再根据实际题目决定距离用途与阈值。

- 接口：软件 I2C，PB0=SCL、PB1=SDA，与 OLED 共用总线。
- 器件：TOF200F 的 VL53L0X，7 位地址 `0x29`。
- 测量：连续测距；主循环每 50 ms 非阻塞查询一次结果。
- 实时边界：10 ms `TIMG8_IRQHandler` 只读取一次 32 位快照，不执行 I2C 初始化、测距或延时。
- 有效性：`RangeStatus == 0` 才标记距离有效；500 ms 没有新数据后自动失效。
- 安全性：默认 `E2025_ENABLE_TOF200F = 0`；`E2025_OFFLINE_SIMULATION = 1` 时电机仍被强制关闭。

## 首次实机使用前

1. 先用 USB 转串口把模块从默认 UART 模式切换到 I2C 模式，HEX 指令为：
   `01 06 00 09 00 01 98 08`
2. 断电后按以下方式接线：TOF VCC→3V3、GND→GND、SCL→PB0、SDA→PB1。
3. 先烧录独立诊断固件 `output/firmware/car_final_ccs_tof200f_i2c_test.out`，确认能稳定输出距离。
4. 再烧录集成安全固件 `output/firmware/car_final_ccs_e2025_tof_safe.out`。该固件保持离线仿真和电机禁用，只验证 TOF 与 E2025 共存。
5. 只有独立诊断和集成安全固件都通过后，才讨论把 `E2025_OFFLINE_SIMULATION` 改为 0。

## 状态字段

`e2025_status_t` 中可读取：

- `tof_present`：模块已返回过可识别状态。
- `distance_valid`：当前距离可用且未超时。
- `tof_range_status`：VL53L0X 测距状态；`0` 表示有效，`0xFE` 表示 I2C/API 错误，`0xFF` 表示不可用。
- `distance_mm`：毫米距离。
- `distance_age_ticks`：距离上次新样本经过的 10 ms tick 数。

## 验证边界

当前只完成源代码审计、离线模型和 CCS 编译验证。硬件不在手边，因此模块模式、总线波形、实际距离、共享 I2C 稳定性和长时间运行仍标记为 `NOT_RUN`。
