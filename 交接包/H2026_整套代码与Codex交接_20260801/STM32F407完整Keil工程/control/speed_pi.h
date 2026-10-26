#ifndef __SPEED_PI_H__
#define __SPEED_PI_H__

#include "stm32f4xx.h"

typedef struct
{
    int16_t target_counts;
    int16_t integral;
    int16_t output_permille;
} speed_pi_t;

void speed_pi_reset(speed_pi_t *controller, int16_t target_counts);
int16_t speed_pi_step(speed_pi_t *controller,
                      int16_t measured_counts,
                      int16_t feedforward_permille);

#endif
