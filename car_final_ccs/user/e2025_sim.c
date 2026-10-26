#include "e2025_sim.h"

#include <string.h>

#define SIM_CORNER_PERIOD_TICKS 75U
#define SIM_CIRCLE_CORNER_PERIOD_TICKS 300U

static int16_t sim_error_decay(uint16_t elapsed, uint16_t delay, int16_t start)
{
    int32_t error;

    if(elapsed <= delay)
    {
        return start;
    }

    error = (int32_t) start - (int32_t) (elapsed - delay) * 2;
    if(error < 0)
    {
        error = 0;
    }
    return (int16_t) error;
}

void e2025_sim_make_input(const e2025_status_t *status, e2025_input_t *input)
{
    uint16_t vision_delay = 0;
    uint16_t corner_period = SIM_CORNER_PERIOD_TICKS;
    int16_t error;

    memset(input, 0, sizeof(*input));
    if(status == 0)
    {
        return;
    }

    /* Deterministic telemetry for host-only replay. When the hardware TOF
     * feature is enabled, main.c replaces these fields with its ISR snapshot. */
    input->tof_present = 1U;
    input->distance_fresh = 1U;
    input->distance_valid = 1U;
    input->tof_range_status = 0U;
    input->distance_mm = 500U;
    input->distance_age_ticks = 0U;

    if(status->state != E2025_STATE_RUNNING)
    {
        return;
    }

    if(status->task == E2025_TASK_TRACK_N_LAPS ||
       status->task == E2025_TASK_MOVE_AIM_1_LAP ||
       status->task == E2025_TASK_MOVE_AIM_2_LAPS ||
       status->task == E2025_TASK_MOVE_DRAW_CIRCLE)
    {
        /* Both centre sensors see the line during nominal offline replay. */
        input->track_pattern = 0x18U;
        if(status->task == E2025_TASK_MOVE_DRAW_CIRCLE)
        {
            /* Four 3 s sides: one vehicle lap matches one 12 s light circle. */
            corner_period = SIM_CIRCLE_CORNER_PERIOD_TICKS;
        }
        if(status->elapsed_ticks > 0U &&
           (status->elapsed_ticks % corner_period) == 0U)
        {
            input->corner_event = 1;
        }
    }

    if(status->task == E2025_TASK_TRACK_N_LAPS)
    {
        return;
    }

    if(status->task == E2025_TASK_AIM_SEARCH_4S)
    {
        vision_delay = 100U; /* Simulate one second of target searching. */
    }

    if(status->elapsed_ticks < vision_delay)
    {
        return;
    }

    error = sim_error_decay(status->elapsed_ticks, vision_delay, 100);
    input->vision_fresh = 1;
    input->target_valid = 1;
    input->spot_valid = 1;
    input->target_x = 160;
    input->target_y = 120;
    input->spot_x = (int16_t) (160 - error);
    input->spot_y = (int16_t) (120 + (error / 2));

    if(status->task == E2025_TASK_MOVE_DRAW_CIRCLE &&
       status->elapsed_ticks >= 1200U)
    {
        input->circle_complete = 1;
    }
}
