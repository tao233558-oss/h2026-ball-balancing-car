#include "e2025_aim_executor.h"

static int8_t e2025_aim_step_direction(int16_t steps)
{
    if(steps > 0)
    {
        return 1;
    }
    if(steps < 0)
    {
        return -1;
    }
    return 0;
}

static void e2025_aim_axis_stop(e2025_aim_executor_axis_t *axis)
{
    axis->pending_steps = 0;
    axis->active_direction = 0;
    axis->phase_ticks = 0U;
    axis->step_active = 0U;
    axis->phase = E2025_AIM_AXIS_IDLE;
}

static void e2025_aim_axis_init(e2025_aim_executor_axis_t *axis)
{
    axis->emitted_position_steps = 0;
    e2025_aim_axis_stop(axis);
}

static uint8_t e2025_aim_axis_busy(const e2025_aim_executor_axis_t *axis)
{
    return (axis->phase != E2025_AIM_AXIS_IDLE ||
            axis->pending_steps != 0) ? 1U : 0U;
}

static void e2025_aim_axis_enqueue(e2025_aim_executor_axis_t *axis,
                                   int8_t requested_steps,
                                   uint8_t *queue_overflow,
                                   uint8_t *reversal_deferred)
{
    int8_t requested_direction;
    int8_t pending_direction;
    int16_t candidate;

    if(requested_steps == 0)
    {
        return;
    }

    requested_direction = e2025_aim_step_direction(requested_steps);
    pending_direction = e2025_aim_step_direction(axis->pending_steps);

    if(axis->phase != E2025_AIM_AXIS_IDLE &&
       axis->active_direction != requested_direction)
    {
        /* Never change DIR during DIR setup or an active STEP pulse. */
        *reversal_deferred = 1U;
        return;
    }

    if(axis->phase == E2025_AIM_AXIS_IDLE &&
       pending_direction != 0 && pending_direction != requested_direction)
    {
        /* Cancel stale, not-yet-started demand before accepting a reversal. */
        axis->pending_steps = 0;
    }

    candidate = axis->pending_steps + requested_steps;
    if(candidate > E2025_AIM_QUEUE_LIMIT)
    {
        candidate = E2025_AIM_QUEUE_LIMIT;
        *queue_overflow = 1U;
    }
    else if(candidate < -E2025_AIM_QUEUE_LIMIT)
    {
        candidate = -E2025_AIM_QUEUE_LIMIT;
        *queue_overflow = 1U;
    }
    axis->pending_steps = candidate;
}

static uint8_t e2025_aim_axis_limit_blocked(
    const e2025_aim_executor_axis_t *axis,
    uint8_t negative_limit,
    uint8_t positive_limit)
{
    int8_t direction = axis->active_direction;

    if(direction == 0)
    {
        direction = e2025_aim_step_direction(axis->pending_steps);
    }
    if((direction < 0 && negative_limit) ||
       (direction > 0 && positive_limit))
    {
        return 1U;
    }
    return 0U;
}

static void e2025_aim_axis_tick(e2025_aim_executor_axis_t *axis)
{
    switch(axis->phase)
    {
        case E2025_AIM_AXIS_IDLE:
            axis->step_active = 0U;
            if(axis->pending_steps != 0)
            {
                axis->active_direction =
                    e2025_aim_step_direction(axis->pending_steps);
                axis->phase = E2025_AIM_AXIS_DIR_SETUP;
                axis->phase_ticks = E2025_AIM_DIR_SETUP_TICKS;
            }
            break;

        case E2025_AIM_AXIS_DIR_SETUP:
            axis->step_active = 0U;
            if(axis->phase_ticks > 1U)
            {
                axis->phase_ticks--;
            }
            else
            {
                axis->phase = E2025_AIM_AXIS_STEP_HIGH;
                axis->phase_ticks = E2025_AIM_STEP_HIGH_TICKS;
                axis->step_active = 1U;
            }
            break;

        case E2025_AIM_AXIS_STEP_HIGH:
            if(axis->phase_ticks > 1U)
            {
                axis->phase_ticks--;
            }
            else
            {
                axis->phase = E2025_AIM_AXIS_STEP_LOW;
                axis->phase_ticks = E2025_AIM_STEP_LOW_TICKS;
                axis->step_active = 0U;
            }
            break;

        case E2025_AIM_AXIS_STEP_LOW:
            axis->step_active = 0U;
            if(axis->phase_ticks > 1U)
            {
                axis->phase_ticks--;
            }
            else
            {
                axis->pending_steps -= axis->active_direction;
                axis->emitted_position_steps += axis->active_direction;
                if(axis->pending_steps != 0)
                {
                    axis->phase = E2025_AIM_AXIS_STEP_HIGH;
                    axis->phase_ticks = E2025_AIM_STEP_HIGH_TICKS;
                    axis->step_active = 1U;
                }
                else
                {
                    axis->active_direction = 0;
                    axis->phase_ticks = 0U;
                    axis->phase = E2025_AIM_AXIS_IDLE;
                }
            }
            break;

        default:
            e2025_aim_axis_stop(axis);
            break;
    }
}

