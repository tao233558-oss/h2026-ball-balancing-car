#include "bsp_encoder.h"

static void encoder_timer_init(TIM_TypeDef *timer)
{
    TIM_TimeBaseInitTypeDef time_base;

    TIM_TimeBaseStructInit(&time_base);
    time_base.TIM_Prescaler = 0U;
    time_base.TIM_CounterMode = TIM_CounterMode_Up;
    time_base.TIM_Period = 0xFFFFU;
    time_base.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(timer, &time_base);

    TIM_EncoderInterfaceConfig(timer,
                               TIM_EncoderMode_TI12,
                               TIM_ICPolarity_Rising,
                               TIM_ICPolarity_Rising);
    TIM_SetCounter(timer, 0U);
    TIM_Cmd(timer, ENABLE);
}

void encoder_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3 |
                           RCC_APB1Periph_TIM4, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 |
                    GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &gpio);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF_TIM3);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF_TIM3);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_TIM4);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_TIM4);

    encoder_timer_init(TIM3);
    encoder_timer_init(TIM4);
}

void encoder_reset(void)
{
    TIM_SetCounter(TIM3, 0U);
    TIM_SetCounter(TIM4, 0U);
}

int16_t encoder_left_count(void)
{
    return (int16_t)TIM_GetCounter(TIM3);
}

int16_t encoder_right_count(void)
{
    return (int16_t)TIM_GetCounter(TIM4);
}
