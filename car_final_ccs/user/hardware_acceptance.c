#include "headfile.h"
#include "hardware_acceptance.h"

#define HW_ACCEPT_KEY_DEBOUNCE_TICKS     3U
#define HW_ACCEPT_ENCODER_MIN_ACTIVITY   4U
#define HW_ACCEPT_REFRESH_TICKS         50U
#define HW_ACCEPT_ALL_KEYS            0x0FU
#define HW_ACCEPT_ALL_TRACK           0xFFU

#if APP_PROFILE_HW_ACCEPT && (E2025_ENABLE_K230_UART0_BRIDGE || E2025_ENABLE_K230_UART2)
#error "HW acceptance keeps all K230 UART paths disabled until wiring validation"
#endif

static void hw_accept_uart_puts(const char *text)
{
    while(*text != '\0')
    {
        uart_sendbyte(UART0, (uint8_t) *text++);
    }
}

static void hw_accept_uart_hex8(uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    uart_sendbyte(UART0, (uint8_t) hex[(value >> 4) & 0x0FU]);
    uart_sendbyte(UART0, (uint8_t) hex[value & 0x0FU]);
}

static void hw_accept_uart_u32(uint32_t value)
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

static uint8_t hw_accept_read_key_mask(void)
{
    uint8_t mask = 0U;

    if(gpio_get(GPIOA, DL_GPIO_PIN_27) == 0U) { mask |= 0x01U; }
    if(gpio_get(GPIOA, DL_GPIO_PIN_17) == 0U) { mask |= 0x02U; }
    if(gpio_get(GPIOB, DL_GPIO_PIN_23) == 0U) { mask |= 0x04U; }
    if(gpio_get(GPIOB, DL_GPIO_PIN_27) == 0U) { mask |= 0x08U; }
    return mask;
}

static void hw_accept_update_keys(uint8_t raw_mask,
                                  uint8_t *seen_mask,
                                  uint8_t debounce[4])
{
    uint8_t key;

    for(key = 0U; key < 4U; key++)
    {
        uint8_t bit = (uint8_t) (1U << key);
        if(raw_mask & bit)
        {
            if(debounce[key] < HW_ACCEPT_KEY_DEBOUNCE_TICKS)
            {
                debounce[key]++;
            }
            if(debounce[key] >= HW_ACCEPT_KEY_DEBOUNCE_TICKS)
            {
                *seen_mask |= bit;
            }
        }
        else
        {
            debounce[key] = 0U;
        }
    }
}

static uint32_t hw_accept_magnitude_i32(int32_t value)
{
    if(value < 0)
    {
        return (uint32_t) (-(value + 1)) + 1U;
    }
    return (uint32_t) value;
}

static uint32_t hw_accept_saturating_add(uint32_t total, uint32_t delta)
{
    if((0xFFFFFFFFU - total) < delta)
    {
        return 0xFFFFFFFFU;
    }
    return total + delta;
}

static uint32_t hw_accept_display_limit(uint32_t value)
{
    return (value > 999U) ? 999U : value;
}

static void hw_accept_show_oled(uint8_t key_seen,
                                uint8_t track_pass,
                                uint32_t left_activity,
                                uint32_t right_activity)
{
    OLED_ShowString(1, 1, "HW ACCEPT SAFE  ");
    OLED_ShowString(2, 1, "                ");
    OLED_ShowString(3, 1, "                ");
    OLED_ShowString(4, 1, "                ");

    if(key_seen != HW_ACCEPT_ALL_KEYS)
    {
        OLED_ShowString(2, 1, "1.KEY SW1-SW4  ");
        OLED_ShowString(3, 1, "SEEN:0x");
        OLED_ShowHexNum(3, 8, key_seen, 2);
        OLED_ShowString(4, 1, "PRESS EACH KEY  ");
    }
    else if(track_pass != HW_ACCEPT_ALL_TRACK)
    {
        OLED_ShowString(2, 1, "2.TRACK D1-D8  ");
        OLED_ShowString(3, 1, "PASS:0x");
        OLED_ShowHexNum(3, 8, track_pass, 2);
        OLED_ShowString(4, 1, "WHITE+BLACK EACH");
    }
    else if(left_activity < HW_ACCEPT_ENCODER_MIN_ACTIVITY ||
            right_activity < HW_ACCEPT_ENCODER_MIN_ACTIVITY)
    {
        OLED_ShowString(2, 1, "3.TURN WHEELS   ");
        OLED_ShowString(3, 1, "L:");
        OLED_ShowNum(3, 3, hw_accept_display_limit(left_activity), 3);
        OLED_ShowString(3, 8, "R:");
        OLED_ShowNum(3, 10, hw_accept_display_limit(right_activity), 3);
        OLED_ShowString(4, 1, "MOTORS DISABLED ");
    }
    else
    {
        OLED_ShowString(2, 1, "INPUT TEST PASS ");
        OLED_ShowString(3, 1, "MOTOR/STBY OFF  ");
        OLED_ShowString(4, 1, "K230:BLOCKED    ");
    }
}

