#ifndef __BSP_ENCODER_H__
#define __BSP_ENCODER_H__

#include "stm32f4xx.h"

/*
 * Quadrature encoder allocation:
 *   Left A  -> PB4 / TIM3_CH1
 *   Left B  -> PB5 / TIM3_CH2
 *   Right A -> PB6 / TIM4_CH1
 *   Right B -> PB7 / TIM4_CH2
 */

void encoder_init(void);
void encoder_reset(void);
int16_t encoder_left_count(void);
int16_t encoder_right_count(void);

#endif
