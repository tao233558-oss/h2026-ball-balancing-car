#ifndef __E2025_AIM_EXECUTOR_H__
#define __E2025_AIM_EXECUTOR_H__

#include <stdint.h>

/*
 * Logical pulse scheduler timing. A future hardware timer must call
 * e2025_aim_executor_tick() once every 100 us from one execution context.
 */
#define E2025_AIM_EXECUTOR_TICK_US 100U
#define E2025_AIM_DIR_SETUP_TICKS  2U
#define E2025_AIM_STEP_HIGH_TICKS  1U
#define E2025_AIM_STEP_LOW_TICKS   1U
#define E2025_AIM_QUEUE_LIMIT      16

typedef enum
{
    E2025_AIM_EXECUTOR_FAULT_NONE = 0,
    E2025_AIM_EXECUTOR_FAULT_ESTOP
} e2025_aim_executor_fault_t;

typedef enum
{
    E2025_AIM_AXIS_IDLE = 0,
    E2025_AIM_AXIS_DIR_SETUP,
    E2025_AIM_AXIS_STEP_HIGH,
    E2025_AIM_AXIS_STEP_LOW
} e2025_aim_axis_phase_t;

typedef struct
{
    uint8_t emergency_stop;
    uint8_t pan_negative_limit;
    uint8_t pan_positive_limit;
    uint8_t tilt_negative_limit;
    uint8_t tilt_positive_limit;
} e2025_aim_executor_safety_t;

typedef struct
{
    uint8_t enable;
    uint8_t laser_request;
    int8_t pan_steps;
    int8_t tilt_steps;
} e2025_aim_executor_command_t;

typedef struct
{
    int16_t pending_steps;
    int8_t active_direction;
    uint8_t phase_ticks;
    uint8_t step_active;
    e2025_aim_axis_phase_t phase;
    int32_t emitted_position_steps;
} e2025_aim_executor_axis_t;

typedef struct
{
    e2025_aim_executor_axis_t pan;
    e2025_aim_executor_axis_t tilt;
    uint8_t enabled;
    uint8_t laser_requested;
    uint8_t queue_overflow;
    uint8_t reversal_deferred;
    e2025_aim_executor_fault_t fault;
} e2025_aim_executor_t;

typedef struct
{
    uint8_t busy;
    uint8_t driver_awake_allowed;
    uint8_t laser_allowed;
    uint8_t pan_step_active;
    uint8_t tilt_step_active;
    uint8_t pan_limit_blocked;
    uint8_t tilt_limit_blocked;
    uint8_t queue_overflow;
    uint8_t reversal_deferred;
    int8_t pan_direction;
    int8_t tilt_direction;
    int16_t pan_pending_steps;
    int16_t tilt_pending_steps;
    int32_t pan_emitted_position_steps;
    int32_t tilt_emitted_position_steps;
    e2025_aim_executor_fault_t fault;
} e2025_aim_executor_output_t;

void e2025_aim_executor_init(e2025_aim_executor_t *executor);
void e2025_aim_executor_abort(e2025_aim_executor_t *executor);
void e2025_aim_executor_submit(
    e2025_aim_executor_t *executor,
    const e2025_aim_executor_command_t *command);
void e2025_aim_executor_tick(
    e2025_aim_executor_t *executor,
    const e2025_aim_executor_safety_t *safety,
    e2025_aim_executor_output_t *output);

#endif
