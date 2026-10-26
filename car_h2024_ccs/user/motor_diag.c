#include "headfile.h"
#include "motor_diag.h"

#define MOTOR_DIAG_PWM              1200
#define MOTOR_DIAG_ARM_RELEASE_TICKS  50U
#define MOTOR_DIAG_REFRESH_TICKS      10U
#define MOTOR_DIAG_DEBOUNCE_TICKS      3U

typedef struct
{
    uint8_t stable_level;
    uint8_t candidate_level;
    uint8_t candidate_ticks;
    uint8_t pressed_event;
} motor_diag_button_t;

static void motor_diag_button_init(motor_diag_button_t *button,
                                   uint8_t raw_level)
{
    button->stable_level = raw_level;
    button->candidate_level = raw_level;
    button->candidate_ticks = MOTOR_DIAG_DEBOUNCE_TICKS;
    button->pressed_event = 0U;
}

static void motor_diag_button_update(motor_diag_button_t *button,
                                     uint8_t raw_level)
{
    button->pressed_event = 0U;
    if(raw_level != button->candidate_level)
    {
        button->candidate_level = raw_level;
        button->candidate_ticks = 1U;
        return;
    }

    if(button->candidate_ticks < MOTOR_DIAG_DEBOUNCE_TICKS)
    {
        button->candidate_ticks++;
    }
    if(button->candidate_ticks >= MOTOR_DIAG_DEBOUNCE_TICKS &&
       button->stable_level != button->candidate_level)
    {
        button->stable_level = button->candidate_level;
        if(button->stable_level == 0U)
        {
            button->pressed_event = 1U;
        }
    }
}

static void motor_diag_uart_puts(const char *text)
{
    while(*text != '\0')
    {
        uart_sendbyte(UART0, (uint8_t) *text++);
    }
}

static void motor_diag_uart_u16(uint16_t value)
{
    char buffer[5];
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

static void motor_diag_uart_i32(int32_t value)
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

    if(magnitude > 65535U)
    {
        magnitude = 65535U;
    }
    motor_diag_uart_u16((uint16_t) magnitude);
}

static int32_t motor_diag_limit_display(int32_t value)
{
    if(value > 999)
    {
        return 999;
    }
    if(value < -999)
    {
        return -999;
    }
    return value;
}

static void motor_diag_stop(void)
{
    motor_set_standby(0U);
    Set_left_pwm(0);
    Set_right_pwm(0);
}

static void motor_diag_apply(uint8_t run,
                             uint8_t select_right,
                             uint8_t reverse)
{
    static uint8_t was_running = 0U;
    static uint8_t last_select_right = 0U;
    static uint8_t last_reverse = 0U;
    int command;

    if(!run)
    {
        motor_diag_stop();
        was_running = 0U;
        return;
    }

    if(was_running && select_right == last_select_right &&
       reverse == last_reverse)
    {
        return;
    }

    motor_diag_stop();
    command = reverse ? -MOTOR_DIAG_PWM : MOTOR_DIAG_PWM;
    if(select_right)
    {
        Set_right_pwm(command);
    }
    else
    {
        Set_left_pwm(command);
    }

    /* Direction and PWM are established while disabled; STBY rises last. */
    motor_set_standby(1U);
    was_running = 1U;
    last_select_right = select_right;
    last_reverse = reverse;
}

static void motor_diag_show_oled(uint8_t select_right,
                                 uint8_t reverse,
                                 uint8_t armed,
                                 uint8_t running,
                                 uint8_t emergency,
                                 int32_t left_delta,
                                 int32_t right_delta,
                                 uint8_t mode_raw,
                                 uint8_t start_raw)
{
    if(emergency)
    {
        OLED_ShowString(1, 1, "MOTOR E-STOP    ");
    }
    else if(running)
    {
        OLED_ShowString(1, 1, "HOLD RUNNING    ");
    }
    else
    {
        OLED_ShowString(1, 1, "MOTOR TEST SAFE ");
    }

    OLED_ShowString(2, 1, "SEL:");
    OLED_ShowChar(2, 5, select_right ? 'R' : 'L');
    OLED_ShowString(2, 7, "DIR:");
    OLED_ShowChar(2, 11, reverse ? 'B' : 'F');

    OLED_ShowString(3, 1, "ARM:");
    OLED_ShowChar(3, 5, armed ? 'Y' : 'N');
    OLED_ShowString(3, 7, "M:");
    OLED_ShowChar(3, 9, mode_raw ? '1' : '0');
    OLED_ShowString(3, 11, "S:");
    OLED_ShowChar(3, 13, start_raw ? '1' : '0');

    OLED_ShowString(4, 1, "D L:");
    OLED_ShowSignedNum(4, 5, motor_diag_limit_display(left_delta), 3);
    OLED_ShowString(4, 9, " R:");
    OLED_ShowSignedNum(4, 12, motor_diag_limit_display(right_delta), 3);
}

