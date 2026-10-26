#include "headfile.h"
#include "dual_motor_diag.h"

#define DUAL_MOTOR_DIAG_PWM                    600
#define DUAL_MOTOR_ARM_RELEASE_TICKS            50U
#define DUAL_MOTOR_DEBOUNCE_TICKS                3U
#define DUAL_MOTOR_SAMPLE_TICKS                 10U

typedef struct
{
    uint8_t stable_level;
    uint8_t candidate_level;
    uint8_t candidate_ticks;
    uint8_t pressed_event;
} dual_motor_button_t;

static void dual_motor_button_init(dual_motor_button_t *button,
                                   uint8_t raw_level)
{
    button->stable_level = raw_level;
    button->candidate_level = raw_level;
    button->candidate_ticks = DUAL_MOTOR_DEBOUNCE_TICKS;
    button->pressed_event = 0U;
}

static void dual_motor_button_update(dual_motor_button_t *button,
                                     uint8_t raw_level)
{
    button->pressed_event = 0U;
    if(raw_level != button->candidate_level)
    {
        button->candidate_level = raw_level;
        button->candidate_ticks = 1U;
        return;
    }

    if(button->candidate_ticks < DUAL_MOTOR_DEBOUNCE_TICKS)
    {
        button->candidate_ticks++;
    }
    if(button->candidate_ticks >= DUAL_MOTOR_DEBOUNCE_TICKS &&
       button->stable_level != button->candidate_level)
    {
        button->stable_level = button->candidate_level;
        if(button->stable_level == 0U)
        {
            button->pressed_event = 1U;
        }
    }
}

static uint32_t dual_motor_abs_i32(int32_t value)
{
    if(value < 0)
    {
        return (uint32_t) (-(value + 1)) + 1U;
    }
    return (uint32_t) value;
}

static void dual_motor_stop(void)
{
    motor_set_standby(0U);
    Set_left_pwm(0);
    Set_right_pwm(0);
}

static void dual_motor_apply(uint8_t run, uint8_t reverse)
{
    static uint8_t was_running = 0U;
    int command;

    if(!run)
    {
        dual_motor_stop();
        was_running = 0U;
        return;
    }
    if(was_running)
    {
        return;
    }

    dual_motor_stop();
    command = reverse ? -DUAL_MOTOR_DIAG_PWM : DUAL_MOTOR_DIAG_PWM;
    Set_left_pwm(command);
    Set_right_pwm(command);
    /* Direction and both PWM channels are established before STBY rises. */
    motor_set_standby(1U);
    was_running = 1U;
}

static int32_t dual_motor_limit_display(int32_t value)
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

static void dual_motor_uart_puts(const char *text)
{
    while(*text != '\0')
    {
        uart_sendbyte(UART0, (uint8_t) *text++);
    }
}

static void dual_motor_uart_u16(uint16_t value)
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

static void dual_motor_uart_i32(int32_t value)
{
    uint32_t magnitude = dual_motor_abs_i32(value);

    uart_sendbyte(UART0, (value < 0) ? '-' : '+');
    if(magnitude > 65535U)
    {
        magnitude = 65535U;
    }
    dual_motor_uart_u16((uint16_t) magnitude);
}

static void dual_motor_show(uint8_t reverse,
                            uint8_t armed,
                            uint8_t running,
                            dual_motor_diag_fault_t fault,
                            int32_t left_delta,
                            int32_t right_delta)
{
    if(fault != DUAL_MOTOR_FAULT_NONE)
    {
        OLED_ShowString(1, 1, "DUAL FAULT      ");
    }
    else if(running)
    {
        OLED_ShowString(1, 1, "DUAL RUNNING    ");
    }
    else
    {
        OLED_ShowString(1, 1, "DUAL TEST SAFE  ");
    }

    OLED_ShowString(2, 1, "DIR:");
    OLED_ShowChar(2, 5, reverse ? 'B' : 'F');
    OLED_ShowString(2, 7, "PWM:");
    OLED_ShowNum(2, 11, DUAL_MOTOR_DIAG_PWM, 4);

    OLED_ShowString(3, 1, "L:");
    OLED_ShowSignedNum(3, 3, dual_motor_limit_display(left_delta), 3);
    OLED_ShowString(3, 8, "R:");
    OLED_ShowSignedNum(3, 10, dual_motor_limit_display(right_delta), 3);
    OLED_ShowString(3, 15, armed ? "A" : "-");

    OLED_ShowString(4, 1, "                ");
    OLED_ShowString(4, 1, "F:");
    OLED_ShowString(4, 3, (char *) dual_motor_fault_name(fault));
}

