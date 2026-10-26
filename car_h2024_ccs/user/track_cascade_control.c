#include "track_cascade_control.h"

/* Counts are per 100 ms and remain placeholders until open-loop calibration. */
const track_cascade_config_t g_track_cascade_default_config =
{
    10, /* base target */
     7, /* minimum target while following */
    13, /* maximum target while following */
     7, /* outer wheel target while searching */
    10, /* outer wheel target at a detected corner */
     4, /* filtered error to target-count correction divisor */
     3  /* maximum target-count correction */
};

static int32_t track_cascade_clamp(int32_t value,
                                   int32_t minimum,
                                   int32_t maximum)
{
    if(value < minimum)
    {
        return minimum;
    }
    if(value > maximum)
    {
        return maximum;
    }
    return value;
}

void track_cascade_init(track_cascade_controller_t *controller,
                        const track_cascade_config_t *config)
{
    controller->config = *config;
    controller->filtered_error = 0;
    controller->last_turn_direction = 1;
}

void track_cascade_step(track_cascade_controller_t *controller,
                        uint8_t pattern,
                        int32_t weighted_error,
                        track_cascade_output_t *output)
{
    int32_t correction;

    output->pattern = pattern;
    output->raw_error = weighted_error;
    output->filtered_error = controller->filtered_error;

    if(pattern == 0U)
    {
        if(controller->last_turn_direction < 0)
        {
            output->left_target_counts = 0;
            output->right_target_counts =
                controller->config.search_target_counts;
            output->mode = TRACK_CASCADE_SEARCH_LEFT;
        }
        else
        {
            output->left_target_counts =
                controller->config.search_target_counts;
            output->right_target_counts = 0;
            output->mode = TRACK_CASCADE_SEARCH_RIGHT;
        }
        return;
    }

    if(weighted_error < -8)
    {
        controller->last_turn_direction = -1;
    }
    else if(weighted_error > 8)
    {
        controller->last_turn_direction = 1;
    }

    if((pattern & 0x03U) == 0x03U && (pattern & 0x3CU) == 0U)
    {
        controller->filtered_error = weighted_error;
        output->filtered_error = weighted_error;
        output->left_target_counts = 0;
        output->right_target_counts =
            controller->config.corner_target_counts;
        output->mode = TRACK_CASCADE_CORNER_LEFT;
        return;
    }
    if((pattern & 0xC0U) == 0xC0U && (pattern & 0x3CU) == 0U)
    {
        controller->filtered_error = weighted_error;
        output->filtered_error = weighted_error;
        output->left_target_counts =
            controller->config.corner_target_counts;
        output->right_target_counts = 0;
        output->mode = TRACK_CASCADE_CORNER_RIGHT;
        return;
    }

    /* One-pole integer filter limits the effect of a one-tick sensor spike. */
    controller->filtered_error =
        (controller->filtered_error + weighted_error) / 2;
    correction = controller->filtered_error /
                 controller->config.correction_divisor;
    correction = track_cascade_clamp(
        correction,
        -controller->config.correction_limit,
        controller->config.correction_limit);

    output->filtered_error = controller->filtered_error;
    output->left_target_counts = track_cascade_clamp(
        controller->config.base_target_counts + correction,
        controller->config.minimum_target_counts,
        controller->config.maximum_target_counts);
    output->right_target_counts = track_cascade_clamp(
        controller->config.base_target_counts - correction,
        controller->config.minimum_target_counts,
        controller->config.maximum_target_counts);
    output->mode = TRACK_CASCADE_FOLLOW;
}

const char *track_cascade_mode_name(track_cascade_mode_t mode)
{
    switch(mode)
    {
        case TRACK_CASCADE_SEARCH_LEFT:  return "SEARCH LEFT";
        case TRACK_CASCADE_SEARCH_RIGHT: return "SEARCH RIGHT";
        case TRACK_CASCADE_CORNER_LEFT:  return "CORNER LEFT";
        case TRACK_CASCADE_CORNER_RIGHT: return "CORNER RIGHT";
        default:                         return "FOLLOW";
    }
}