static void motor_diag_send_serial(uint8_t select_right,
                                   uint8_t reverse,
                                   uint8_t armed,
                                   uint8_t running,
                                   uint8_t emergency,
                                   int32_t left_delta,
                                   int32_t right_delta)
{
    motor_diag_uart_puts("MOTOR SEL=");
    uart_sendbyte(UART0, select_right ? 'R' : 'L');
    motor_diag_uart_puts(" DIR=");
    uart_sendbyte(UART0, reverse ? 'B' : 'F');
    motor_diag_uart_puts(" PWM=");
    motor_diag_uart_u16(MOTOR_DIAG_PWM);
    motor_diag_uart_puts(" ARM=");
    uart_sendbyte(UART0, armed ? '1' : '0');
    motor_diag_uart_puts(" RUN=");
    uart_sendbyte(UART0, running ? '1' : '0');
    motor_diag_uart_puts(" ESTOP=");
    uart_sendbyte(UART0, emergency ? '1' : '0');
    motor_diag_uart_puts(" dL=");
    motor_diag_uart_i32(left_delta);
    motor_diag_uart_puts(" dR=");
    motor_diag_uart_i32(right_delta);
    motor_diag_uart_puts("\r\n");
}

void motor_diag_run(void)
{
    motor_diag_button_t sw2;
    uint8_t select_right = 0U;
    uint8_t reverse = 0U;
    uint8_t armed = 0U;
    uint8_t emergency = 0U;
    uint8_t release_ticks = 0U;
    uint8_t refresh_ticks = 0U;
    uint8_t previous_running = 0U;

    motor_safe_init_disabled();
    key_init();
    OLED_Init();
    OLED_Clear();
    uart0_init();
    encoder_init();

    motor_diag_button_init(&sw2, gpio_get(GPIOA, DL_GPIO_PIN_27));
    motor_diag_uart_puts(
        "MOTOR TEST SAFE: wheels up; hold SET to run; each release selects the other channel\r\n");

    while(1)
    {
        uint8_t sw1_raw = gpio_get(GPIOB, DL_GPIO_PIN_23);
        uint8_t sw2_raw = gpio_get(GPIOA, DL_GPIO_PIN_27);
        uint8_t running;

        motor_diag_button_update(&sw2, sw2_raw);

        if(!emergency && sw2.stable_level != 0U)
        {
            if(release_ticks < MOTOR_DIAG_ARM_RELEASE_TICKS)
            {
                release_ticks++;
            }
            if(release_ticks >= MOTOR_DIAG_ARM_RELEASE_TICKS)
            {
                armed = 1U;
            }
        }
        else if(!armed)
        {
            release_ticks = 0U;
        }

        /* Raw release stops in this same loop; debounce is required only to start. */
        running = (armed && !emergency && sw2.stable_level == 0U &&
                   sw2_raw == 0U && sw1_raw != 0U) ? 1U : 0U;
        if(previous_running && !running && !emergency)
        {
            select_right ^= 1U;
        }
        previous_running = running;

        motor_diag_apply(running, select_right, reverse);

        if(refresh_ticks == 0U)
        {
            int32_t raw1;
            int32_t raw2;
            int32_t left_delta;
            int32_t right_delta;

            encoder_read_and_reset(&raw1, &raw2);
            left_delta = encoder_left_apply_sign(raw1);
            right_delta = encoder_right_apply_sign(raw2);
            motor_diag_show_oled(select_right, reverse, armed, running,
                                 emergency, left_delta, right_delta,
                                 sw1_raw, sw2_raw);
            motor_diag_send_serial(select_right, reverse, armed, running,
                                   emergency, left_delta, right_delta);
        }

        refresh_ticks++;
        if(refresh_ticks >= MOTOR_DIAG_REFRESH_TICKS)
        {
            refresh_ticks = 0U;
        }
        delay_ms(10);
    }
}
