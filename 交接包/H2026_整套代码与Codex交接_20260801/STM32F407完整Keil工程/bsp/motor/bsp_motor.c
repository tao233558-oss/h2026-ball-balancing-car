#include "bsp_motor.h"

#define MOTOR_PWM_PERIOD       999U

#define MOTOR_AIN1_PIN         GPIO_Pin_0
#define MOTOR_AIN2_PIN         GPIO_Pin_1
#define MOTOR_BIN1_PIN         GPIO_Pin_2
#define MOTOR_BIN2_PIN         GPIO_Pin_3
#define MOTOR_STBY_PIN         GPIO_Pin_4

static uint16_t motor_duty_to_compare(int16_t duty_permille)
{
    uint16_t magnitude;

    if(duty_permille < 0)
    {
        magnitude = (uint16_t)(-duty_permille);
    }
    else
    {
        magnitude = (uint16_t)duty_permille;
    }

    if(magnitude > 1000U)
    {
        magnitude = 1000U;
    }

    return (uint16_t)(((uint32_t)magnitude * (MOTOR_PWM_PERIOD + 1U)) /
                      1000U);
}

static void motor_standby(uint8_t enable)
{
    if(enable != 0U)
    {
        GPIO_SetBits(GPIOD, MOTOR_STBY_PIN);
    }
    else
    {
        GPIO_ResetBits(GPIOD, MOTOR_STBY_PIN);
    }
}

void motor_all_stop(void)
{
    TIM_SetCompare3(TIM1, 0U);
    TIM_SetCompare4(TIM1, 0U);

    GPIO_ResetBits(GPIOD, MOTOR_AIN1_PIN | MOTOR_AIN2_PIN |
                          MOTOR_BIN1_PIN | MOTOR_BIN2_PIN);
    motor_standby(0U);
}

void motor_set_both(int16_t left_duty_permille,
                    int16_t right_duty_permille)
{
    /*
     * Remove PWM before changing either H-bridge direction.  This also makes
     * a direction reversal pass through a deterministic zero-output state.
     */
    TIM_SetCompare3(TIM1, 0U);
    TIM_SetCompare4(TIM1, 0U);

    if(left_duty_permille > 0)
    {
        GPIO_SetBits(GPIOD, MOTOR_AIN1_PIN);
        GPIO_ResetBits(GPIOD, MOTOR_AIN2_PIN);
    }
    else if(left_duty_permille < 0)
    {
        GPIO_ResetBits(GPIOD, MOTOR_AIN1_PIN);
        GPIO_SetBits(GPIOD, MOTOR_AIN2_PIN);
    }
    else
    {
        GPIO_ResetBits(GPIOD, MOTOR_AIN1_PIN | MOTOR_AIN2_PIN);
    }

    if(right_duty_permille > 0)
    {
        GPIO_SetBits(GPIOD, MOTOR_BIN1_PIN);
        GPIO_ResetBits(GPIOD, MOTOR_BIN2_PIN);
    }
    else if(right_duty_permille < 0)
    {
        GPIO_ResetBits(GPIOD, MOTOR_BIN1_PIN);
        GPIO_SetBits(GPIOD, MOTOR_BIN2_PIN);
    }
    else
    {
        GPIO_ResetBits(GPIOD, MOTOR_BIN1_PIN | MOTOR_BIN2_PIN);
    }

    if((left_duty_permille == 0) && (right_duty_permille == 0))
    {
        motor_standby(0U);
        return;
    }

    motor_standby(1U);
    TIM_SetCompare3(TIM1, motor_duty_to_compare(left_duty_permille));
    TIM_SetCompare4(TIM1, motor_duty_to_compare(right_duty_permille));
}

void motor_left_run(int16_t duty_permille)
{
    motor_set_both(duty_permille, 0);
}

void motor_right_run(int16_t duty_permille)
{
    motor_set_both(0, duty_permille);
}

void motor_init_safe(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef time_base;
    TIM_OCInitTypeDef output_compare;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD |
                           RCC_AHB1Periph_GPIOE, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    /*
     * Configure direction and standby first.  GPIO output data registers
     * reset low, so STBY remains disabled throughout PWM initialization.
     */
    gpio.GPIO_Pin = MOTOR_AIN1_PIN | MOTOR_AIN2_PIN |
                    MOTOR_BIN1_PIN | MOTOR_BIN2_PIN | MOTOR_STBY_PIN;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOD, &gpio);
    GPIO_ResetBits(GPIOD, MOTOR_AIN1_PIN | MOTOR_AIN2_PIN |
                          MOTOR_BIN1_PIN | MOTOR_BIN2_PIN | MOTOR_STBY_PIN);

    gpio.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOE, &gpio);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource13, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource14, GPIO_AF_TIM1);

    /*
     * TIM1 clock is 168 MHz in the board's standard 168 MHz clock setup.
     * 168 MHz / (168 * 1000) = 1 kHz PWM.
     */
    time_base.TIM_Prescaler = 168U - 1U;
    time_base.TIM_CounterMode = TIM_CounterMode_Up;
    time_base.TIM_Period = MOTOR_PWM_PERIOD;
    time_base.TIM_ClockDivision = TIM_CKD_DIV1;
    time_base.TIM_RepetitionCounter = 0U;
    TIM_TimeBaseInit(TIM1, &time_base);

    TIM_OCStructInit(&output_compare);
    output_compare.TIM_OCMode = TIM_OCMode_PWM1;
    output_compare.TIM_OutputState = TIM_OutputState_Enable;
    output_compare.TIM_Pulse = 0U;
    output_compare.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC3Init(TIM1, &output_compare);
    TIM_OC4Init(TIM1, &output_compare);

    TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_OC4PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);

    motor_all_stop();
}
