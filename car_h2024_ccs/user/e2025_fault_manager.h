#ifndef __E2025_FAULT_MANAGER_H__
#define __E2025_FAULT_MANAGER_H__

#include <stdint.h>

#include "dual_motor_guard.h"
#include "e2025_aim_adapter.h"
#include "e2025_app.h"

typedef struct
{
    uint8_t was_running;
    uint8_t k230_seen_in_session;
    e2025_task_t active_task;
} e2025_fault_manager_t;

void e2025_fault_manager_init(e2025_fault_manager_t *manager);
e2025_fault_t e2025_fault_manager_step(
    e2025_fault_manager_t *manager,
    const e2025_status_t *status,
    const e2025_input_t *input,
    dual_motor_diag_fault_t drive_fault,
    e2025_aim_fault_t aim_fault,
    uint8_t k230_connected);

#endif
