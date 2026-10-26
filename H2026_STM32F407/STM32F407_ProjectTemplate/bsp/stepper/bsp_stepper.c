#include "bsp_stepper.h"

#define STEPPER_STEP_PIN          GPIO_Pin_6
#define STEPPER_DIR_PIN           GPIO_Pin_7
#define STEPPER_SLP_PIN           GPIO_Pin_5
#define STEPPER_RST_PIN           GPIO_Pin_4

#define STEPPER_TIMER_HZ          20000U
#define STEPPER_TIMER_PRESCALER   (84U - 1U)
#define STEPPER_TIMER_PERIOD      (50U - 1U)

static volatile int16_t g_stepper_speed_steps_s;
static volatile int32_t g_stepper_position_steps;
static volatile uint32_t g_stepper_phase;
static volatile uint8_t g_stepper_enabled;
static volatile uint8_t g_stepper_pulse_high;

static void stepper_write_step(uint8_t high)
{
    if(high != 0U)
    {
        GPIO_SetBits(GPIOA, STEPPER_STEP_PIN);
    }
    else
    {
        GPIO_ResetBits(GPIOA, STEPPER_STEP_PIN);
    }
}

static void stepper_write_direction(int16_t speed_steps_s)
{
    if(speed_steps_s >= 0)
    {
        GPIO_SetBits(GPIOA, STEPPER_DIR_PIN);
    }
    else
    {
        GPIO_ResetBits(GPIOA, STEPPER_DIR_PIN);
    }
}

void stepper_init_safe(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef timer;
    NVIC_InitTypeDef nvic;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);

    gpio.GPIO_Pin = STEPPER_STEP_PIN |
                    STEPPER_DIR_PIN |
                    STEPPER_SLP_PIN |
                    STEPPER_RST_PIN;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOA, &gpio);
    GPIO_ResetBits(GPIOA, STEPPER_STEP_PIN |
                          STEPPER_DIR_PIN |
                          STEPPER_SLP_PIN |
                          STEPPER_RST_PIN);

    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = STEPPER_TIMER_PRESCALER;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    timer.TIM_Period = STEPPER_TIMER_PERIOD;
    timer.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM6, &timer);
    TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
    TIM_ITConfig(TIM6, TIM_IT_Update, ENABLE);

    nvic.NVIC_IRQChannel = TIM6_DAC_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1U;
    nvic.NVIC_IRQChannelSubPriority = 0U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    g_stepper_speed_steps_s = 0;
    g_stepper_position_steps = 0;
    g_stepper_phase = 0U;
    g_stepper_enabled = 0U;
    g_stepper_pulse_high = 0U;

    TIM_Cmd(TIM6, ENABLE);
}

void stepper_enable(uint8_t enable)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    g_stepper_speed_steps_s = 0;
    g_stepper_phase = 0U;
    g_stepper_pulse_high = 0U;
    stepper_write_step(0U);

    if(enable != 0U)
    {
        GPIO_SetBits(GPIOA, STEPPER_RST_PIN);
        GPIO_SetBits(GPIOA, STEPPER_SLP_PIN);
        g_stepper_enabled = 1U;
    }
    else
    {
        GPIO_ResetBits(GPIOA, STEPPER_SLP_PIN);
        GPIO_ResetBits(GPIOA, STEPPER_RST_PIN);
        g_stepper_enabled = 0U;
    }

    if(primask == 0U)
    {
        __enable_irq();
    }
}

uint8_t stepper_is_enabled(void)
{
    return g_stepper_enabled;
}

void stepper_set_speed(int16_t speed_steps_s)
{
    int16_t previous_speed;
    uint32_t primask;

    if(speed_steps_s > STEPPER_SPEED_LIMIT_STEPS_S)
    {
        speed_steps_s = STEPPER_SPEED_LIMIT_STEPS_S;
    }
    else if(speed_steps_s < -STEPPER_SPEED_LIMIT_STEPS_S)
    {
        speed_steps_s = -STEPPER_SPEED_LIMIT_STEPS_S;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    previous_speed = g_stepper_speed_steps_s;

    if(((previous_speed < 0) && (speed_steps_s >= 0)) ||
       ((previous_speed > 0) && (speed_steps_s <= 0)))
    {
        g_stepper_speed_steps_s = 0;
        g_stepper_phase = 0U;
        g_stepper_pulse_high = 0U;
        stepper_write_step(0U);
    }

    stepper_write_direction(speed_steps_s);
    if(g_stepper_enabled != 0U)
    {
        g_stepper_speed_steps_s = speed_steps_s;
    }

    if(primask == 0U)
    {
        __enable_irq();
    }
}

int16_t stepper_get_speed(void)
{
    return g_stepper_speed_steps_s;
}

int32_t stepper_get_position(void)
{
    return g_stepper_position_steps;
}

void stepper_set_position(int32_t position_steps)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    g_stepper_position_steps = position_steps;
    if(primask == 0U)
    {
        __enable_irq();
    }
}

void stepper_emergency_stop(void)
{
    stepper_enable(0U);
}

void TIM6_DAC_IRQHandler(void)
{
    uint32_t speed_magnitude;

    if(TIM_GetITStatus(TIM6, TIM_IT_Update) == RESET)
    {
        return;
    }
    TIM_ClearITPendingBit(TIM6, TIM_IT_Update);

    if((g_stepper_enabled == 0U) ||
       (g_stepper_speed_steps_s == 0))
    {
        stepper_write_step(0U);
        g_stepper_pulse_high = 0U;
        return;
    }

    if(g_stepper_pulse_high != 0U)
    {
        stepper_write_step(0U);
        g_stepper_pulse_high = 0U;
        return;
    }

    speed_magnitude =
        (g_stepper_speed_steps_s < 0) ?
        (uint32_t)(-g_stepper_speed_steps_s) :
        (uint32_t)g_stepper_speed_steps_s;

    g_stepper_phase += speed_magnitude;
    if(g_stepper_phase >= STEPPER_TIMER_HZ)
    {
        g_stepper_phase -= STEPPER_TIMER_HZ;
        stepper_write_step(1U);
        g_stepper_pulse_high = 1U;
        if(g_stepper_speed_steps_s > 0)
        {
            g_stepper_position_steps++;
        }
        else
        {
            g_stepper_position_steps--;
        }
    }
}
