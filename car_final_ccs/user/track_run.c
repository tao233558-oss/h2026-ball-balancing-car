#include "headfile.h"
#include "track_run.h"
#include "xunji.h"

#define TRACK_RUN_LOOP_MS                  10U
#define TRACK_RUN_REPORT_TICKS             10U
#define TRACK_RUN_DEBOUNCE_TICKS            3U
#define TRACK_RUN_LOST_SEARCH_TICKS        20U
#define TRACK_RUN_LOST_STOP_TICKS          35U

#define TRACK_RUN_BASE_START             1200
#define TRACK_RUN_BASE_MIN                800
#define TRACK_RUN_BASE_MAX               2600
#define TRACK_RUN_BASE_STEP               100
#define TRACK_RUN_SEARCH_PWM              900
#define TRACK_RUN_PWM_LIMIT              3200

/* Integer PD gains. The sensor error range is approximately -35 to +35. */
#define TRACK_RUN_KP                       32
#define TRACK_RUN_KD                       12

#define TRACK_RUN_STATUS_LED_PORT       GPIOB
#define TRACK_RUN_STATUS_LED_PIN        DL_GPIO_PIN_22
#define TRACK_RUN_STATUS_LED_IOMUX      PB22

typedef struct
{
    uint8_t stable_level;
    uint8_t candidate_level;
    uint8_t candidate_ticks;
    uint8_t pressed_event;
} track_run_button_t;

static void track_run_button_init(track_run_button_t *button,
                                  uint8_t raw_level)
{
    button->stable_level = raw_level;
    button->candidate_level = raw_level;
    button->candidate_ticks = TRACK_RUN_DEBOUNCE_TICKS;
    button->pressed_event = 0U;
}

static void track_run_button_update(track_run_button_t *button,
                                    uint8_t raw_level)
{
    button->pressed_event = 0U;

    if(raw_level != button->candidate_level)
    {
        button->candidate_level = raw_level;
        button->candidate_ticks = 1U;
        return;
    }

    if(button->candidate_ticks < TRACK_RUN_DEBOUNCE_TICKS)
    {
        button->candidate_ticks++;
    }

    if(button->candidate_ticks >= TRACK_RUN_DEBOUNCE_TICKS &&
       button->stable_level != button->candidate_level)
    {
        button->stable_level = button->candidate_level;
        if(button->stable_level == 0U)
        {
            button->pressed_event = 1U;
        }
    }
}

static int track_run_clamp(int value, int minimum, int maximum)
{
    if(value < minimum)
    {
        return minimum;
    }
    if(value > maximum)
    {
        return maximum;
    }
    return value;
}

static void track_run_stop_motors(void)
{
    Set_left_pwm(0);
    Set_right_pwm(0);
    motor_set_standby(0U);
}

static void track_run_oled_fault_loop(void)
{
    track_run_stop_motors();
    while(1)
    {
        gpio_toggle(TRACK_RUN_STATUS_LED_PORT, TRACK_RUN_STATUS_LED_PIN);
        delay_ms(100);
    }
}

static void track_run_uart_puts(const char *text)
{
    while(*text != '\0')
    {
        uart_sendbyte(UART0, (uint8_t) *text++);
    }
}

static void track_run_uart_u16(uint16_t value)
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

static void track_run_uart_i16(int16_t value)
{
    uint16_t magnitude;

    if(value < 0)
    {
        uart_sendbyte(UART0, '-');
        magnitude = (uint16_t) (-(value + 1)) + 1U;
    }
    else
    {
        uart_sendbyte(UART0, '+');
        magnitude = (uint16_t) value;
    }

    track_run_uart_u16(magnitude);
}

static void track_run_uart_bits(uint8_t value)
{
    uint8_t channel;

    for(channel = 0U; channel < 8U; channel++)
    {
        uart_sendbyte(UART0,
                      (value & (uint8_t) (1U << channel)) ? '1' : '0');
    }
}

