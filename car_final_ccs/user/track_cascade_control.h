#ifndef __TRACK_CASCADE_CONTROL_H__
#define __TRACK_CASCADE_CONTROL_H__

#include <stdint.h>

typedef enum
{
    TRACK_CASCADE_FOLLOW = 0,
    TRACK_CASCADE_SEARCH_LEFT,
    TRACK_CASCADE_SEARCH_RIGHT,
    TRACK_CASCADE_CORNER_LEFT,
    TRACK_CASCADE_CORNER_RIGHT
} track_cascade_mode_t;

typedef struct
{
    int32_t base_target_counts;
    int32_t minimum_target_counts;
    int32_t maximum_target_counts;
    int32_t search_target_counts;
    int32_t corner_target_counts;
    int32_t correction_divisor;
    int32_t correction_limit;
} track_cascade_config_t;

typedef struct
{
    track_cascade_config_t config;
    int32_t filtered_error;
    int8_t last_turn_direction;
} track_cascade_controller_t;

typedef struct
{
    uint8_t pattern;
    int32_t raw_error;
    int32_t filtered_error;
    int32_t left_target_counts;
    int32_t right_target_counts;
    track_cascade_mode_t mode;
} track_cascade_output_t;

extern const track_cascade_config_t g_track_cascade_default_config;

void track_cascade_init(track_cascade_controller_t *controller,
                        const track_cascade_config_t *config);
void track_cascade_step(track_cascade_controller_t *controller,
                        uint8_t pattern,
                        int32_t weighted_error,
                        track_cascade_output_t *output);
const char *track_cascade_mode_name(track_cascade_mode_t mode);

#endif
