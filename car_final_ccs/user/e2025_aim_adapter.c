#include "e2025_aim_adapter.h"

static void e2025_aim_clear_output(const e2025_aim_adapter_t *adapter,
                                    e2025_aim_output_t *output)
{
    output->active = 0U;
    output->vision_recent = 0U;
    output->laser_allowed = 0U;
    output->pan_limit_blocked = 0U;
    output->tilt_limit_blocked = 0U;
    output->pan_steps = 0;
    output->tilt_steps = 0;
    output->pan_position_steps = adapter->pan_position_steps;
    output->tilt_position_steps = adapter->tilt_position_steps;
    output->fault = adapter->fault;
}

static void e2025_aim_reset_motion(e2025_aim_adapter_t *adapter)
{
    adapter->pan_accumulator = 0;
    adapter->tilt_accumulator = 0;
    adapter->pan_last_direction = 0;
    adapter->tilt_last_direction = 0;
}

static int16_t e2025_aim_limit_command(int16_t command)
{
    if(command > 1000)
    {
        return 1000;
    }
    if(command < -1000)
    {
        return -1000;
    }
    return command;
}

static int8_t e2025_aim_axis_step(int16_t command,
                                  int32_t *accumulator,
                                  int8_t *last_direction,
                                  int32_t *position,
                                  uint8_t *limit_blocked)
{
    int8_t direction;
    int32_t steps;
    int32_t candidate;

    command = e2025_aim_limit_command(command);
    if(command == 0)
    {
        *accumulator = 0;
        *last_direction = 0;
        return 0;
    }

    direction = (command > 0) ? 1 : -1;
    if(*last_direction != 0 && *last_direction != direction)
    {
        /* Discard residual demand when reversing so the response is immediate. */
        *accumulator = 0;
    }
    *last_direction = direction;

    if((*position >= E2025_AIM_SOFT_LIMIT_STEPS && direction > 0) ||
       (*position <= -E2025_AIM_SOFT_LIMIT_STEPS && direction < 0))
    {
        *accumulator = 0;
        *limit_blocked = 1U;
        return 0;
    }

    *accumulator += command;
    steps = *accumulator / E2025_AIM_STEP_COMMAND_QUANTUM;
    if(steps > E2025_AIM_MAX_STEPS_PER_TICK)
    {
        steps = E2025_AIM_MAX_STEPS_PER_TICK;
    }
    else if(steps < -E2025_AIM_MAX_STEPS_PER_TICK)
    {
        steps = -E2025_AIM_MAX_STEPS_PER_TICK;
    }
    *accumulator -= steps * E2025_AIM_STEP_COMMAND_QUANTUM;

    candidate = *position + steps;
    if(candidate > E2025_AIM_SOFT_LIMIT_STEPS)
    {
        steps = E2025_AIM_SOFT_LIMIT_STEPS - *position;
        *accumulator = 0;
        *limit_blocked = 1U;
    }
    else if(candidate < -E2025_AIM_SOFT_LIMIT_STEPS)
    {
        steps = -E2025_AIM_SOFT_LIMIT_STEPS - *position;
        *accumulator = 0;
        *limit_blocked = 1U;
    }
    *position += steps;
    return (int8_t) steps;
}

void e2025_aim_adapter_init(e2025_aim_adapter_t *adapter)
{
    adapter->pan_accumulator = 0;
    adapter->tilt_accumulator = 0;
    adapter->pan_position_steps = 0;
    adapter->tilt_position_steps = 0;
    adapter->pan_last_direction = 0;
    adapter->tilt_last_direction = 0;
    adapter->vision_age_ticks = E2025_AIM_VISION_TIMEOUT_TICKS + 1U;
    adapter->was_enabled = 0U;
    adapter->fault = E2025_AIM_FAULT_NONE;
}

void e2025_aim_adapter_tick(e2025_aim_adapter_t *adapter,
                            uint8_t aim_enable,
                            uint8_t emergency_stop,
                            uint8_t vision_fresh,
                            uint8_t target_valid,
                            uint8_t spot_valid,
                            uint8_t laser_request,
                            int16_t pan_command,
                            int16_t tilt_command,
                            e2025_aim_output_t *output)
{
    e2025_aim_clear_output(adapter, output);
    if(aim_enable && emergency_stop && adapter->fault == E2025_AIM_FAULT_NONE)
    {
        adapter->fault = E2025_AIM_FAULT_ESTOP;
    }

    if(!aim_enable)
    {
        e2025_aim_reset_motion(adapter);
        adapter->vision_age_ticks = E2025_AIM_VISION_TIMEOUT_TICKS + 1U;
        adapter->was_enabled = 0U;
        output->fault = adapter->fault;
        return;
    }
    if(adapter->fault != E2025_AIM_FAULT_NONE)
    {
        output->fault = adapter->fault;
        return;
    }
    if(!adapter->was_enabled)
    {
        e2025_aim_reset_motion(adapter);
        adapter->vision_age_ticks = E2025_AIM_VISION_TIMEOUT_TICKS + 1U;
        adapter->was_enabled = 1U;
    }

    if(vision_fresh)
    {
        adapter->vision_age_ticks = 0U;
    }
    else if(adapter->vision_age_ticks < 0xFFU)
    {
        adapter->vision_age_ticks++;
    }

    output->vision_recent =
        (adapter->vision_age_ticks <= E2025_AIM_VISION_TIMEOUT_TICKS) ? 1U : 0U;
    if(!output->vision_recent || !target_valid || !spot_valid)
    {
        e2025_aim_reset_motion(adapter);
        return;
    }

    output->active = 1U;
    output->pan_steps = e2025_aim_axis_step(
        pan_command,
        &adapter->pan_accumulator,
        &adapter->pan_last_direction,
        &adapter->pan_position_steps,
        &output->pan_limit_blocked);
    output->tilt_steps = e2025_aim_axis_step(
        tilt_command,
        &adapter->tilt_accumulator,
        &adapter->tilt_last_direction,
        &adapter->tilt_position_steps,
        &output->tilt_limit_blocked);
    output->pan_position_steps = adapter->pan_position_steps;
    output->tilt_position_steps = adapter->tilt_position_steps;
    output->laser_allowed =
        (laser_request && !output->pan_limit_blocked &&
         !output->tilt_limit_blocked) ? 1U : 0U;
}
