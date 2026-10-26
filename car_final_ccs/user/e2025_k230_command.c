#include "e2025_k230_command.h"

#include <stddef.h>

e2025_k230_mode_t e2025_k230_select_mode(const e2025_status_t *status)
{
    if((status == NULL) || (status->state != E2025_STATE_RUNNING))
    {
        return E2025_K230_MODE_STOP;
    }

    switch(status->task)
    {
        case E2025_TASK_AIM_2S:
        case E2025_TASK_AIM_SEARCH_4S:
        case E2025_TASK_MOVE_AIM_1_LAP:
        case E2025_TASK_MOVE_AIM_2_LAPS:
            return E2025_K230_MODE_CENTER;

        case E2025_TASK_MOVE_DRAW_CIRCLE:
            return E2025_K230_MODE_CIRCLE;

        default:
            return E2025_K230_MODE_STOP;
    }
}

const char *e2025_k230_command_text(e2025_k230_mode_t mode)
{
    switch(mode)
    {
        case E2025_K230_MODE_CENTER:
            return "CENTER\n";
        case E2025_K230_MODE_CIRCLE:
            return "CIRCLE\n";
        default:
            return "STOP\n";
    }
}