static void e2025_aim_fill_output(
    const e2025_aim_executor_t *executor,
    uint8_t pan_limit_blocked,
    uint8_t tilt_limit_blocked,
    uint8_t driver_awake_allowed,
    uint8_t laser_allowed,
    e2025_aim_executor_output_t *output)
{
    output->busy =
        (e2025_aim_axis_busy(&executor->pan) ||
         e2025_aim_axis_busy(&executor->tilt)) ? 1U : 0U;
    output->driver_awake_allowed = driver_awake_allowed;
    output->laser_allowed = laser_allowed;
    output->pan_step_active = executor->pan.step_active;
    output->tilt_step_active = executor->tilt.step_active;
    output->pan_limit_blocked = pan_limit_blocked;
    output->tilt_limit_blocked = tilt_limit_blocked;
    output->queue_overflow = executor->queue_overflow;
    output->reversal_deferred = executor->reversal_deferred;
    output->pan_direction = executor->pan.active_direction;
    output->tilt_direction = executor->tilt.active_direction;
    output->pan_pending_steps = executor->pan.pending_steps;
    output->tilt_pending_steps = executor->tilt.pending_steps;
    output->pan_emitted_position_steps =
        executor->pan.emitted_position_steps;
    output->tilt_emitted_position_steps =
        executor->tilt.emitted_position_steps;
    output->fault = executor->fault;
}

void e2025_aim_executor_init(e2025_aim_executor_t *executor)
{
    e2025_aim_axis_init(&executor->pan);
    e2025_aim_axis_init(&executor->tilt);
    executor->enabled = 0U;
    executor->laser_requested = 0U;
    executor->queue_overflow = 0U;
    executor->reversal_deferred = 0U;
    executor->fault = E2025_AIM_EXECUTOR_FAULT_NONE;
}

void e2025_aim_executor_abort(e2025_aim_executor_t *executor)
{
    e2025_aim_axis_stop(&executor->pan);
    e2025_aim_axis_stop(&executor->tilt);
    executor->enabled = 0U;
    executor->laser_requested = 0U;
}

void e2025_aim_executor_submit(
    e2025_aim_executor_t *executor,
    const e2025_aim_executor_command_t *command)
{
    executor->queue_overflow = 0U;
    executor->reversal_deferred = 0U;

    if(!command->enable ||
       executor->fault != E2025_AIM_EXECUTOR_FAULT_NONE)
    {
        e2025_aim_executor_abort(executor);
        return;
    }

    executor->enabled = 1U;
    executor->laser_requested = command->laser_request ? 1U : 0U;
    e2025_aim_axis_enqueue(&executor->pan,
                           command->pan_steps,
                           &executor->queue_overflow,
                           &executor->reversal_deferred);
    e2025_aim_axis_enqueue(&executor->tilt,
                           command->tilt_steps,
                           &executor->queue_overflow,
                           &executor->reversal_deferred);
}

void e2025_aim_executor_tick(
    e2025_aim_executor_t *executor,
    const e2025_aim_executor_safety_t *safety,
    e2025_aim_executor_output_t *output)
{
    uint8_t pan_limit_blocked = 0U;
    uint8_t tilt_limit_blocked = 0U;
    uint8_t any_limit_active;
    uint8_t driver_awake_allowed = 0U;
    uint8_t laser_allowed = 0U;

    if(safety->emergency_stop)
    {
        executor->fault = E2025_AIM_EXECUTOR_FAULT_ESTOP;
        e2025_aim_executor_abort(executor);
    }

    if(executor->enabled &&
       executor->fault == E2025_AIM_EXECUTOR_FAULT_NONE)
    {
        /* Maps to DCC-100v3 SLP/RST only in the future GPIO backend. */
        driver_awake_allowed = 1U;
        pan_limit_blocked = e2025_aim_axis_limit_blocked(
            &executor->pan,
            safety->pan_negative_limit,
            safety->pan_positive_limit);
        tilt_limit_blocked = e2025_aim_axis_limit_blocked(
            &executor->tilt,
            safety->tilt_negative_limit,
            safety->tilt_positive_limit);

        if(pan_limit_blocked)
        {
            e2025_aim_axis_stop(&executor->pan);
        }
        else
        {
            e2025_aim_axis_tick(&executor->pan);
        }
        if(tilt_limit_blocked)
        {
            e2025_aim_axis_stop(&executor->tilt);
        }
        else
        {
            e2025_aim_axis_tick(&executor->tilt);
        }

        any_limit_active =
            (safety->pan_negative_limit || safety->pan_positive_limit ||
             safety->tilt_negative_limit || safety->tilt_positive_limit) ?
                1U : 0U;
        if(executor->laser_requested && !any_limit_active &&
           !executor->queue_overflow && !executor->reversal_deferred)
        {
            laser_allowed = 1U;
        }
    }
    else
    {
        e2025_aim_axis_stop(&executor->pan);
        e2025_aim_axis_stop(&executor->tilt);
    }

    e2025_aim_fill_output(executor,
                          pan_limit_blocked,
                          tilt_limit_blocked,
                          driver_awake_allowed,
                          laser_allowed,
                          output);
}