static void dual_motor_send_serial(uint8_t reverse,
                                   uint8_t armed,
                                   uint8_t running,
                                   dual_motor_diag_fault_t fault,
                                   int32_t left_delta,
                                   int32_t right_delta)
{
    dual_motor_uart_puts("DUAL DIR=");
    uart_sendbyte(UART0, reverse ? 'B' : 'F');
    dual_motor_uart_puts(" PWM=");
    dual_motor_uart_u16(DUAL_MOTOR_DIAG_PWM);
    dual_motor_uart_puts(" ARM=");
    uart_sendbyte(UART0, armed ? '1' : '0');
    dual_motor_uart_puts(" RUN=");
    uart_sendbyte(UART0, running ? '1' : '0');
    dual_motor_uart_puts(" FAULT=");
    dual_motor_uart_puts(dual_motor_fault_name(fault));
    dual_motor_uart_puts(" dL=");
    dual_motor_uart_i32(left_delta);
    dual_motor_uart_puts(" dR=");
    dual_motor_uart_i32(right_delta);
    dual_motor_uart_puts("\r\n");
}

void dual_motor_diag_run(void)
{
    dual_motor_button_t sw2;
    dual_motor_button_t sw3;
    dual_motor_guard_t monitor;
    dual_motor_diag_fault_t fault = DUAL_MOTOR_FAULT_NONE;
    uint8_t reverse = 0U;
    uint8_t armed = 0U;
    uint8_t release_ticks = 0U;
    uint8_t sample_ticks = 0U;
    uint8_t previous_running = 0U;
    int32_t left_delta = 0;
    int32_t right_delta = 0;

    motor_safe_init_disabled();
    key_init();
    OLED_Init();
    OLED_Clear();
    uart0_init();
    encoder_init();
    dual_motor_guard_reset(&monitor);

    dual_motor_button_init(&sw2, gpio_get(GPIOA, DL_GPIO_PIN_17));
    dual_motor_button_init(&sw3, gpio_get(GPIOB, DL_GPIO_PIN_23));
    dual_motor_uart_puts(
        "DUAL MOTOR SAFE: wheels up; hold SW2 run, SW3 dir, SW4 E-STOP\r\n");

    while(1)
    {
        uint8_t sw2_raw = gpio_get(GPIOA, DL_GPIO_PIN_17);
        uint8_t sw4_raw = gpio_get(GPIOB, DL_GPIO_PIN_27);
        uint8_t requested_run;
        uint8_t running;
        uint8_t sample_due;
        uint8_t fault_changed = 0U;

        /* SW4 bypasses debounce and latches a fault until reset/power cycle. */
        if(sw4_raw == 0U && fault == DUAL_MOTOR_FAULT_NONE)
        {
            fault = DUAL_MOTOR_FAULT_ESTOP;
            fault_changed = 1U;
        }

        dual_motor_button_update(&sw2, sw2_raw);
        dual_motor_button_update(&sw3, gpio_get(GPIOB, DL_GPIO_PIN_23));

        if(fault == DUAL_MOTOR_FAULT_NONE && sw2.stable_level != 0U)
        {
            if(release_ticks < DUAL_MOTOR_ARM_RELEASE_TICKS)
            {
                release_ticks++;
            }
            if(release_ticks >= DUAL_MOTOR_ARM_RELEASE_TICKS)
            {
                armed = 1U;
            }
        }
        else if(!armed)
        {
            release_ticks = 0U;
        }

        requested_run = (armed && fault == DUAL_MOTOR_FAULT_NONE &&
                         sw2.stable_level == 0U && sw2_raw == 0U) ? 1U : 0U;
        if(!requested_run && fault == DUAL_MOTOR_FAULT_NONE &&
           sw3.pressed_event)
        {
            reverse ^= 1U;
        }

        sample_due = (sample_ticks == 0U) ? 1U : 0U;
        if(requested_run && !previous_running)
        {
            int32_t unused1;
            int32_t unused2;
            encoder_read_and_reset(&unused1, &unused2);
            left_delta = 0;
            right_delta = 0;
            dual_motor_guard_reset(&monitor);
        }
        else if(sample_due)
        {
            int32_t raw1;
            int32_t raw2;
            encoder_read_and_reset(&raw1, &raw2);
            left_delta = encoder_left_apply_sign(raw1);
            right_delta = encoder_right_apply_sign(raw2);

            if(requested_run)
            {
                dual_motor_diag_fault_t detected = dual_motor_guard_step(
                    &monitor, reverse, left_delta, right_delta);
                if(detected != DUAL_MOTOR_FAULT_NONE)
                {
                    fault = detected;
                    fault_changed = 1U;
                }
            }
            else
            {
                dual_motor_guard_reset(&monitor);
            }
        }

        running = (requested_run && fault == DUAL_MOTOR_FAULT_NONE) ? 1U : 0U;
        dual_motor_apply(running, reverse);
        previous_running = running;

        if(sample_due || fault_changed)
        {
            dual_motor_show(reverse, armed, running, fault,
                            left_delta, right_delta);
            dual_motor_send_serial(reverse, armed, running, fault,
                                   left_delta, right_delta);
        }

        sample_ticks++;
        if(sample_ticks >= DUAL_MOTOR_SAMPLE_TICKS)
        {
            sample_ticks = 0U;
        }
        delay_ms(10);
    }
}
