#include "e2025_app.h"

#include <stddef.h>

/* Official time limits expressed in 10 ms ticks. */
#define E2025_LIMIT_TRACK_TICKS       2000U
#define E2025_LIMIT_AIM_2S_TICKS       200U
#define E2025_LIMIT_AIM_4S_TICKS       400U
#define E2025_LIMIT_MOVE_1_TICKS      2000U
#define E2025_LIMIT_MOVE_2_TICKS      4000U
#define E2025_LIMIT_CIRCLE_TICKS      2000U
#define E2025_COMMAND_LIMIT            1000
#define E2025_COMMAND_GAIN                4

typedef struct
{
    volatile e2025_task_t task;
    volatile e2025_state_t state;
    volatile e2025_fault_t fault;
    volatile uint8_t requested_laps;
    volatile uint8_t completed_laps;
    volatile uint8_t completed_corners;
    volatile uint8_t aim_stable_frames;
    volatile uint8_t aim_locked;
    volatile uint16_t elapsed_ticks;
    volatile uint16_t limit_ticks;
    volatile uint16_t vision_age_ticks;
    volatile int16_t aim_error_x;
    volatile int16_t aim_error_y;
    volatile uint8_t tof_present;
    volatile uint8_t distance_valid;
    volatile uint8_t tof_range_status;
    volatile uint16_t distance_mm;
    volatile uint16_t distance_age_ticks;
    volatile e2025_output_t output;
} e2025_context_t;

static e2025_context_t g_e2025;

/* Requests are written by the menu and consumed by the 10 ms control tick. */
static volatile uint8_t g_start_pending;
static volatile uint8_t g_abort_pending;
static volatile uint8_t g_fault_pending;
static volatile e2025_task_t g_pending_task;
static volatile uint8_t g_pending_laps;
static volatile e2025_fault_t g_pending_fault;

int g_e2025_laps_setting = 1;

static int16_t e2025_limit_command(int32_t value)
{
    if(value > E2025_COMMAND_LIMIT)
    {
        return E2025_COMMAND_LIMIT;
    }
    if(value < -E2025_COMMAND_LIMIT)
    {
        return -E2025_COMMAND_LIMIT;
    }
    return (int16_t) value;
}

static int16_t e2025_abs_i16(int16_t value)
{
    return (value < 0) ? (int16_t) -value : value;
}

static uint16_t e2025_task_limit(e2025_task_t task)
{
    switch(task)
    {
        case E2025_TASK_TRACK_N_LAPS:
            return E2025_LIMIT_TRACK_TICKS;
        case E2025_TASK_AIM_2S:
            return E2025_LIMIT_AIM_2S_TICKS;
        case E2025_TASK_AIM_SEARCH_4S:
            return E2025_LIMIT_AIM_4S_TICKS;
        case E2025_TASK_MOVE_AIM_1_LAP:
            return E2025_LIMIT_MOVE_1_TICKS;
        case E2025_TASK_MOVE_AIM_2_LAPS:
            return E2025_LIMIT_MOVE_2_TICKS;
        case E2025_TASK_MOVE_DRAW_CIRCLE:
            return E2025_LIMIT_CIRCLE_TICKS;
        default:
            return 0;
    }
}

static uint8_t e2025_task_uses_drive(e2025_task_t task)
{
    return (task == E2025_TASK_TRACK_N_LAPS ||
            task == E2025_TASK_MOVE_AIM_1_LAP ||
            task == E2025_TASK_MOVE_AIM_2_LAPS ||
            task == E2025_TASK_MOVE_DRAW_CIRCLE) ? 1U : 0U;
}

static uint8_t e2025_task_uses_aim(e2025_task_t task)
{
    return (task == E2025_TASK_AIM_2S ||
            task == E2025_TASK_AIM_SEARCH_4S ||
            task == E2025_TASK_MOVE_AIM_1_LAP ||
            task == E2025_TASK_MOVE_AIM_2_LAPS ||
            task == E2025_TASK_MOVE_DRAW_CIRCLE) ? 1U : 0U;
}

static void e2025_outputs_off(void)
{
    g_e2025.output.drive_enable = 0;
    g_e2025.output.track_enable = 0;
    g_e2025.output.aim_enable = 0;
    g_e2025.output.laser_enable = 0;
    g_e2025.output.pan_command = 0;
    g_e2025.output.tilt_command = 0;
}

static void e2025_finish(e2025_state_t terminal_state)
{
    g_e2025.state = terminal_state;
    e2025_outputs_off();
}

