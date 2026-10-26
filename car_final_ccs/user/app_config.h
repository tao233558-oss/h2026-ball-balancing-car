#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

/*
 * Select exactly one firmware entry profile.
 *
 * E2025 is the normal application. LED and TOF remain available as isolated
 * bring-up profiles. The TOF profile uses PB0/PB1 I2C and the ST VL53L0X API.
 */
#define APP_PROFILE_E2025      0
#define APP_PROFILE_LED_TEST   0
#define APP_PROFILE_TOF_TEST   0
#define APP_PROFILE_TRACK_DIAG 0
#define APP_PROFILE_TRACK_RUN  0
#define APP_PROFILE_ENCODER_DIAG 0
#define APP_PROFILE_MOTOR_DIAG 0
#define APP_PROFILE_DUAL_MOTOR_DIAG 0
#define APP_PROFILE_DUAL_SPEED_DIAG 0
#define APP_PROFILE_AIM_DIAG   0
#define APP_PROFILE_STEPPER_STANDALONE 0
#define APP_PROFILE_IR_ADC_DIAG 0
#define APP_PROFILE_H2026_B_UART_DIAG 0
#define APP_PROFILE_H2026_B_STEPPER_DIAG 1
#define APP_PROFILE_HW_ACCEPT  0

/*
 * Safe offline mode: exercise the E2025 state machine with synthetic sensor
 * and vision data, but never drive motors, stepper axes, or the laser.
 * Change to 0 only after the car is back and the output wiring is rechecked.
 */
#define E2025_OFFLINE_SIMULATION 1

/*
 * Feature-gated adapter: track target generation + 100 ms encoder PI.
 * Keep at 0 until the open-loop encoder counts and PI parameters are measured.
 */
#define E2025_ENABLE_CASCADE_TRACKING 0

/*
 * Pure-software aiming safety adapter. It never toggles STEP/DIR or laser
 * GPIO; enable only for integration testing until the final pins are known.
 */
#define E2025_ENABLE_AIM_ADAPTER 0

/*
 * The current two-axis mechanism has no physical limit switches. Keep all
 * four physical-limit fields false in the future GPIO backend. Motion must
 * use a reset-on-boot relative position and conservative software bounds.
 */
#define E2025_AIM_PHYSICAL_LIMITS_PRESENT 0

/*
 * TOF200F/VL53L0X telemetry on the shared PB0=SCL, PB1=SDA software-I2C bus.
 * The main loop polls continuous ranging and the 10 ms ISR reads only a
 * lock-free snapshot. Keep at 0 until the module has been switched to I2C.
 */
#define E2025_ENABLE_TOF200F 0

/*
 * Compatibility only: feed PA11/UART0 RX bytes into the K230 link parser.
 * Keep at 0 because UART0 is connected to the board CH340 debug interface.
 */
#define E2025_ENABLE_K230_UART0_BRIDGE 0

/*
 * Final PCB route: UART2_TX=PB15 and UART2_RX=PA22 for the K230 link.
 * Keep disabled until the K230-to-MSPM0 wiring has been checked on hardware.
 */
#define E2025_ENABLE_K230_UART2 0

#if E2025_ENABLE_K230_UART0_BRIDGE && E2025_ENABLE_K230_UART2
#error "Select only one K230 UART input path"
#endif

#if ((APP_PROFILE_E2025 + APP_PROFILE_LED_TEST + APP_PROFILE_TOF_TEST + \
      APP_PROFILE_TRACK_DIAG + APP_PROFILE_TRACK_RUN + \
      APP_PROFILE_ENCODER_DIAG + \
      APP_PROFILE_MOTOR_DIAG + APP_PROFILE_DUAL_MOTOR_DIAG + \
      APP_PROFILE_DUAL_SPEED_DIAG + APP_PROFILE_AIM_DIAG + \
      APP_PROFILE_STEPPER_STANDALONE + \
      APP_PROFILE_IR_ADC_DIAG + APP_PROFILE_H2026_B_UART_DIAG + \
      APP_PROFILE_H2026_B_STEPPER_DIAG + \
      APP_PROFILE_HW_ACCEPT) != 1)
#error "Select exactly one APP_PROFILE_* option"
#endif

#endif
