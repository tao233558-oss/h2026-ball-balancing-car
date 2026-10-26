#include "ball_beam_controller.h"

#define Q10_ONE 1024L

static int32_t abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static int32_t clamp_i32(int32_t value,
                         int32_t minimum,
                         int32_t maximum)
{
    if(value < minimum)
    {
        return minimum;
    }
    if(value > maximum)
    {
        return maximum;
    }
    return value;
}

static int32_t move_toward_zero(int32_t value, int32_t amount)
{
    if(value > amount)
    {
        return value - amount;
    }
    if(value < -amount)
    {
        return value + amount;
    }
    return 0;
}

static int32_t signed_gain(int32_t input,
                           int16_t numerator,
                           int16_t denominator)
{
    if(denominator <= 0)
    {
        return 0;
    }
    return (input * numerator) / denominator;
}

void ball_beam_controller_init(ball_beam_controller_t *controller,
                               int16_t initial_target_steps)
{
    controller->assist_q10 = 0;
    controller->target_q10 = (int32_t)initial_target_steps * Q10_ONE;
    controller->last_error_mm = 0;
    controller->initialized = 1U;
}

void ball_beam_controller_reset_level(
    ball_beam_controller_t *controller)
{
    controller->assist_q10 = 0;
    controller->target_q10 = 0;
    controller->last_error_mm = 0;
    controller->initialized = 1U;
}

int16_t ball_beam_controller_update(
    ball_beam_controller_t *controller,
    const ball_beam_params_t *params,
    int16_t ball_x_mm,
    int16_t ball_velocity_mm_s,
    int16_t setpoint_mm,
    uint16_t dt_ms)
{
    int32_t error_mm;
    int32_t abs_error;
    int32_t assist_step_q10;
    int32_t assist_limit_q10;
    int32_t desired_steps;
    int32_t desired_q10;
    int32_t slew_q10;
    uint8_t moving_toward_center;

    if(controller->initialized == 0U)
    {
        ball_beam_controller_init(controller, 0);
    }

    if(dt_ms < 5U)
    {
        dt_ms = 5U;
    }
    else if(dt_ms > 500U)
    {
        dt_ms = 500U;
    }

    error_mm = (int32_t)ball_x_mm - setpoint_mm;
    abs_error = abs_i32(error_mm);

    /* A reversal must earn its own breakaway assistance; old assistance in
     * the opposite direction is unsafe and is discarded immediately. */
    if(((controller->last_error_mm > params->position_deadband_mm) &&
        (error_mm < -params->position_deadband_mm)) ||
       ((controller->last_error_mm < -params->position_deadband_mm) &&
        (error_mm > params->position_deadband_mm)))
    {
        controller->assist_q10 = 0;
    }

    moving_toward_center =
        (((error_mm > 0) &&
          (ball_velocity_mm_s < -params->quiet_velocity_mm_s)) ||
         ((error_mm < 0) &&
          (ball_velocity_mm_s > params->quiet_velocity_mm_s))) ? 1U : 0U;

    if(moving_toward_center != 0U)
    {
        /* The breakaway pulse has done its job.  Remove it immediately so
         * actuator return and velocity damping can brake the ball before it
         * crosses the centre. */
        controller->assist_q10 = 0;
    }
    else if(abs_error <= params->position_deadband_mm)
    {
        assist_step_q10 =
            ((int32_t)params->assist_decay_steps_s * dt_ms * Q10_ONE) /
            1000L;
        controller->assist_q10 =
            move_toward_zero(controller->assist_q10,
                             assist_step_q10);
    }
    else
    {
        assist_step_q10 =
            ((int32_t)params->assist_rate_steps_s * dt_ms * Q10_ONE) /
            1000L;
        if(error_mm > 0)
        {
            controller->assist_q10 += assist_step_q10;
        }
        else if(error_mm < 0)
        {
            controller->assist_q10 -= assist_step_q10;
        }
    }

    assist_limit_q10 =
        (int32_t)params->assist_limit_steps * Q10_ONE;
    controller->assist_q10 =
        clamp_i32(controller->assist_q10,
                  -assist_limit_q10,
                  assist_limit_q10);

    if((abs_error <= params->position_deadband_mm) &&
       (abs_i32(ball_velocity_mm_s) <=
        params->quiet_velocity_mm_s))
    {
        desired_steps = 0;
    }
    else
    {
        desired_steps =
            signed_gain(error_mm,
                        params->kp_num,
                        params->kp_den) +
            signed_gain(ball_velocity_mm_s,
                        params->kd_num,
                        params->kd_den) +
            (controller->assist_q10 / Q10_ONE);
    }
    desired_steps =
        clamp_i32(desired_steps,
                  -params->command_limit_steps,
                  params->command_limit_steps);
    desired_q10 = desired_steps * Q10_ONE;

    slew_q10 =
        ((int32_t)params->target_slew_steps_s * dt_ms * Q10_ONE) /
        1000L;
    if(controller->target_q10 < desired_q10)
    {
        controller->target_q10 += slew_q10;
        if(controller->target_q10 > desired_q10)
        {
            controller->target_q10 = desired_q10;
        }
    }
    else if(controller->target_q10 > desired_q10)
    {
        controller->target_q10 -= slew_q10;
        if(controller->target_q10 < desired_q10)
        {
            controller->target_q10 = desired_q10;
        }
    }

    controller->target_q10 =
        clamp_i32(controller->target_q10,
                  -(int32_t)params->command_limit_steps * Q10_ONE,
                  (int32_t)params->command_limit_steps * Q10_ONE);
    controller->last_error_mm = (int16_t)error_mm;

    return (int16_t)(controller->target_q10 / Q10_ONE);
}

int16_t ball_beam_controller_get_assist_steps(
    const ball_beam_controller_t *controller)
{
    return (int16_t)(controller->assist_q10 / Q10_ONE);
}
