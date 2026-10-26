#include "headfile.h"
#include "encoder_diag.h"

#define ENCODER_DIAG_REFRESH_TICKS 10U

static void encoder_diag_uart_puts(const char *text)
{
    while(*text != '\0')
    {
        uart_sendbyte(UART0, (uint8_t) *text++);
    }
}

static void encoder_diag_uart_u32(uint32_t value)
{
    char buffer[10];
    uint8_t length = 0U;

    if(value == 0U)
    {
        uart_sendbyte(UART0, '0');
        return;
    }

    while(value > 0U && length < sizeof(buffer))
    {
        buffer[length++] = (char) ('0' + (value % 10U));
        value /= 10U;
    }
    while(length > 0U)
    {
        uart_sendbyte(UART0, (uint8_t) buffer[--length]);
    }
}

static void encoder_diag_uart_i32(int32_t value)
{
    uint32_t magnitude;

    if(value < 0)
    {
        uart_sendbyte(UART0, '-');
        magnitude = (uint32_t) (-(value + 1)) + 1U;
    }
    else
    {
        uart_sendbyte(UART0, '+');
        magnitude = (uint32_t) value;
    }
    encoder_diag_uart_u32(magnitude);
}

static int32_t encoder_diag_limit_display(int32_t value, int32_t limit)
{
    if(value > limit)
    {
        return limit;
    }
    if(value < -limit)
    {
        return -limit;
    }
    return value;
}

static void encoder_diag_show_oled(int32_t left_delta,
                                   int32_t right_delta,
                                   int32_t left_total,
                                   int32_t right_total)
{
    OLED_ShowString(1, 1, "ENC DIAG SAFE   ");
    OLED_ShowString(2, 1, "D L:");
    OLED_ShowSignedNum(2, 5,
                       encoder_diag_limit_display(left_delta, 999), 3);
    OLED_ShowString(2, 9, " R:");
    OLED_ShowSignedNum(2, 12,
                       encoder_diag_limit_display(right_delta, 999), 3);
    OLED_ShowString(3, 1, "L:");
    OLED_ShowSignedNum(3, 3,
                       encoder_diag_limit_display(left_total, 999999), 6);
    OLED_ShowString(4, 1, "R:");
    OLED_ShowSignedNum(4, 3,
                       encoder_diag_limit_display(right_total, 999999), 6);
}

static void encoder_diag_send_serial(int32_t raw1,
                                     int32_t raw2,
                                     int32_t left_delta,
                                     int32_t right_delta,
                                     int32_t left_total,
                                     int32_t right_total)
{
    encoder_diag_uart_puts("ENC RAW1=");
    encoder_diag_uart_i32(raw1);
    encoder_diag_uart_puts(" RAW2=");
    encoder_diag_uart_i32(raw2);
    encoder_diag_uart_puts(" APP_L=");
    encoder_diag_uart_i32(left_delta);
    encoder_diag_uart_puts(" APP_R=");
    encoder_diag_uart_i32(right_delta);
    encoder_diag_uart_puts(" TOTAL_L=");
    encoder_diag_uart_i32(left_total);
    encoder_diag_uart_puts(" TOTAL_R=");
    encoder_diag_uart_i32(right_total);
    encoder_diag_uart_puts("\r\n");
}

void encoder_diag_run(void)
{
    int32_t left_total = 0;
    int32_t right_total = 0;
    uint8_t refresh_ticks = 0U;

    /* PWM starts at zero; STBY is then forced low and reasserted every loop. */
    motor_safe_init_disabled();
    Set_left_pwm(0);
    Set_right_pwm(0);
    motor_set_standby(0U);

    OLED_Init();
    OLED_Clear();
    uart0_init();
    encoder_init();
    encoder_diag_uart_puts(
        "ENC DIAG SAFE: turn wheels by hand, motors=0, PB16/STBY=0\r\n");

    while(1)
    {
        Set_left_pwm(0);
        Set_right_pwm(0);
        motor_set_standby(0U);

        if(refresh_ticks == 0U)
        {
            int32_t raw1;
            int32_t raw2;
            int32_t left_delta;
            int32_t right_delta;

            encoder_read_and_reset(&raw1, &raw2);

            /* Match the signs already used by the E2025 10 ms control tick. */
            left_delta = encoder_left_apply_sign(raw1);
            right_delta = encoder_right_apply_sign(raw2);
            left_total += left_delta;
            right_total += right_delta;

            encoder_diag_show_oled(left_delta, right_delta,
                                   left_total, right_total);
            encoder_diag_send_serial(raw1, raw2, left_delta, right_delta,
                                     left_total, right_total);
        }

        refresh_ticks++;
        if(refresh_ticks >= ENCODER_DIAG_REFRESH_TICKS)
        {
            refresh_ticks = 0U;
        }
        delay_ms(10);
    }
}
