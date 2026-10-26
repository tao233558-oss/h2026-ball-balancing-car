#include "dual_motor_guard.h"

#define DUAL_MOTOR_GRACE_SAMPLES                 5U
#define DUAL_MOTOR_NO_FEEDBACK_SAMPLES          10U
#define DUAL_MOTOR_SIDE_MISSING_SAMPLES          3U
#define DUAL_MOTOR_DIRECTION_FAULT_SAMPLES       3U
#define DUAL_MOTOR_MISMATCH_SAMPLES              5U
#define DUAL_MOTOR_ACTIVITY_MIN_COUNTS           2U
#define DUAL_MOTOR_MISMATCH_MIN_COUNTS           4U
#define DUAL_MOTOR_MISMATCH_PERCENT             50U

static uint32_t dual_motor_guard_abs_i32(int32_t value)
{
    if(value < 0)
    {
        return (uint32_t) (-(value + 1)) + 1U;
    }
    return (uint32_t) value;
}

static uint8_t dual_motor_guard_increment_if(uint8_t value,
                                             uint8_t condition)
{
    if(!condition)
    {
        return 0U;
    }
    return (value < 255U) ? (uint8_t) (value + 1U) : value;
}

void dual_motor_guard_reset(dual_motor_guard_t *guard)
{
    guard->run_samples = 0U;
    guard->no_feedback_samples = 0U;
    guard->left_missing_samples = 0U;
    guard->right_missing_samples = 0U;
    guard->mismatch_samples = 0U;
    guard->left_direction_samples = 0U;
    guard->right_direction_samples = 0U;
}

dual_motor_diag_fault_t dual_motor_guard_step(
    dual_motor_guard_t *guard,
    uint8_t reverse,
    int32_t left_delta,
    int32_t right_delta)
{
    return dual_motor_guard_step_targets(
        guard, reverse, left_delta, right_delta, 1U, 1U);
}

dual_motor_diag_fault_t dual_motor_guard_step_targets(
    dual_motor_guard_t *guard,
    uint8_t reverse,
    int32_t left_delta,
    int32_t right_delta,
    uint32_t left_target,
    uint32_t right_target)
{
    uint32_t left_abs = dual_motor_guard_abs_i32(left_delta);
    uint32_t right_abs = dual_motor_guard_abs_i32(right_delta);
    uint32_t left_scaled = left_abs * right_target;
    uint32_t right_scaled = right_abs * left_target;
    uint32_t maximum = (left_scaled > right_scaled) ?
                       left_scaled : right_scaled;
    uint32_t difference = (left_scaled > right_scaled) ?
                          (left_scaled - right_scaled) :
                          (right_scaled - left_scaled);
    uint8_t left_commanded = (left_target > 0U) ? 1U : 0U;
    uint8_t right_commanded = (right_target > 0U) ? 1U : 0U;
    uint8_t left_active = (left_abs >= DUAL_MOTOR_ACTIVITY_MIN_COUNTS) ? 1U : 0U;
    uint8_t right_active = (right_abs >= DUAL_MOTOR_ACTIVITY_MIN_COUNTS) ? 1U : 0U;
    uint8_t left_wrong = left_commanded && left_active &&
        ((!reverse && left_delta < 0) || (reverse && left_delta > 0));
    uint8_t right_wrong = right_commanded && right_active &&
        ((!reverse && right_delta < 0) || (reverse && right_delta > 0));
    uint8_t both_commanded = left_commanded && right_commanded;
    uint8_t mismatch = both_commanded && left_active && right_active &&
        ((left_abs > right_abs ? left_abs : right_abs) >=
         DUAL_MOTOR_MISMATCH_MIN_COUNTS) &&
        ((difference * 100U) > (maximum * DUAL_MOTOR_MISMATCH_PERCENT));

    if(guard->run_samples < DUAL_MOTOR_GRACE_SAMPLES)
    {
        guard->run_samples++;
        return DUAL_MOTOR_FAULT_NONE;
    }

    guard->no_feedback_samples = dual_motor_guard_increment_if(
        guard->no_feedback_samples,
        both_commanded && !left_active && !right_active);
    guard->left_missing_samples = dual_motor_guard_increment_if(
        guard->left_missing_samples,
        left_commanded && !left_active &&
        ((!right_commanded) || right_active));
    guard->right_missing_samples = dual_motor_guard_increment_if(
        guard->right_missing_samples,
        right_commanded && !right_active &&
        ((!left_commanded) || left_active));
    guard->left_direction_samples = dual_motor_guard_increment_if(
        guard->left_direction_samples, left_wrong);
    guard->right_direction_samples = dual_motor_guard_increment_if(
        guard->right_direction_samples, right_wrong);
    guard->mismatch_samples = dual_motor_guard_increment_if(
        guard->mismatch_samples, mismatch);

    if(guard->no_feedback_samples >= DUAL_MOTOR_NO_FEEDBACK_SAMPLES)
    {
        return DUAL_MOTOR_FAULT_NO_FEEDBACK;
    }
    if(guard->left_missing_samples >= DUAL_MOTOR_SIDE_MISSING_SAMPLES)
    {
        return DUAL_MOTOR_FAULT_LEFT_MISSING;
    }
    if(guard->right_missing_samples >= DUAL_MOTOR_SIDE_MISSING_SAMPLES)
    {
        return DUAL_MOTOR_FAULT_RIGHT_MISSING;
    }
    if(guard->left_direction_samples >=
       DUAL_MOTOR_DIRECTION_FAULT_SAMPLES)
    {
        return DUAL_MOTOR_FAULT_LEFT_DIRECTION;
    }
    if(guard->right_direction_samples >=
       DUAL_MOTOR_DIRECTION_FAULT_SAMPLES)
    {
        return DUAL_MOTOR_FAULT_RIGHT_DIRECTION;
    }
    if(guard->mismatch_samples >= DUAL_MOTOR_MISMATCH_SAMPLES)
    {
        return DUAL_MOTOR_FAULT_SPEED_MISMATCH;
    }
    return DUAL_MOTOR_FAULT_NONE;
}

const char *dual_motor_fault_name(dual_motor_diag_fault_t fault)
{
    switch(fault)
    {
        case DUAL_MOTOR_FAULT_ESTOP:          return "E-STOP";
        case DUAL_MOTOR_FAULT_NO_FEEDBACK:    return "NO FEEDBACK";
        case DUAL_MOTOR_FAULT_LEFT_MISSING:   return "LEFT MISSING";
        case DUAL_MOTOR_FAULT_RIGHT_MISSING:  return "RIGHT MISSING";
        case DUAL_MOTOR_FAULT_SPEED_MISMATCH: return "SPEED DIFF";
        case DUAL_MOTOR_FAULT_LEFT_DIRECTION: return "LEFT DIR";
        case DUAL_MOTOR_FAULT_RIGHT_DIRECTION:return "RIGHT DIR";
        default:                              return "NONE";
    }
}