static void track_run_show_oled(uint8_t running,
                                uint8_t estop,
                                uint8_t line_lost,
                                uint8_t pattern,
                                int16_t error,
                                int base_pwm,
                                int left_pwm,
                                int right_pwm)
{
    uint8_t channel;

    if(estop)
    {
        OLED_ShowString(1, 1, "TRACK E-STOP    ");
    }
    else if(line_lost)
    {
        OLED_ShowString(1, 1, "TRACK LINE LOST ");
    }
    else if(running)
    {
        OLED_ShowString(1, 1, "TRACK RUNNING   ");
    }
    else
    {
        OLED_ShowString(1, 1, "TRACK READY SW1 ");
    }

    OLED_ShowString(2, 1, "B:");
    for(channel = 0U; channel < 8U; channel++)
    {
        OLED_ShowChar(2, (uint8_t) (3U + channel),
                      (pattern & (uint8_t) (1U << channel)) ? '1' : '0');
    }
    OLED_ShowString(2, 12, "E:");
    OLED_ShowSignedNum(2, 14, error, 2);

    OLED_ShowString(3, 1, "BASE:");
    OLED_ShowNum(3, 6, (uint16_t) base_pwm, 4);
    OLED_ShowString(3, 11, "2+ 3-");

    OLED_ShowString(4, 1, "L:");
    OLED_ShowNum(4, 3, (uint16_t) left_pwm, 4);
    OLED_ShowString(4, 8, "R:");
    OLED_ShowNum(4, 10, (uint16_t) right_pwm, 4);
}

static void track_run_send_serial(uint8_t running,
                                  uint8_t estop,
                                  uint8_t line_lost,
                                  uint8_t pattern,
                                  int16_t error,
                                  int base_pwm,
                                  int left_pwm,
                                  int right_pwm)
{
    track_run_uart_puts("TRACK RUN=");
    uart_sendbyte(UART0, running ? '1' : '0');
    track_run_uart_puts(" ESTOP=");
    uart_sendbyte(UART0, estop ? '1' : '0');
    track_run_uart_puts(" LOST=");
    uart_sendbyte(UART0, line_lost ? '1' : '0');
    track_run_uart_puts(" B=");
    track_run_uart_bits(pattern);
    track_run_uart_puts(" E=");
    track_run_uart_i16(error);
    track_run_uart_puts(" BASE=");
    track_run_uart_u16((uint16_t) base_pwm);
    track_run_uart_puts(" L=");
    track_run_uart_u16((uint16_t) left_pwm);
    track_run_uart_puts(" R=");
    track_run_uart_u16((uint16_t) right_pwm);
    track_run_uart_puts("\r\n");
}

