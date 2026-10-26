#include "e2025_fault_manager.h"

static uint8_t e2025_fault_task_uses_aim(e2025_task_t task)
{
    return (task == E2025_TASK_AIM_2S ||
            task == E2025_TASK_AIM_SEARCH_4S ||
            task == E2025_TASK_MOVE_AIM_1_LAP ||
            task == E2025_TASK_MOVE_AIM_2_LAPS ||
            task == E2025_TASK_MOVE_DRAW_CIRCLE) ? 1U : 0U;
}

static e2025_fault_t e2025_fault_from_drive(
    dual_motor_diag_fault_t drive_fault)
{
    switch(drive_fault)
    {
        case DUAL_MOTOR_FAULT_NONE:
            return E2025_FAULT_NONE;
        case DUAL_MOTOR_FAULT_ESTOP:
            return E2025_FAULT_USER_ESTOP;
        case DUAL_MOTOR_FAULT_NO_FEEDBACK:
            return E2025_FAULT_DRIVE_NO_FEEDBACK;
        case DUAL_MOTOR_FAULT_LEFT_MISSING:
            return E2025_FAULT_DRIVE_LEFT_MISSING;
        case DUAL_MOTOR_FAULT_RIGHT_MISSING:
            return E2025_FAULT_DRIVE_RIGHT_MISSING;
        case DUAL_MOTOR_FAULT_SPEED_MISMATCH:
            return E2025_FAULT_DRIVE_SPEED_MISMATCH;
        case DUAL_MOTOR_FAULT_LEFT_DIRECTION:
            return E2025_FAULT_DRIVE_LEFT_DIRECTION;
        case DUAL_MOTOR_FAULT_RIGHT_DIRECTION:
            return E2025_FAULT_DRIVE_RIGHT_DIRECTION;
        default:
            return E2025_FAULT_INTERNAL;
    }
}

void e2025_fault_manager_init(e2025_fault_manager_t *manager)
{
    manager->was_running = 0U;
    manager->k230_seen_in_session = 0U;
    manager->active_task = E2025_TASK_NONE;
}

e2025_fault_t e2025_fault_manager_step(
    e2025_fault_manager_t *manager,
    const e2025_status_t *status,
    const e2025_input_t *input,
    dual_motor_diag_fault_t drive_fault,
    e2025_aim_fault_t aim_fault,
    uint8_t k230_connected)
{
    e2025_fault_t mapped_drive;

    if(status == 0 || input == 0)
    {
        return E2025_FAULT_INTERNAL;
    }
    if(status->state != E2025_STATE_RUNNING)
    {
        manager->was_running = 0U;
        manager->k230_seen_in_session = 0U;
        manager->active_task = status->task;
        return E2025_FAULT_NONE;
    }

    if(!manager->was_running || manager->active_task != status->task)
    {
        manager->k230_seen_in_session = 0U;
        manager->active_task = status->task;
    }
    manager->was_running = 1U;
    if(input->vision_fresh)
    {
        manager->k230_seen_in_session = 1U;
    }

    mapped_drive = e2025_fault_from_drive(drive_fault);
    if(mapped_drive != E2025_FAULT_NONE)
    {
        return mapped_drive;
    }
    if(aim_fault == E2025_AIM_FAULT_ESTOP)
    {
        return E2025_FAULT_AIM_ESTOP;
    }
    if(aim_fault != E2025_AIM_FAULT_NONE)
    {
        return E2025_FAULT_INTERNAL;
    }
    if(e2025_fault_task_uses_aim(status->task) &&
       manager->k230_seen_in_session && !k230_connected)
    {
        return E2025_FAULT_K230_LINK_LOST;
    }
    return E2025_FAULT_NONE;
}
