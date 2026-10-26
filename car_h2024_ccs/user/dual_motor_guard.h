#ifndef __DUAL_MOTOR_GUARD_H__
#define __DUAL_MOTOR_GUARD_H__

#include <stdint.h>

typedef enum
{
    DUAL_MOTOR_FAULT_NONE = 0,
    DUAL_MOTOR_FAULT_ESTOP,
    DUAL_MOTOR_FAULT_NO_FEEDBACK,
    DUAL_MOTOR_FAULT_LEFT_MISSING,
    DUAL_MOTOR_FAULT_RIGHT_MISSING,
    DUAL_MOTOR_FAULT_SPEED_MISMATCH,
    DUAL_MOTOR_FAULT_LEFT_DIRECTION,
    DUAL_MOTOR_FAULT_RIGHT_DIRECTION
} dual_motor_diag_fault_t;

typedef struct
{
    uint8_t run_samples;
    uint8_t no_feedback_samples;
    uint8_t left_missing_samples;
    uint8_t right_missing_samples;
    uint8_t mismatch_samples;
    uint8_t left_direction_samples;
    uint8_t right_direction_samples;
} dual_motor_guard_t;

void dual_motor_guard_reset(dual_motor_guard_t *guard);
dual_motor_diag_fault_t dual_motor_guard_step(
    dual_motor_guard_t *guard,
    uint8_t reverse,
    int32_t left_delta,
    int32_t right_delta);
dual_motor_diag_fault_t dual_motor_guard_step_targets(
    dual_motor_guard_t *guard,
    uint8_t reverse,
    int32_t left_delta,
    int32_t right_delta,
    uint32_t left_target,
    uint32_t right_target);
const char *dual_motor_fault_name(dual_motor_diag_fault_t fault);

#endif
