#include "e2025_drive_adapter.h"

#define E2025_DRIVE_FEEDFORWARD_PWM 600
#define E2025_DRIVE_MINIMUM_PWM     400
#define E2025_DRIVE_MAXIMUM_PWM     900
#define E2025_DRIVE_KP               18
#define E2025_DRIVE_KI                6
#define E2025_DRIVE_INTEGRATOR_LIMIT 100

static const wheel_speed_pi_config_t g_e2025_drive_pi_config =
{
    E2025_DRIVE_FEEDFORWARD_PWM,
    E2025_DRIVE_MINIMUM_PWM,
    E2025_DRIVE_MAXIMUM_PWM,
    E2025_DRIVE_KP,
    E2025_DRIVE_KI,
    E2025_DRIVE_INTEGRATOR_LIMIT
};

static uint32_t e2025_drive_abs_i32(int32_t value)
{
    if(value < 0)
    {
        return (uint32_t) (-(value + 1)) + 1U;
    }
    return (uint32_t) value;
}

static void e2025_drive_clear_output(e2025_drive_output_t *output,
                                      dual_motor_diag_fault_t fault)
{
    output->active = 0U;
    output->sample_updated = 0U;
    output->left_target_counts = 0;
    output->right_target_counts = 0;
    output->left_sample_counts = 0;
    output->right_sample_counts = 0;
    output->left_pwm = 0;
    output->right_pwm = 0;
    output->track_mode = TRACK_CASCADE_FOLLOW;
    output->fault = fault;
}

static void e2025_drive_start_session(e2025_drive_adapter_t *adapter)
{
    track_cascade_init(&adapter->track, &g_track_cascade_default_config);
    wheel_speed_pi_reset(&adapter->left_pi);
    wheel_speed_pi_reset(&adapter->right_pi);
    dual_motor_guard_reset(&adapter->guard);
    adapter->left_accumulator = 0;
    adapter->right_accumulator = 0;
    adapter->left_pwm = E2025_DRIVE_FEEDFORWARD_PWM;
    adapter->right_pwm = E2025_DRIVE_FEEDFORWARD_PWM;
    adapter->previous_left_target = 0;
    adapter->previous_right_target = 0;
    adapter->sample_ticks = 0U;
    adapter->was_enabled = 1U;
}

void e2025_drive_adapter_init(e2025_drive_adapter_t *adapter)
{
    track_cascade_init(&adapter->track, &g_track_cascade_default_config);
    wheel_speed_pi_init(&adapter->left_pi, &g_e2025_drive_pi_config);
    wheel_speed_pi_init(&adapter->right_pi, &g_e2025_drive_pi_config);
    dual_motor_guard_reset(&adapter->guard);
    adapter->left_accumulator = 0;
    adapter->right_accumulator = 0;
    adapter->left_pwm = 0;
    adapter->right_pwm = 0;
    adapter->previous_left_target = 0;
    adapter->previous_right_target = 0;
    adapter->sample_ticks = 0U;
    adapter->was_enabled = 0U;
    adapter->fault = DUAL_MOTOR_FAULT_NONE;
}

void e2025_drive_adapter_tick(e2025_drive_adapter_t *adapter,
                              uint8_t track_enable,
                              uint8_t emergency_stop,
                              uint8_t track_pattern,
                              int32_t weighted_error,
                              int32_t left_delta_10ms,
                              int32_t right_delta_10ms,
                              e2025_drive_output_t *output)
{
    track_cascade_output_t track_output;

    e2025_drive_clear_output(output, adapter->fault);
    if(track_enable && emergency_stop &&
       adapter->fault == DUAL_MOTOR_FAULT_NONE)
    {
        adapter->fault = DUAL_MOTOR_FAULT_ESTOP;
    }

    if(!track_enable)
    {
        adapter->left_accumulator = 0;
        adapter->right_accumulator = 0;
        adapter->left_pwm = 0;
        adapter->right_pwm = 0;
        adapter->sample_ticks = 0U;
        adapter->was_enabled = 0U;
        output->fault = adapter->fault;
        return;
    }
    if(adapter->fault != DUAL_MOTOR_FAULT_NONE)
    {
        output->fault = adapter->fault;
        return;
    }
    if(!adapter->was_enabled)
    {
        e2025_drive_start_session(adapter);
    }

    track_cascade_step(&adapter->track, track_pattern, weighted_error,
                       &track_output);

    /* A commanded stop takes effect on this 10 ms tick, not at the next PI sample. */
    if(track_output.left_target_counts == 0)
    {
        adapter->left_pwm = 0;
        wheel_speed_pi_reset(&adapter->left_pi);
    }
    else if(adapter->previous_left_target == 0)
    {
        adapter->left_pwm = E2025_DRIVE_FEEDFORWARD_PWM;
        wheel_speed_pi_reset(&adapter->left_pi);
    }
    if(track_output.right_target_counts == 0)
    {
        adapter->right_pwm = 0;
        wheel_speed_pi_reset(&adapter->right_pi);
    }
    else if(adapter->previous_right_target == 0)
    {
        adapter->right_pwm = E2025_DRIVE_FEEDFORWARD_PWM;
        wheel_speed_pi_reset(&adapter->right_pi);
    }
    adapter->previous_left_target = track_output.left_target_counts;
    adapter->previous_right_target = track_output.right_target_counts;

    adapter->left_accumulator += left_delta_10ms;
    adapter->right_accumulator += right_delta_10ms;
    adapter->sample_ticks++;

    output->active = 1U;
    output->left_target_counts = track_output.left_target_counts;
    output->right_target_counts = track_output.right_target_counts;
    output->left_pwm = adapter->left_pwm;
    output->right_pwm = adapter->right_pwm;
    output->track_mode = track_output.mode;
    output->fault = adapter->fault;

    if(adapter->sample_ticks < E2025_DRIVE_SAMPLE_TICKS)
    {
        return;
    }

    output->sample_updated = 1U;
    output->left_sample_counts = adapter->left_accumulator;
    output->right_sample_counts = adapter->right_accumulator;
    adapter->fault = dual_motor_guard_step_targets(
        &adapter->guard,
        0U,
        adapter->left_accumulator,
        adapter->right_accumulator,
        (uint32_t) track_output.left_target_counts,
        (uint32_t) track_output.right_target_counts);

    if(adapter->fault == DUAL_MOTOR_FAULT_NONE)
    {
        adapter->left_pwm = wheel_speed_pi_step(
            &adapter->left_pi,
            track_output.left_target_counts,
            (int32_t) e2025_drive_abs_i32(adapter->left_accumulator));
        adapter->right_pwm = wheel_speed_pi_step(
            &adapter->right_pi,
            track_output.right_target_counts,
            (int32_t) e2025_drive_abs_i32(adapter->right_accumulator));
    }
    else
    {
        adapter->left_pwm = 0;
        adapter->right_pwm = 0;
        output->active = 0U;
    }

    adapter->left_accumulator = 0;
    adapter->right_accumulator = 0;
    adapter->sample_ticks = 0U;
    output->left_pwm = adapter->left_pwm;
    output->right_pwm = adapter->right_pwm;
    output->fault = adapter->fault;
}
