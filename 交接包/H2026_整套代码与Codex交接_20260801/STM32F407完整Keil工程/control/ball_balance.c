#include "ball_balance.h"

#define BALANCE_POSITION_KP_NUM     2
#define BALANCE_POSITION_KP_DEN     1
#define BALANCE_VELOCITY_KD_NUM     2
#define BALANCE_VELOCITY_KD_DEN     1
#define BALL_VELOCITY_LIMIT_MM_S    500

/* Measured on the assembled rack/pipe mechanism on 2026-08-01. */
#define POSITIVE_BREAKAWAY_STEPS    288
#define NEGATIVE_BREAKAWAY_STEPS    253
#define BALANCE_EFFORT_DEADBAND      18

static int16_t clamp_i16(int32_t value,
                         int16_t minimum,
                         int16_t maximum)
{
    if(value < minimum)
    {
        return minimum;
    }
    if(value > maximum)
    {
        return maximum;
    }
    return (int16_t)value;
}

void ball_balance_init(ball_balance_t *controller)
{
    controller->x_mm = 0;
    controller->velocity_mm_s = 0;
    controller->previous_x_mm = 0;
    controller->previous_sample_ms = 0U;
    controller->previous_frame_count = 0U;
    controller->have_measurement = 0U;
}

int16_t ball_pixel_to_mm(uint16_t x_pixel)
{
    int32_t result;

    if(x_pixel <= BALL_CENTER_PIXEL)
    {
        result =
            -BALL_HALF_LENGTH_MM +
            ((int32_t)((int32_t)x_pixel -
                       (int32_t)BALL_LEFT_PIXEL) *
             BALL_HALF_LENGTH_MM) /
            (int32_t)(BALL_CENTER_PIXEL - BALL_LEFT_PIXEL);
    }
    else
    {
        result =
            ((int32_t)(x_pixel - BALL_CENTER_PIXEL) *
             BALL_HALF_LENGTH_MM) /
            (int32_t)(BALL_RIGHT_PIXEL - BALL_CENTER_PIXEL);
    }

    return clamp_i16(result,
                     -BALL_HALF_LENGTH_MM,
                     BALL_HALF_LENGTH_MM);
}

void ball_balance_update_measurement(
    ball_balance_t *controller,
    uint16_t x_pixel,
    uint32_t frame_count,
    uint32_t now_ms)
{
    int16_t x_mm;

    if((controller->have_measurement != 0U) &&
       (frame_count == controller->previous_frame_count))
    {
        return;
    }

    x_mm = ball_pixel_to_mm(x_pixel);

    if(controller->have_measurement != 0U)
    {
        uint32_t dt_ms =
            now_ms - controller->previous_sample_ms;
        if((dt_ms >= 20U) && (dt_ms <= 500U))
        {
            int32_t raw_velocity =
                ((int32_t)(x_mm - controller->previous_x_mm) *
                 1000) /
                (int32_t)dt_ms;

            raw_velocity = clamp_i16(
                raw_velocity,
                -BALL_VELOCITY_LIMIT_MM_S,
                BALL_VELOCITY_LIMIT_MM_S);

            /*
             * K230 already compensates inference delay.  This filter only
             * suppresses one-frame coordinate jitter before differentiation.
             */
            controller->velocity_mm_s =
                (int16_t)(((int32_t)controller->velocity_mm_s +
                           raw_velocity) /
                          2);
        }
        else
        {
            controller->velocity_mm_s = 0;
        }
    }
    else
    {
        controller->velocity_mm_s = 0;
        controller->have_measurement = 1U;
    }

    controller->x_mm = x_mm;
    controller->previous_x_mm = x_mm;
    controller->previous_sample_ms = now_ms;
    controller->previous_frame_count = frame_count;
}

int16_t ball_balance_target_steps(
    const ball_balance_t *controller,
    int16_t target_x_mm)
{
    int32_t position_error =
        (int32_t)controller->x_mm - target_x_mm;
    int32_t output =
        (position_error * BALANCE_POSITION_KP_NUM) /
        BALANCE_POSITION_KP_DEN;

    output +=
        ((int32_t)controller->velocity_mm_s *
         BALANCE_VELOCITY_KD_NUM) /
        BALANCE_VELOCITY_KD_DEN;

    /*
     * The ball does not start rolling until the rack has moved roughly
     * +288 steps upward or -253 steps downward from level.  Map a real
     * control request across that dead mechanical zone, while keeping a
     * small quiet region around the requested ball position.
     */
    if(output > BALANCE_EFFORT_DEADBAND)
    {
        output += POSITIVE_BREAKAWAY_STEPS;
    }
    else if(output < -BALANCE_EFFORT_DEADBAND)
    {
        output -= NEGATIVE_BREAKAWAY_STEPS;
    }
    else
    {
        output = 0;
    }

    return clamp_i16(output,
                     -BALANCE_TARGET_LIMIT_STEPS,
                     BALANCE_TARGET_LIMIT_STEPS);
}
