#ifndef BALL_BALANCE_H
#define BALL_BALANCE_H

#include <stdint.h>

#define BALL_LEFT_PIXEL             37U
#define BALL_CENTER_PIXEL           351U
#define BALL_RIGHT_PIXEL            658U
#define BALL_HALF_LENGTH_MM         125

#define BALANCE_TARGET_LIMIT_STEPS  400

typedef struct
{
    int16_t x_mm;
    int16_t velocity_mm_s;
    int16_t previous_x_mm;
    uint32_t previous_sample_ms;
    uint32_t previous_frame_count;
    uint8_t have_measurement;
} ball_balance_t;

void ball_balance_init(ball_balance_t *controller);
int16_t ball_pixel_to_mm(uint16_t x_pixel);
void ball_balance_update_measurement(
    ball_balance_t *controller,
    uint16_t x_pixel,
    uint32_t frame_count,
    uint32_t now_ms);
int16_t ball_balance_target_steps(
    const ball_balance_t *controller,
    int16_t target_x_mm);

#endif
