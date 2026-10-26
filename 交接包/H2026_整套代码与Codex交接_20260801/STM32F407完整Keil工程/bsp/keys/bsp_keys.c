#include "bsp_keys.h"

void keys_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOE, &gpio);
}

uint8_t keys_read_pressed_mask(void)
{
    uint8_t mask = 0U;

    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_7) == Bit_RESET)
    {
        mask |= KEY_SW1_MASK;
    }
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_8) == Bit_RESET)
    {
        mask |= KEY_SW2_MASK;
    }
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_9) == Bit_RESET)
    {
        mask |= KEY_SW3_MASK;
    }
    if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_10) == Bit_RESET)
    {
        mask |= KEY_SW4_MASK;
    }

    return mask;
}
