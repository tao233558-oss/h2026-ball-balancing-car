#ifndef BALL_BEAM_CONTROLLER_H
#define BALL_BEAM_CONTROLLER_H

#include <stdint.h>

/*
 * Pure integer outer-loop controller for the ball/beam mechanism.
 *
 * Sign convention used by this project:
 *   ball x > 0       : ball is on the positive/right side of the centre
 *   actuator > 0     : rack moves up and raises the tail of the pipe
 *   target_steps > 0 : command the actuator toward the positive direction
 *
 * All gains and limits are supplied at run time so the temporary JY61P
 * calibration can be removed from the competition build without changing
 * the controller itself.
 */
typedef struct
{
    int16_t kp_num;
    int16_t kp_den;
    int16_t kd_num;
    int16_t kd_den;
    int16_t position_deadband_mm;
    int16_t quiet_velocity_mm_s;
    int16_t assist_rate_steps_s;
    int16_t assist_decay_steps_s;
    int16_t assist_limit_steps;
    int16_t command_limit_steps;
    int16_t target_slew_steps_s;
} ball_beam_params_t;

typedef struct
{
    int32_t assist_q10;
    int32_t target_q10;
    int16_t last_error_mm;
    uint8_t initialized;
} ball_beam_controller_t;

void ball_beam_controller_init(ball_beam_controller_t *controller,
                               int16_t initial_target_steps);

/*
 * Update only when a new K230 coordinate arrives.  dt_ms is the time since
 * the previous valid coordinate, normally 20...200 ms.  The returned value
 * is an absolute actuator target relative to the level position captured by
 * the operator.
 */
int16_t ball_beam_controller_update(
    ball_beam_controller_t *controller,
    const ball_beam_params_t *params,
    int16_t ball_x_mm,
    int16_t ball_velocity_mm_s,
    int16_t setpoint_mm,
    uint16_t dt_ms);

/* Clear stored stiction assistance and command the calibrated level point. */
void ball_beam_controller_reset_level(
    ball_beam_controller_t *controller);

int16_t ball_beam_controller_get_assist_steps(
    const ball_beam_controller_t *controller);

#endif