void track_run(void)
{
    track_run_button_t sw1;
    track_run_button_t sw2;
    track_run_button_t sw3;
    uint8_t running = 0U;
    uint8_t estop = 0U;
    uint8_t line_lost = 0U;
    uint8_t report_ticks = 0U;
    uint8_t lost_ticks = 0U;
    uint8_t pattern;
    int base_pwm = TRACK_RUN_BASE_START;
    int error = 0;
    int last_error = 0;
    int last_turn_direction = 0;
    int left_pwm = 0;
    int right_pwm = 0;

    motor_safe_init_disabled();
    key_init();
    xunji_init();
    gpio_init(TRACK_RUN_STATUS_LED_PORT, TRACK_RUN_STATUS_LED_PIN,
              TRACK_RUN_STATUS_LED_IOMUX, OUT);
    gpio_set(TRACK_RUN_STATUS_LED_PORT, TRACK_RUN_STATUS_LED_PIN, 0U);
    OLED_Init();
    if(OLED_GetDetectedAddress() == 0U)
    {
        track_run_oled_fault_loop();
    }
    OLED_Clear();
    uart0_init();

    track_run_button_init(&sw1, gpio_get(GPIOA, DL_GPIO_PIN_27));
    track_run_button_init(&sw2, gpio_get(GPIOA, DL_GPIO_PIN_17));
    track_run_button_init(&sw3, gpio_get(GPIOB, DL_GPIO_PIN_23));
    track_run_stop_motors();

    track_run_uart_puts(
        "TRACK RUN SAFE: SW1 start/stop, SW2 speed+, SW3 speed-, "
        "SW4 latched E-STOP\r\n");

    while(1)
    {
        uint8_t sw4_raw = gpio_get(GPIOB, DL_GPIO_PIN_27);

        if(sw4_raw == 0U)
        {
            estop = 1U;
            running = 0U;
        }

        track_run_button_update(&sw1, gpio_get(GPIOA, DL_GPIO_PIN_27));
        track_run_button_update(&sw2, gpio_get(GPIOA, DL_GPIO_PIN_17));
        track_run_button_update(&sw3, gpio_get(GPIOB, DL_GPIO_PIN_23));

        pattern = track_read_pattern();
        if(pattern != 0U)
        {
            error = track_error_from_pattern(pattern);
        }

        if(!estop && sw2.pressed_event)
        {
            base_pwm = track_run_clamp(base_pwm + TRACK_RUN_BASE_STEP,
                                       TRACK_RUN_BASE_MIN,
                                       TRACK_RUN_BASE_MAX);
        }
        if(!estop && sw3.pressed_event)
        {
            base_pwm = track_run_clamp(base_pwm - TRACK_RUN_BASE_STEP,
                                       TRACK_RUN_BASE_MIN,
                                       TRACK_RUN_BASE_MAX);
        }

        if(!estop && sw1.pressed_event)
        {
            if(running)
            {
                running = 0U;
            }
            else if(pattern != 0U)
            {
                running = 1U;
                line_lost = 0U;
                lost_ticks = 0U;
                error = track_error_from_pattern(pattern);
                last_error = error;
            }
        }

        left_pwm = 0;
        right_pwm = 0;

        if(running && !estop)
        {
            if(pattern != 0U)
            {
                int correction;

                lost_ticks = 0U;
                line_lost = 0U;
                error = track_error_from_pattern(pattern);

                if(error < -4)
                {
                    last_turn_direction = -1;
                }
                else if(error > 4)
                {
                    last_turn_direction = 1;
                }

                correction = TRACK_RUN_KP * error +
                             TRACK_RUN_KD * (error - last_error);
                last_error = error;

                left_pwm = track_run_clamp(base_pwm + correction,
                                           0, TRACK_RUN_PWM_LIMIT);
                right_pwm = track_run_clamp(base_pwm - correction,
                                            0, TRACK_RUN_PWM_LIMIT);
            }
            else
            {
                if(lost_ticks < 255U)
                {
                    lost_ticks++;
                }

                if(lost_ticks <= TRACK_RUN_LOST_SEARCH_TICKS &&
                   last_turn_direction != 0)
                {
                    if(last_turn_direction < 0)
                    {
                        left_pwm = 0;
                        right_pwm = TRACK_RUN_SEARCH_PWM;
                    }
                    else
                    {
                        left_pwm = TRACK_RUN_SEARCH_PWM;
                        right_pwm = 0;
                    }
                }

                if(lost_ticks >= TRACK_RUN_LOST_STOP_TICKS)
                {
                    line_lost = 1U;
                    running = 0U;
                    left_pwm = 0;
                    right_pwm = 0;
                }
            }
        }

        if(running && !estop)
        {
            Set_left_pwm(left_pwm);
            Set_right_pwm(right_pwm);
            motor_set_standby(1U);
        }
        else
        {
            track_run_stop_motors();
        }

        if(report_ticks == 0U)
        {
            track_run_show_oled(running, estop, line_lost, pattern,
                                (int16_t) error, base_pwm,
                                left_pwm, right_pwm);
            track_run_send_serial(running, estop, line_lost, pattern,
                                  (int16_t) error, base_pwm,
                                  left_pwm, right_pwm);
        }

        report_ticks++;
        if(report_ticks >= TRACK_RUN_REPORT_TICKS)
        {
            report_ticks = 0U;
        }

        delay_ms(TRACK_RUN_LOOP_MS);
    }
}
