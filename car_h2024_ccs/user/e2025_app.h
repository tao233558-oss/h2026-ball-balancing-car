#ifndef __E2025_APP_H__
#define __E2025_APP_H__

#include <stdint.h>

#define E2025_TICK_MS             10U
#define E2025_CORNERS_PER_LAP      4U
#define E2025_AIM_TOLERANCE_PX     6
#define E2025_AIM_STABLE_FRAMES    3U

typedef enum
{
    E2025_TASK_NONE = 0,
    E2025_TASK_TRACK_N_LAPS = 1,
    E2025_TASK_AIM_2S = 2,
    E2025_TASK_AIM_SEARCH_4S = 3,
    E2025_TASK_MOVE_AIM_1_LAP = 4,
    E2025_TASK_MOVE_AIM_2_LAPS = 5,
    E2025_TASK_MOVE_DRAW_CIRCLE = 6,
} e2025_task_t;

typedef enum
{
    E2025_STATE_IDLE = 0,
    E2025_STATE_RUNNING,
    E2025_STATE_COMPLETE,
    E2025_STATE_TIMEOUT,
    E2025_STATE_ABORTED,
    E2025_STATE_FAULT,
} e2025_state_t;

typedef enum
{
    E2025_FAULT_NONE = 0,
    E2025_FAULT_USER_ESTOP,
    E2025_FAULT_DRIVE_NO_FEEDBACK,
    E2025_FAULT_DRIVE_LEFT_MISSING,
    E2025_FAULT_DRIVE_RIGHT_MISSING,
    E2025_FAULT_DRIVE_SPEED_MISMATCH,
    E2025_FAULT_DRIVE_LEFT_DIRECTION,
    E2025_FAULT_DRIVE_RIGHT_DIRECTION,
    E2025_FAULT_AIM_ESTOP,
    E2025_FAULT_K230_LINK_LOST,
    E2025_FAULT_INTERNAL
} e2025_fault_t;

typedef struct
{
    /* One-tick event generated when the car enters a validated corner. */
    uint8_t corner_event;
    uint8_t track_pattern;

    /* One-tick flag: a new, CRC-checked vision frame arrived. */
    uint8_t vision_fresh;
    uint8_t target_valid;
    uint8_t spot_valid;
    uint8_t circle_complete;
    uint8_t emergency_stop;

    int16_t target_x;
    int16_t target_y;
    int16_t spot_x;
    int16_t spot_y;

    /* Read-only TOF telemetry. It does not change task behavior yet. */
    uint8_t tof_present;
    uint8_t distance_fresh;
    uint8_t distance_valid;
    uint8_t tof_range_status;
    uint16_t distance_mm;
    uint16_t distance_age_ticks;
} e2025_input_t;

typedef struct
{
    uint8_t drive_enable;
    uint8_t track_enable;
    uint8_t aim_enable;
    uint8_t laser_enable;
    int16_t pan_command;
    int16_t tilt_command;
} e2025_output_t;

typedef struct
{
    e2025_task_t task;
    e2025_state_t state;
    e2025_fault_t fault;
    uint8_t requested_laps;
    uint8_t completed_laps;
    uint8_t completed_corners;
    uint8_t aim_locked;
    uint16_t elapsed_ticks;
    uint16_t limit_ticks;
    uint16_t vision_age_ticks;
    int16_t aim_error_x;
    int16_t aim_error_y;
    uint8_t tof_present;
    uint8_t distance_valid;
    uint8_t tof_range_status;
    uint16_t distance_mm;
    uint16_t distance_age_ticks;
} e2025_status_t;

/* Exposed as int so the existing OLED parameter menu can edit it directly. */
extern int g_e2025_laps_setting;

void e2025_app_init(void);
void e2025_request_start(e2025_task_t task, uint8_t requested_laps);
void e2025_request_abort(void);
void e2025_request_fault(e2025_fault_t fault);
void e2025_tick_10ms(const e2025_input_t *input);
void e2025_get_status(e2025_status_t *status);
void e2025_get_output(e2025_output_t *output);
uint8_t e2025_state_is_terminal(e2025_state_t state);
const char *e2025_task_name(e2025_task_t task);
const char *e2025_state_name(e2025_state_t state);
const char *e2025_fault_name(e2025_fault_t fault);

#endif
