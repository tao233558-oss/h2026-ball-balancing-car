#ifndef __E2025_DRIVE_ADAPTER_H__
#define __E2025_DRIVE_ADAPTER_H__

#include <stdint.h>

#include "dual_motor_guard.h"
#include "track_cascade_control.h"
#include "wheel_speed_pi.h"

#define E2025_DRIVE_SAMPLE_TICKS 10U

typedef struct
{
    uint8_t active;
    uint8_t sample_updated;
    int32_t left_target_counts;
    int32_t right_target_counts;
    int32_t left_sample_counts;
    int32_t right_sample_counts;
    int32_t left_pwm;
    int32_t right_pwm;
    track_cascade_mode_t track_mode;
    dual_motor_diag_fault_t fault;
} e2025_drive_output_t;

typedef struct
{
    track_cascade_controller_t track;
    wheel_speed_pi_t left_pi;
    wheel_speed_pi_t right_pi;
    dual_motor_guard_t guard;
    int32_t left_accumulator;
    int32_t right_accumulator;
    int32_t left_pwm;
    int32_t right_pwm;
    int32_t previous_left_target;
    int32_t previous_right_target;
    uint8_t sample_ticks;
    uint8_t was_enabled;
    dual_motor_diag_fault_t fault;
} e2025_drive_adapter_t;

void e2025_drive_adapter_init(e2025_drive_adapter_t *adapter);
void e2025_drive_adapter_tick(e2025_drive_adapter_t *adapter,
                              uint8_t track_enable,
                              uint8_t emergency_stop,
                              uint8_t track_pattern,
                              int32_t weighted_error,
                              int32_t left_delta_10ms,
                              int32_t right_delta_10ms,
                              e2025_drive_output_t *output);

#endif
