#include "board.h"
#include "bsp_motor.h"
#include "bsp_stepper.h"
#include "oled.h"

/*
 * Four-channel digital line-sensor diagnostic firmware.
 *
 * Wiring supplied by the user:
 *   S4 -> PC4
 *   S3 -> PC5
 *   S2 -> PB0
 *   S1 -> PB1
 *
 * Motors and stepper remain disabled throughout this test.
 */

static void heartbeat_led_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_2;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &gpio);
    GPIO_ResetBits(GPIOB, GPIO_Pin_2);
}

static void line_sensor_gpio_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB |
                           RCC_AHB1Periph_GPIOC, ENABLE);

    gpio.GPIO_Mode = GPIO_Mode_IN;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;

    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_Init(GPIOB, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_Init(GPIOC, &gpio);
}

static uint8_t read_s1(void)
{
    return (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) != Bit_RESET) ?
           1U : 0U;
}

static uint8_t read_s2(void)
{
    return (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) != Bit_RESET) ?
           1U : 0U;
}

static uint8_t read_s3(void)
{
    return (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_5) != Bit_RESET) ?
           1U : 0U;
}

static uint8_t read_s4(void)
{
    return (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_4) != Bit_RESET) ?
           1U : 0U;
}

int main(void)
{
    uint8_t sensor_line_12[] = "S1:0  S2:0";
    uint8_t sensor_line_34[] = "S3:0  S4:0";
    uint16_t heartbeat_ms = 0U;

    board_init();
    heartbeat_led_init();
    motor_init_safe();
    motor_all_stop();
    stepper_init_safe();
    stepper_emergency_stop();
    line_sensor_gpio_init();

    OLED_Init();
    OLED_Clear();
    OLED_ShowString(0U, 0U,
                    (uint8_t *)"4CH GPIO TEST V1", 8U, 1U);
    OLED_ShowString(0U, 48U,
                    (uint8_t *)"WHITE=1 BLACK=0?", 8U, 1U);
    OLED_Refresh();

    while(1)
    {
        sensor_line_12[3] = (uint8_t)('0' + read_s1());
        sensor_line_12[9] = (uint8_t)('0' + read_s2());
        sensor_line_34[3] = (uint8_t)('0' + read_s3());
        sensor_line_34[9] = (uint8_t)('0' + read_s4());

        OLED_ShowString(0U, 16U, sensor_line_12, 8U, 1U);
        OLED_ShowString(0U, 32U, sensor_line_34, 8U, 1U);
        OLED_Refresh();

        delay_ms(100U);
        heartbeat_ms = (uint16_t)(heartbeat_ms + 100U);
        if(heartbeat_ms >= 500U)
        {
            GPIO_ToggleBits(GPIOB, GPIO_Pin_2);
            heartbeat_ms = 0U;
        }
    }
}