static void hw_accept_send_serial(uint8_t key_raw,
                                  uint8_t key_seen,
                                  uint8_t track_pattern,
                                  uint8_t track_pass,
                                  uint32_t left_activity,
                                  uint32_t right_activity)
{
    uint8_t inputs_pass =
        (key_seen == HW_ACCEPT_ALL_KEYS &&
         track_pass == HW_ACCEPT_ALL_TRACK &&
         left_activity >= HW_ACCEPT_ENCODER_MIN_ACTIVITY &&
         right_activity >= HW_ACCEPT_ENCODER_MIN_ACTIVITY) ? 1U : 0U;

    hw_accept_uart_puts("HWACC KEY_RAW=0x");
    hw_accept_uart_hex8(key_raw);
    hw_accept_uart_puts(" KEY_PASS=0x");
    hw_accept_uart_hex8(key_seen);
    hw_accept_uart_puts(" TRACK_BLACK=0x");
    hw_accept_uart_hex8(track_pattern);
    hw_accept_uart_puts(" TRACK_PASS=0x");
    hw_accept_uart_hex8(track_pass);
    hw_accept_uart_puts(" ENC_L=");
    hw_accept_uart_u32(left_activity);
    hw_accept_uart_puts(" ENC_R=");
    hw_accept_uart_u32(right_activity);
    hw_accept_uart_puts(" MOTOR=0 STBY=0 K230=BLOCKED_UART2_UNVERIFIED RESULT=");
    hw_accept_uart_puts(inputs_pass ? "INPUT_PASS" : "TESTING");
    hw_accept_uart_puts("\r\n");
}

void hardware_acceptance_run(void)
{
    uint8_t key_seen = 0U;
    uint8_t key_debounce[4] = {0U, 0U, 0U, 0U};
    uint8_t track_white_seen = 0U;
    uint8_t track_black_seen = 0U;
    uint8_t refresh_ticks = 0U;
    uint32_t left_activity = 0U;
    uint32_t right_activity = 0U;

    /* Configure every motor-related pin, then hold all power outputs disabled. */
    motor_safe_init_disabled();
    Set_left_pwm(0);
    Set_right_pwm(0);
    motor_set_standby(0U);

    key_init();
    xunji_init();
    encoder_init();
    OLED_Init();
    OLED_Clear();
    uart0_init();

    hw_accept_uart_puts(
        "\r\nHW ACCEPT SAFE START: PWM=0, PB16/STBY=0, no STEP/DIR/laser output\r\n");
    hw_accept_uart_puts(
        "Order: press SW1..SW4; move black tape across D1..D8; turn both wheels by hand\r\n");
    hw_accept_uart_puts(
        "K230 is BLOCKED until PB15/PA22 UART2 is validated; UART0 remains CH340 debug\r\n");

    while(1)
    {
        int32_t encoder1;
        int32_t encoder2;
        uint8_t key_raw;
        uint8_t track_pattern;
        uint8_t track_pass;

        /* Reassert the safe output state on every 10 ms polling cycle. */
        Set_left_pwm(0);
        Set_right_pwm(0);
        motor_set_standby(0U);

        key_raw = hw_accept_read_key_mask();
        hw_accept_update_keys(key_raw, &key_seen, key_debounce);

        track_pattern = track_read_pattern();
        track_black_seen |= track_pattern;
        track_white_seen |= (uint8_t) (~track_pattern);
        track_pass = (uint8_t) (track_black_seen & track_white_seen);

        encoder_read_and_reset(&encoder1, &encoder2);
        left_activity = hw_accept_saturating_add(
            left_activity, hw_accept_magnitude_i32(encoder1));
        right_activity = hw_accept_saturating_add(
            right_activity, hw_accept_magnitude_i32(encoder2));

        if(refresh_ticks == 0U)
        {
            hw_accept_show_oled(key_seen, track_pass,
                                left_activity, right_activity);
            hw_accept_send_serial(key_raw, key_seen, track_pattern, track_pass,
                                  left_activity, right_activity);
        }

        refresh_ticks++;
        if(refresh_ticks >= HW_ACCEPT_REFRESH_TICKS)
        {
            refresh_ticks = 0U;
        }
        delay_ms(10);
    }
}
