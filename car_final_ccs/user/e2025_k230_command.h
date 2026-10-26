#ifndef E2025_K230_COMMAND_H_
#define E2025_K230_COMMAND_H_

#include "e2025_app.h"

typedef enum
{
    E2025_K230_MODE_STOP = 0,
    E2025_K230_MODE_CENTER,
    E2025_K230_MODE_CIRCLE
} e2025_k230_mode_t;

e2025_k230_mode_t e2025_k230_select_mode(const e2025_status_t *status);
const char *e2025_k230_command_text(e2025_k230_mode_t mode);

#endif
