#ifndef __WHEEL_SPEED_PI_H__
#define __WHEEL_SPEED_PI_H__

#include <stdint.h>

typedef struct
{
    int32_t feedforward_pwm;
    int32_t minimum_pwm;
    int32_t maximum_pwm;
    int32_t kp;
    int32_t ki;
    int32_t integrator_limit;
} wheel_speed_pi_config_t;

typedef struct
{
    wheel_speed_pi_config_t config;
    int32_t integrator;
    int32_t output_pwm;
} wheel_speed_pi_t;

void wheel_speed_pi_init(wheel_speed_pi_t *controller,
                         const wheel_speed_pi_config_t *config);
void wheel_speed_pi_reset(wheel_speed_pi_t *controller);
int32_t wheel_speed_pi_step(wheel_speed_pi_t *controller,
                            int32_t target_counts,
                            int32_t measured_counts);

#endif
