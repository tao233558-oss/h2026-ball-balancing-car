#ifndef __BSP_MOTOR_H__
#define __BSP_MOTOR_H__

#include "stm32f4xx.h"

/*
 * TB6612 signal allocation:
 *   PWMA  -> PE13 / TIM1_CH3
 *   AIN1  -> PD0
 *   AIN2  -> PD1
 *   PWMB  -> PE14 / TIM1_CH4
 *   BIN1  -> PD2
 *   BIN2  -> PD3
 *   STBY  -> PD4
 */

#define MOTOR_TEST_DUTY_PERMILLE  250U

void motor_init_safe(void);
void motor_all_stop(void);
void motor_set_both(int16_t left_duty_permille,
                    int16_t right_duty_permille);
void motor_left_run(int16_t duty_permille);
void motor_right_run(int16_t duty_permille);

#endif