static void e2025_start_now(e2025_task_t task, uint8_t requested_laps)
{
    uint16_t limit = e2025_task_limit(task);

    if(limit == 0U)
    {
        g_e2025.fault = E2025_FAULT_INTERNAL;
        e2025_finish(E2025_STATE_FAULT);
        return;
    }

    if(requested_laps < 1U)
    {
        requested_laps = 1U;
    }
    if(requested_laps > 5U)
    {
        requested_laps = 5U;
    }

    if(task == E2025_TASK_MOVE_AIM_1_LAP ||
       task == E2025_TASK_MOVE_DRAW_CIRCLE)
    {
        requested_laps = 1U;
    }
    else if(task == E2025_TASK_MOVE_AIM_2_LAPS)
    {
        requested_laps = 2U;
    }

    g_e2025.task = task;
    g_e2025.state = E2025_STATE_RUNNING;
    g_e2025.fault = E2025_FAULT_NONE;
    g_e2025.requested_laps = requested_laps;
    g_e2025.completed_laps = 0;
    g_e2025.completed_corners = 0;
    g_e2025.aim_stable_frames = 0;
    g_e2025.aim_locked = 0;
    g_e2025.elapsed_ticks = 0;
    g_e2025.limit_ticks = limit;
    g_e2025.vision_age_ticks = 0xFFFFU;
    g_e2025.aim_error_x = 0;
    g_e2025.aim_error_y = 0;
    e2025_outputs_off();
}

static void e2025_update_laps(const e2025_input_t *input)
{
    if(!e2025_task_uses_drive(g_e2025.task) || !input->corner_event)
    {
        return;
    }

    g_e2025.completed_corners++;
    if(g_e2025.completed_corners >= E2025_CORNERS_PER_LAP)
    {
        g_e2025.completed_corners = 0;
        if(g_e2025.completed_laps < 255U)
        {
            g_e2025.completed_laps++;
        }
    }
}

static void e2025_update_aim(const e2025_input_t *input)
{
    int16_t error_x;
    int16_t error_y;

    if(!e2025_task_uses_aim(g_e2025.task))
    {
        g_e2025.aim_locked = 0;
        return;
    }

    g_e2025.output.aim_enable = 1;
    g_e2025.output.laser_enable = input->target_valid ? 1U : 0U;

    if(input->vision_fresh)
    {
        g_e2025.vision_age_ticks = 0;
    }
    else if(g_e2025.vision_age_ticks < 0xFFFFU)
    {
        g_e2025.vision_age_ticks++;
    }

    if(!input->target_valid || !input->spot_valid)
    {
        g_e2025.aim_stable_frames = 0;
        g_e2025.aim_locked = 0;
        g_e2025.output.pan_command = 0;
        g_e2025.output.tilt_command = 0;
        return;
    }

    error_x = (int16_t) (input->target_x - input->spot_x);
    error_y = (int16_t) (input->target_y - input->spot_y);
    g_e2025.aim_error_x = error_x;
    g_e2025.aim_error_y = error_y;
    g_e2025.output.pan_command = e2025_limit_command((int32_t) error_x * E2025_COMMAND_GAIN);
    g_e2025.output.tilt_command = e2025_limit_command((int32_t) error_y * E2025_COMMAND_GAIN);

    /* Consecutive means consecutive camera frames, not consecutive 10 ms
     * control ticks. Between valid frames, retain the current stable count. */
    if(input->vision_fresh)
    {
        if(e2025_abs_i16(error_x) <= E2025_AIM_TOLERANCE_PX &&
           e2025_abs_i16(error_y) <= E2025_AIM_TOLERANCE_PX)
        {
            if(g_e2025.aim_stable_frames < E2025_AIM_STABLE_FRAMES)
            {
                g_e2025.aim_stable_frames++;
            }
        }
        else
        {
            g_e2025.aim_stable_frames = 0;
        }
    }

    g_e2025.aim_locked =
        (g_e2025.aim_stable_frames >= E2025_AIM_STABLE_FRAMES) ? 1U : 0U;
}

static void e2025_update_tof(const e2025_input_t *input)
{
    g_e2025.tof_present = input->tof_present;
    g_e2025.distance_valid = input->distance_valid;
    g_e2025.tof_range_status = input->tof_range_status;
    g_e2025.distance_mm = input->distance_mm;
    g_e2025.distance_age_ticks = input->distance_age_ticks;
}

