#ifndef BSP_STEPPER_H
#define BSP_STEPPER_H

#include "board.h"

/*
 * DCC-100 V3 channel 1 control:
 *   PA6 -> PWM/STEP
 *   PA7 -> DIR
 *   PA5 -> SLP
 *   PA4 -> RST
 * DCY is intentionally left unconnected.
 */

#define STEPPER_SPEED_LIMIT_STEPS_S  1200

void stepper_init_safe(void);
void stepper_enable(uint8_t enable);
uint8_t stepper_is_enabled(void);
void stepper_set_speed(int16_t speed_steps_s);
int16_t stepper_get_speed(void);
int32_t stepper_get_position(void);
void stepper_set_position(int32_t position_steps);
void stepper_emergency_stop(void);

void TIM6_DAC_IRQHandler(void);

#endif
