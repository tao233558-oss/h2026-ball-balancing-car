#ifndef __E2025_AIM_ADAPTER_H__
#define __E2025_AIM_ADAPTER_H__

#include <stdint.h>

#define E2025_AIM_VISION_TIMEOUT_TICKS 10U
#define E2025_AIM_STEP_COMMAND_QUANTUM 250
#define E2025_AIM_MAX_STEPS_PER_TICK 4
#define E2025_AIM_SOFT_LIMIT_STEPS 2000

typedef enum
{
    E2025_AIM_FAULT_NONE = 0,
    E2025_AIM_FAULT_ESTOP
} e2025_aim_fault_t;

typedef struct
{
    uint8_t active;
    uint8_t vision_recent;
    uint8_t laser_allowed;
    uint8_t pan_limit_blocked;
    uint8_t tilt_limit_blocked;
    int8_t pan_steps;
    int8_t tilt_steps;
    int32_t pan_position_steps;
    int32_t tilt_position_steps;
    e2025_aim_fault_t fault;
} e2025_aim_output_t;

typedef struct
{
    int32_t pan_accumulator;
    int32_t tilt_accumulator;
    int32_t pan_position_steps;
    int32_t tilt_position_steps;
    int8_t pan_last_direction;
    int8_t tilt_last_direction;
    uint8_t vision_age_ticks;
    uint8_t was_enabled;
    e2025_aim_fault_t fault;
} e2025_aim_adapter_t;

void e2025_aim_adapter_init(e2025_aim_adapter_t *adapter);
void e2025_aim_adapter_tick(e2025_aim_adapter_t *adapter,
                            uint8_t aim_enable,
                            uint8_t emergency_stop,
                            uint8_t vision_fresh,
                            uint8_t target_valid,
                            uint8_t spot_valid,
                            uint8_t laser_request,
                            int16_t pan_command,
                            int16_t tilt_command,
                            e2025_aim_output_t *output);

#endif