void e2025_app_init(void)
{
    g_start_pending = 0;
    g_abort_pending = 0;
    g_fault_pending = 0;
    g_pending_task = E2025_TASK_NONE;
    g_pending_laps = 1;
    g_pending_fault = E2025_FAULT_NONE;
    g_e2025.task = E2025_TASK_NONE;
    g_e2025.state = E2025_STATE_IDLE;
    g_e2025.fault = E2025_FAULT_NONE;
    g_e2025.requested_laps = 1;
    g_e2025.completed_laps = 0;
    g_e2025.completed_corners = 0;
    g_e2025.aim_stable_frames = 0;
    g_e2025.aim_locked = 0;
    g_e2025.elapsed_ticks = 0;
    g_e2025.limit_ticks = 0;
    g_e2025.vision_age_ticks = 0xFFFFU;
    g_e2025.aim_error_x = 0;
    g_e2025.aim_error_y = 0;
    g_e2025.tof_present = 0U;
    g_e2025.distance_valid = 0U;
    g_e2025.tof_range_status = 0xFFU;
    g_e2025.distance_mm = 0U;
    g_e2025.distance_age_ticks = 0xFFFFU;
    e2025_outputs_off();
}

void e2025_request_start(e2025_task_t task, uint8_t requested_laps)
{
    g_pending_task = task;
    g_pending_laps = requested_laps;
    g_start_pending = 1;
}

void e2025_request_abort(void)
{
    g_abort_pending = 1;
}

void e2025_request_fault(e2025_fault_t fault)
{
    if(fault != E2025_FAULT_NONE)
    {
        g_pending_fault = fault;
        g_fault_pending = 1U;
    }
}

void e2025_tick_10ms(const e2025_input_t *input)
{
    if(input == NULL)
    {
        g_e2025.fault = E2025_FAULT_INTERNAL;
        e2025_finish(E2025_STATE_FAULT);
        return;
    }

    e2025_update_tof(input);

    if(g_fault_pending)
    {
        e2025_fault_t fault = g_pending_fault;
        g_fault_pending = 0U;
        g_start_pending = 0U;
        g_abort_pending = 0U;
        g_e2025.fault = fault;
        e2025_finish(E2025_STATE_FAULT);
        return;
    }

    if(g_abort_pending)
    {
        g_abort_pending = 0;
        if(g_e2025.state == E2025_STATE_RUNNING)
        {
            e2025_finish(E2025_STATE_ABORTED);
        }
    }

    if(g_start_pending)
    {
        e2025_task_t task = g_pending_task;
        uint8_t laps = g_pending_laps;
        g_start_pending = 0;
        e2025_start_now(task, laps);
    }

    if(g_e2025.state != E2025_STATE_RUNNING)
    {
        e2025_outputs_off();
        return;
    }

    if(input->emergency_stop)
    {
        g_e2025.fault = E2025_FAULT_USER_ESTOP;
        e2025_finish(E2025_STATE_ABORTED);
        return;
    }

    if(g_e2025.elapsed_ticks >= g_e2025.limit_ticks)
    {
        e2025_finish(E2025_STATE_TIMEOUT);
        return;
    }
    g_e2025.elapsed_ticks++;

    g_e2025.output.drive_enable = e2025_task_uses_drive(g_e2025.task);
    g_e2025.output.track_enable = g_e2025.output.drive_enable;
    g_e2025.output.aim_enable = 0;
    g_e2025.output.laser_enable = 0;
    g_e2025.output.pan_command = 0;
    g_e2025.output.tilt_command = 0;

    e2025_update_laps(input);
    e2025_update_aim(input);

    switch(g_e2025.task)
    {
        case E2025_TASK_TRACK_N_LAPS:
            if(g_e2025.completed_laps >= g_e2025.requested_laps)
            {
                e2025_finish(E2025_STATE_COMPLETE);
            }
            break;

        case E2025_TASK_AIM_2S:
        case E2025_TASK_AIM_SEARCH_4S:
            if(g_e2025.aim_locked)
            {
                e2025_finish(E2025_STATE_COMPLETE);
            }
            break;

        case E2025_TASK_MOVE_AIM_1_LAP:
        case E2025_TASK_MOVE_AIM_2_LAPS:
            if(g_e2025.completed_laps >= g_e2025.requested_laps)
            {
                e2025_finish(E2025_STATE_COMPLETE);
            }
            break;

        case E2025_TASK_MOVE_DRAW_CIRCLE:
            if(g_e2025.completed_laps >= 1U && input->circle_complete)
            {
                e2025_finish(E2025_STATE_COMPLETE);
            }
            break;

        default:
            g_e2025.fault = E2025_FAULT_INTERNAL;
            e2025_finish(E2025_STATE_FAULT);
            break;
    }
}

void e2025_get_status(e2025_status_t *status)
{
    if(status == NULL)
    {
        return;
    }

    status->task = g_e2025.task;
    status->state = g_e2025.state;
    status->fault = g_e2025.fault;
    status->requested_laps = g_e2025.requested_laps;
    status->completed_laps = g_e2025.completed_laps;
    status->completed_corners = g_e2025.completed_corners;
    status->aim_locked = g_e2025.aim_locked;
    status->elapsed_ticks = g_e2025.elapsed_ticks;
    status->limit_ticks = g_e2025.limit_ticks;
    status->vision_age_ticks = g_e2025.vision_age_ticks;
    status->aim_error_x = g_e2025.aim_error_x;
    status->aim_error_y = g_e2025.aim_error_y;
    status->tof_present = g_e2025.tof_present;
    status->distance_valid = g_e2025.distance_valid;
    status->tof_range_status = g_e2025.tof_range_status;
    status->distance_mm = g_e2025.distance_mm;
    status->distance_age_ticks = g_e2025.distance_age_ticks;
}

void e2025_get_output(e2025_output_t *output)
{
    if(output == NULL)
    {
        return;
    }

    output->drive_enable = g_e2025.output.drive_enable;
    output->track_enable = g_e2025.output.track_enable;
    output->aim_enable = g_e2025.output.aim_enable;
    output->laser_enable = g_e2025.output.laser_enable;
    output->pan_command = g_e2025.output.pan_command;
    output->tilt_command = g_e2025.output.tilt_command;
}

uint8_t e2025_state_is_terminal(e2025_state_t state)
{
    return (state == E2025_STATE_COMPLETE ||
            state == E2025_STATE_TIMEOUT ||
            state == E2025_STATE_ABORTED ||
            state == E2025_STATE_FAULT) ? 1U : 0U;
}

const char *e2025_task_name(e2025_task_t task)
{
    switch(task)
    {
        case E2025_TASK_TRACK_N_LAPS:       return "E1 TRACK N";
        case E2025_TASK_AIM_2S:             return "E2 AIM 2S";
        case E2025_TASK_AIM_SEARCH_4S:      return "E3 AIM 4S";
        case E2025_TASK_MOVE_AIM_1_LAP:     return "E4 MOVE 1";
        case E2025_TASK_MOVE_AIM_2_LAPS:    return "E5 MOVE 2";
        case E2025_TASK_MOVE_DRAW_CIRCLE:   return "E6 CIRCLE";
        default:                            return "E2025 IDLE";
    }
}

const char *e2025_state_name(e2025_state_t state)
{
    switch(state)
    {
        case E2025_STATE_IDLE:       return "IDLE";
        case E2025_STATE_RUNNING:    return "RUN";
        case E2025_STATE_COMPLETE:   return "DONE";
        case E2025_STATE_TIMEOUT:    return "TIMEOUT";
        case E2025_STATE_ABORTED:    return "ABORT";
        case E2025_STATE_FAULT:      return "FAULT";
        default:                     return "UNKNOWN";
    }
}

const char *e2025_fault_name(e2025_fault_t fault)
{
    switch(fault)
    {
        case E2025_FAULT_NONE:                 return "NONE";
        case E2025_FAULT_USER_ESTOP:           return "E-STOP";
        case E2025_FAULT_DRIVE_NO_FEEDBACK:    return "NO FB";
        case E2025_FAULT_DRIVE_LEFT_MISSING:   return "LEFT FB";
        case E2025_FAULT_DRIVE_RIGHT_MISSING:  return "RIGHT FB";
        case E2025_FAULT_DRIVE_SPEED_MISMATCH: return "SPEED";
        case E2025_FAULT_DRIVE_LEFT_DIRECTION: return "LEFT DIR";
        case E2025_FAULT_DRIVE_RIGHT_DIRECTION:return "RIGHT DIR";
        case E2025_FAULT_AIM_ESTOP:            return "AIM STOP";
        case E2025_FAULT_K230_LINK_LOST:       return "K230 LOST";
        case E2025_FAULT_INTERNAL:             return "INTERNAL";
        default:                               return "UNKNOWN";
    }
}
