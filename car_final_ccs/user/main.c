#include "headfile.h"
#include "_pid.h"
#include "key.h"
#include "track_diag.h"
#include "track_run.h"
#include "encoder_diag.h"
#include "motor_diag.h"
#include "dual_motor_diag.h"
#include "dual_speed_diag.h"
#include "e2025_drive_adapter.h"
#include "e2025_aim_adapter.h"
#include "k230_link.h"
#include "e2025_fault_manager.h"
#include "e2025_k230_command.h"
#include "e2025_tof_adapter.h"
#include "hardware_acceptance.h"
#include "tof200f.h"
#include "ir_adc_diag.h"
#include "h2026_b_uart_diag.h"
#include "h2026_b_stepper_diag.h"

int left_pwm, right_pwm, left_angle_pwm, right_angle_pwm;
int base_left_pwm, base_right_pwm, angle_pwm, angle_pwm1;
int cur_state, last_state, pre_state;
unsigned int Temp[2] = {0};
int turn_pwm, cnt;
uint8_t KeyNum;
int display_left_encoder, display_right_encoder;

#define STEPPER_DIR_PORT GPIOA
#define STEPPER_DIR_PIN  DL_GPIO_PIN_14
#define STEPPER_DIR_IOMUX PA14
#define STEPPER_PWM_PORT GPIOA
#define STEPPER_PWM_PIN  DL_GPIO_PIN_26
#define STEPPER_PWM_IOMUX PA26
#define TILT_STEPPER_DIR_PORT GPIOA
#define TILT_STEPPER_DIR_PIN  DL_GPIO_PIN_14
#define TILT_STEPPER_DIR_IOMUX PA14
#define TILT_STEPPER_PWM_PORT GPIOB
#define TILT_STEPPER_PWM_PIN  DL_GPIO_PIN_4
#define TILT_STEPPER_PWM_IOMUX PB4
#define DCC_AWAKE_PORT GPIOB
#define DCC_AWAKE_PIN  DL_GPIO_PIN_17
#define DCC_AWAKE_IOMUX PB17
#define AIM_DIAG_PULSES_PER_PRESS 200U
#define AIM_DIAG_SOFT_LIMIT_STEPS 2000
#define STANDALONE_DCC_AWAKE_PORT GPIOB
#define STANDALONE_DCC_AWAKE_PIN DL_GPIO_PIN_20
#define STANDALONE_DCC_AWAKE_IOMUX PB20
#define STANDALONE_KEY_CW_PORT GPIOB
#define STANDALONE_KEY_CW_PIN DL_GPIO_PIN_24
#define STANDALONE_KEY_CW_IOMUX PB24
#define STANDALONE_KEY_CCW_PORT GPIOA
#define STANDALONE_KEY_CCW_PIN DL_GPIO_PIN_24
#define STANDALONE_KEY_CCW_IOMUX PA24
#define STANDALONE_PULSES_PER_PRESS 400U
#define STANDALONE_SOFT_LIMIT_STEPS 20000
#define BOARD_LED_PORT GPIOB
#define BOARD_LED_PIN  DL_GPIO_PIN_22
#define BOARD_LED_IOMUX PB22

static e2025_drive_adapter_t g_e2025_drive_adapter;
static e2025_aim_adapter_t g_e2025_aim_adapter;
static e2025_fault_manager_t g_e2025_fault_manager;
static e2025_tof_mailbox_t g_e2025_tof_mailbox;
static e2025_tof_reader_t g_e2025_tof_reader;
static volatile uint8_t g_e2025_tof_poll_due;
static uint8_t g_e2025_tof_running;
#if E2025_ENABLE_K230_UART2
static volatile uint8_t g_e2025_k230_command_due;
static uint8_t g_e2025_k230_last_mode = 0xFFU;
#endif

static void debug_puts(const char *s)
{
    while(*s)
    {
        uart_sendbyte(UART0, (uint8_t) *s++);
    }
}

static void debug_put_u16(uint16_t value)
{
    char buf[6];
    uint8_t i = 0;

    if(value == 0)
    {
        uart_sendbyte(UART0, '0');
        return;
    }

    while(value > 0 && i < sizeof(buf))
    {
        buf[i++] = (char) ('0' + (value % 10));
        value /= 10;
    }

    while(i > 0)
    {
        uart_sendbyte(UART0, (uint8_t) buf[--i]);
    }
}

static void debug_put_i16(int16_t value)
{
    if(value < 0)
    {
        uart_sendbyte(UART0, '-');
        value = (int16_t) -value;
    }
    debug_put_u16((uint16_t) value);
}

static void tof_bringup_run(void)
{
    uint16_t distance = 0U;
    uint8_t range_status = 0U;
    tof200f_status_t status;

    gpio_init(BOARD_LED_PORT, BOARD_LED_PIN, BOARD_LED_IOMUX, OUT);
    uart0_init();
    OLED_Init();

    debug_puts("\r\nTOF200F I2C test start\r\n");
    debug_puts("Wiring: PB0=SCL, PB1=SDA, 3V3, GND; VL53L0X addr=0x29\r\n");
    debug_puts("If NO ACK: switch module to I2C first with UART command ");
    debug_puts("01 06 00 09 00 01 98 08\r\n");

    OLED_Clear();
    OLED_ShowString(1, 1, "TOF I2C TEST    ");
    OLED_ShowString(4, 1, "PB0 SCL PB1 SDA ");

    status = tof200f_init();
    while(status != TOF200F_OK)
    {
        debug_puts("TOF init failed: stage=");
        debug_put_i16((int16_t) status);
        debug_puts(" api=");
        debug_put_i16((int16_t) tof200f_last_api_error());
        debug_puts(" id=0x");
        debug_put_u16(tof200f_model_id());
        debug_puts("\r\n");

        OLED_ShowString(2, 1, "INIT ERROR      ");
        OLED_ShowString(3, 1, "CODE:           ");
        OLED_ShowSignedNum(3, 7, (int32_t) status, 2);
        gpio_toggle(BOARD_LED_PORT, BOARD_LED_PIN);
        delay_ms(1000U);
        status = tof200f_init();
    }

    debug_puts("VL53L0X init OK, model id=0xEEAA\r\n");
    OLED_ShowString(2, 1, "VL53L0X READY   ");

    while(1)
    {
        status = tof200f_read_mm(&distance, &range_status);
        if(status == TOF200F_OK)
        {
            debug_puts("TOF distance: ");
            debug_put_u16(distance);
            debug_puts(" mm, range_status=");
            debug_put_u16(range_status);
            debug_puts("\r\n");

            OLED_ShowString(3, 1, "DIST:      mm   ");
            OLED_ShowNum(3, 6, distance, 4);
            gpio_set(BOARD_LED_PORT, BOARD_LED_PIN,
                     (range_status == 0U) ? 1U : 0U);
        }
        else
        {
            debug_puts("TOF measurement failed: api=");
            debug_put_i16((int16_t) tof200f_last_api_error());
            debug_puts("\r\n");
            OLED_ShowString(3, 1, "MEASURE ERROR   ");
            gpio_set(BOARD_LED_PORT, BOARD_LED_PIN, 0);
        }
        delay_ms(200U);
    }
}

static void e2025_tof_runtime_init(void)
{
    e2025_tof_mailbox_init(&g_e2025_tof_mailbox);
    e2025_tof_reader_init(&g_e2025_tof_reader);
    g_e2025_tof_poll_due = 0U;
    g_e2025_tof_running = 0U;

#if E2025_ENABLE_TOF200F
    {
        tof200f_status_t status = tof200f_init();

        if(status == TOF200F_OK)
        {
            status = tof200f_start_continuous();
        }
        if(status == TOF200F_OK)
        {
            g_e2025_tof_running = 1U;
            debug_puts("E2025 TOF continuous ranging ready\r\n");
        }
        else
        {
            e2025_tof_publish_error(&g_e2025_tof_mailbox,
                                    E2025_TOF_STATUS_UNAVAILABLE);
            debug_puts("E2025 TOF unavailable: stage=");
            debug_put_i16((int16_t) status);
            debug_puts(" api=");
            debug_put_i16((int16_t) tof200f_last_api_error());
            debug_puts("\r\n");
        }
    }
#endif
}

static void e2025_tof_runtime_service(void)
{
#if E2025_ENABLE_TOF200F
    uint16_t distance_mm;
    uint8_t range_status;
    tof200f_status_t status;

    if((g_e2025_tof_poll_due == 0U) || (g_e2025_tof_running == 0U))
    {
        return;
    }
    g_e2025_tof_poll_due = 0U;

    status = tof200f_poll_mm(&distance_mm, &range_status);
    if(status == TOF200F_OK)
    {
        e2025_tof_publish_sample(&g_e2025_tof_mailbox,
                                 distance_mm, range_status);
    }
    else if(status != TOF200F_NOT_READY)
    {
        (void) tof200f_stop_continuous();
        g_e2025_tof_running = 0U;
        e2025_tof_publish_error(&g_e2025_tof_mailbox,
                                E2025_TOF_STATUS_IO_ERROR);
        debug_puts("E2025 TOF polling stopped: api=");
        debug_put_i16((int16_t) tof200f_last_api_error());
        debug_puts("\r\n");
    }
#endif
}

#if E2025_ENABLE_K230_UART2
static void e2025_k230_send_text(const char *text)
{
    while(*text != '\0')
    {
        uart_sendbyte(UART2, (uint8_t) *text++);
    }
}

static void e2025_k230_command_service(void)
{
    e2025_status_t status;
    e2025_k230_mode_t mode;

    e2025_get_status(&status);
    mode = e2025_k230_select_mode(&status);
    if(((uint8_t) mode != g_e2025_k230_last_mode) ||
       (g_e2025_k230_command_due != 0U))
    {
        g_e2025_k230_command_due = 0U;
        g_e2025_k230_last_mode = (uint8_t) mode;
        e2025_k230_send_text(e2025_k230_command_text(mode));
    }
}
#endif

static void stepper_init(void)
{
    gpio_init(STEPPER_DIR_PORT, STEPPER_DIR_PIN, STEPPER_DIR_IOMUX, OUT);
    gpio_init(STEPPER_PWM_PORT, STEPPER_PWM_PIN, STEPPER_PWM_IOMUX, OUT);
    gpio_set(STEPPER_DIR_PORT, STEPPER_DIR_PIN, 0);
    gpio_set(STEPPER_PWM_PORT, STEPPER_PWM_PIN, 0);
}

static void stepper_pulse(uint16_t steps, uint8_t dir)
{
    gpio_set(STEPPER_DIR_PORT, STEPPER_DIR_PIN, dir);
    delay_ms(10);
    for(uint16_t i = 0; i < steps; i++)
    {
        gpio_set(STEPPER_PWM_PORT, STEPPER_PWM_PIN, 1);
        delay_us(5000);
        gpio_set(STEPPER_PWM_PORT, STEPPER_PWM_PIN, 0);
        delay_us(5000);
    }
}

static void stepper_test_run(void)
{
    Set_left_pwm(0);
    Set_right_pwm(0);
    stepper_pulse(800, 0);
    delay_ms(1000);
    stepper_pulse(800, 1);
    delay_ms(1000);
}

static void aim_diag_uart_puts(const char *text)
{
    while(*text != '\0')
    {
        uart_sendbyte(UART0, (uint8_t) *text++);
    }
}

static void aim_diag_uart_i32(int32_t value)
{
    char buffer[10];
    uint8_t length = 0U;
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

    if(magnitude == 0U)
    {
        uart_sendbyte(UART0, '0');
        return;
    }
    while(magnitude > 0U && length < sizeof(buffer))
    {
        buffer[length++] = (char) ('0' + (magnitude % 10U));
        magnitude /= 10U;
    }
    while(length > 0U)
    {
        uart_sendbyte(UART0, (uint8_t) buffer[--length]);
    }
}

static void aim_diag_all_outputs_low(void)
{
    gpio_set(DCC_AWAKE_PORT, DCC_AWAKE_PIN, 0U);
    gpio_set(STEPPER_PWM_PORT, STEPPER_PWM_PIN, 0U);
    gpio_set(TILT_STEPPER_PWM_PORT, TILT_STEPPER_PWM_PIN, 0U);
    gpio_set(STEPPER_DIR_PORT, STEPPER_DIR_PIN, 0U);
    gpio_set(TILT_STEPPER_DIR_PORT, TILT_STEPPER_DIR_PIN, 0U);
}

static void aim_diag_sleep_outputs(void)
{
    gpio_set(DCC_AWAKE_PORT, DCC_AWAKE_PIN, 0U);
    gpio_set(STEPPER_PWM_PORT, STEPPER_PWM_PIN, 0U);
    gpio_set(TILT_STEPPER_PWM_PORT, TILT_STEPPER_PWM_PIN, 0U);
}

static void aim_diag_gpio_init(void)
{
    /* SLP is initialized first with a hardware pulldown on the PCB. */
    gpio_init(DCC_AWAKE_PORT, DCC_AWAKE_PIN, DCC_AWAKE_IOMUX, OUT);
    gpio_init(STEPPER_PWM_PORT, STEPPER_PWM_PIN, STEPPER_PWM_IOMUX, OUT);
    gpio_init(STEPPER_DIR_PORT, STEPPER_DIR_PIN, STEPPER_DIR_IOMUX, OUT);
    gpio_init(TILT_STEPPER_PWM_PORT, TILT_STEPPER_PWM_PIN,
              TILT_STEPPER_PWM_IOMUX, OUT);
    gpio_init(TILT_STEPPER_DIR_PORT, TILT_STEPPER_DIR_PIN,
              TILT_STEPPER_DIR_IOMUX, OUT);
    aim_diag_all_outputs_low();
}

static uint8_t aim_diag_all_buttons_released(void)
{
    return (gpio_get(GPIOA, DL_GPIO_PIN_27) != 0U &&
            gpio_get(GPIOA, DL_GPIO_PIN_17) != 0U &&
            gpio_get(GPIOB, DL_GPIO_PIN_23) != 0U &&
            gpio_get(GPIOB, DL_GPIO_PIN_27) != 0U) ? 1U : 0U;
}

static void aim_diag_wait_for_safe_release(void)
{
    uint8_t release_ticks = 0U;

    OLED_ShowString(1, 1, "AIM TEST SAFE   ");
    OLED_ShowString(2, 1, "RELEASE KEYS    ");
    OLED_ShowString(3, 1, "DCC SLEEP       ");
    OLED_ShowString(4, 1, "LASER OFF       ");
    while(release_ticks < 50U)
    {
        aim_diag_all_outputs_low();
        if(aim_diag_all_buttons_released())
        {
            release_ticks++;
        }
        else
        {
            release_ticks = 0U;
        }
        delay_ms(10U);
    }
}

static void aim_diag_pulse_axis(uint8_t select_tilt, uint8_t reverse)
{
    GPIO_Regs *step_port = select_tilt ? TILT_STEPPER_PWM_PORT :
                                        STEPPER_PWM_PORT;
    uint32_t step_pin = select_tilt ? TILT_STEPPER_PWM_PIN :
                                      STEPPER_PWM_PIN;
    GPIO_Regs *dir_port = select_tilt ? TILT_STEPPER_DIR_PORT :
                                       STEPPER_DIR_PORT;
    uint32_t dir_pin = select_tilt ? TILT_STEPPER_DIR_PIN :
                                     STEPPER_DIR_PIN;

    gpio_set(step_port, step_pin, 0U);
    gpio_set(dir_port, dir_pin, reverse ? 1U : 0U);
    gpio_set(DCC_AWAKE_PORT, DCC_AWAKE_PIN, 1U);
    delay_ms(2U);
    delay_ms(1U); /* conservative DIR setup after the driver wakes */
    for(uint8_t pulse = 0U; pulse < AIM_DIAG_PULSES_PER_PRESS; pulse++)
    {
        gpio_set(step_port, step_pin, 1U);
        delay_ms(5U);
        gpio_set(step_port, step_pin, 0U);
        delay_ms(5U);
    }
    gpio_set(DCC_AWAKE_PORT, DCC_AWAKE_PIN, 0U);
}

static void aim_diag_show(uint8_t select_tilt,
                          int32_t pan_position,
                          int32_t tilt_position)
{
    OLED_ShowString(1, 1, "AIM TEST SAFE   ");
    OLED_ShowString(2, 1, select_tilt ? "SEL:TILT SLEEP  " :
                                       "SEL:PAN  SLEEP  ");
    OLED_ShowString(3, 1, "P:");
    OLED_ShowSignedNum(3, 3, pan_position, 4);
    OLED_ShowString(3, 9, "T:");
    OLED_ShowSignedNum(3, 11, tilt_position, 4);
    OLED_ShowString(4, 1, "S3:+200 S4:-200 ");
}

static void aim_diag_run(void)
{
    uint8_t select_tilt = 0U;
    int32_t pan_position = 0;
    int32_t tilt_position = 0;

    aim_diag_gpio_init();
    key_init();
    OLED_Init();
    OLED_Clear();
    uart0_init();
    aim_diag_wait_for_safe_release();
    aim_diag_show(select_tilt, pan_position, tilt_position);
    aim_diag_uart_puts(
        "AIM DIAG SAFE: PB4/PA14 TILT test, PB17 SLP; laser off\r\n");
    aim_diag_uart_puts(
        "SW1 PAN, SW2 TILT, SW3 +200 pulses, SW4 -200 pulses\r\n");

    while(1)
    {
        uint8_t key = key_GetNum();
        int32_t *position;

        if(key == KEY_SW1)
        {
            select_tilt = 0U;
        }
        else if(key == KEY_SW2)
        {
            select_tilt = 1U;
        }

        position = select_tilt ? &tilt_position : &pan_position;
        if(key == KEY_SW3 &&
                *position <= AIM_DIAG_SOFT_LIMIT_STEPS -
                             (int32_t) AIM_DIAG_PULSES_PER_PRESS)
        {
            aim_diag_pulse_axis(select_tilt, 0U);
            *position += (int32_t) AIM_DIAG_PULSES_PER_PRESS;
        }
        else if(key == KEY_SW4 &&
                *position >= -AIM_DIAG_SOFT_LIMIT_STEPS +
                             (int32_t) AIM_DIAG_PULSES_PER_PRESS)
        {
            aim_diag_pulse_axis(select_tilt, 1U);
            *position -= (int32_t) AIM_DIAG_PULSES_PER_PRESS;
        }

        if(key != KEY_NONE)
        {
            /*
             * Keep the last DIR level latched for static multimeter checks.
             * STEP remains low and PB17 keeps both driver channels asleep.
             */
            aim_diag_sleep_outputs();
            aim_diag_show(select_tilt, pan_position, tilt_position);
            aim_diag_uart_puts(select_tilt ? "AIM AXIS=TILT POS=" :
                                             "AIM AXIS=PAN POS=");
            aim_diag_uart_i32(*position);
            aim_diag_uart_puts(" SLEEP=1\r\n");
        }
        delay_ms(10U);
    }
}

static void standalone_stepper_safe_outputs(void)
{
    gpio_set(STANDALONE_DCC_AWAKE_PORT, STANDALONE_DCC_AWAKE_PIN, 0U);
    gpio_set(STEPPER_PWM_PORT, STEPPER_PWM_PIN, 0U);
    gpio_set(STEPPER_DIR_PORT, STEPPER_DIR_PIN, 0U);
}

static void standalone_stepper_gpio_init(void)
{
    gpio_init(STANDALONE_DCC_AWAKE_PORT, STANDALONE_DCC_AWAKE_PIN,
              STANDALONE_DCC_AWAKE_IOMUX, OUT);
    gpio_init(STEPPER_PWM_PORT, STEPPER_PWM_PIN, STEPPER_PWM_IOMUX, OUT);
    gpio_init(STEPPER_DIR_PORT, STEPPER_DIR_PIN, STEPPER_DIR_IOMUX, OUT);
    gpio_init(STANDALONE_KEY_CW_PORT, STANDALONE_KEY_CW_PIN,
              STANDALONE_KEY_CW_IOMUX, IN_UP);
    gpio_init(STANDALONE_KEY_CCW_PORT, STANDALONE_KEY_CCW_PIN,
              STANDALONE_KEY_CCW_IOMUX, IN_UP);
    standalone_stepper_safe_outputs();
}

static uint8_t standalone_stepper_read_key(void)
{
    uint8_t key = 0U;

    if(gpio_get(STANDALONE_KEY_CW_PORT, STANDALONE_KEY_CW_PIN) == 0U)
    {
        delay_ms(20U);
        if(gpio_get(STANDALONE_KEY_CW_PORT, STANDALONE_KEY_CW_PIN) == 0U)
        {
            key = 1U;
            while(gpio_get(STANDALONE_KEY_CW_PORT,
                           STANDALONE_KEY_CW_PIN) == 0U)
            {
                standalone_stepper_safe_outputs();
                delay_ms(1U);
            }
        }
    }
    else if(gpio_get(STANDALONE_KEY_CCW_PORT,
                     STANDALONE_KEY_CCW_PIN) == 0U)
    {
        delay_ms(20U);
        if(gpio_get(STANDALONE_KEY_CCW_PORT,
                    STANDALONE_KEY_CCW_PIN) == 0U)
        {
            key = 2U;
            while(gpio_get(STANDALONE_KEY_CCW_PORT,
                           STANDALONE_KEY_CCW_PIN) == 0U)
            {
                standalone_stepper_safe_outputs();
                delay_ms(1U);
            }
        }
    }

    if(key != 0U)
    {
        delay_ms(20U);
    }
    return key;
}

static void standalone_stepper_show(int32_t position, const char *state)
{
    OLED_ShowString(1, 1, "STEPPER TEST    ");
    OLED_ShowString(2, 1, "K1:CW K2:CCW   ");
    OLED_ShowString(3, 1, "POS:");
    OLED_ShowSignedNum(3, 5, position, 5);
    OLED_ShowString(4, 1, "                ");
    OLED_ShowString(4, 1, (char *) state);
}

static void standalone_stepper_pulse(uint8_t reverse)
{
    gpio_set(STANDALONE_DCC_AWAKE_PORT, STANDALONE_DCC_AWAKE_PIN, 0U);
    gpio_set(STEPPER_PWM_PORT, STEPPER_PWM_PIN, 0U);
    gpio_set(STEPPER_DIR_PORT, STEPPER_DIR_PIN, reverse ? 1U : 0U);
    delay_ms(10U);
    gpio_set(STANDALONE_DCC_AWAKE_PORT, STANDALONE_DCC_AWAKE_PIN, 1U);
    delay_ms(10U);

    for(uint16_t pulse = 0U;
        pulse < STANDALONE_PULSES_PER_PRESS;
        pulse++)
    {
        gpio_set(STEPPER_PWM_PORT, STEPPER_PWM_PIN, 1U);
        delay_ms(2U);
        gpio_set(STEPPER_PWM_PORT, STEPPER_PWM_PIN, 0U);
        delay_ms(2U);
    }

    gpio_set(STANDALONE_DCC_AWAKE_PORT, STANDALONE_DCC_AWAKE_PIN, 0U);
    gpio_set(STEPPER_PWM_PORT, STEPPER_PWM_PIN, 0U);
}

static void standalone_stepper_run(void)
{
    int32_t position = 0;
    uint8_t released_ticks = 0U;

    standalone_stepper_gpio_init();
    OLED_Init();
    OLED_Clear();
    standalone_stepper_show(position, "RELEASE KEYS");

    while(released_ticks < 50U)
    {
        standalone_stepper_safe_outputs();
        if(gpio_get(STANDALONE_KEY_CW_PORT,
                    STANDALONE_KEY_CW_PIN) != 0U &&
           gpio_get(STANDALONE_KEY_CCW_PORT,
                    STANDALONE_KEY_CCW_PIN) != 0U)
        {
            released_ticks++;
        }
        else
        {
            released_ticks = 0U;
        }
        delay_ms(10U);
    }

    standalone_stepper_show(position, "DCC:SLEEP");
    while(1)
    {
        uint8_t key = standalone_stepper_read_key();

        if(key == 1U &&
           position <= STANDALONE_SOFT_LIMIT_STEPS -
                       (int32_t) STANDALONE_PULSES_PER_PRESS)
        {
            standalone_stepper_show(position, "RUN:CW");
            standalone_stepper_pulse(0U);
            position += (int32_t) STANDALONE_PULSES_PER_PRESS;
            standalone_stepper_show(position, "DCC:SLEEP");
        }
        else if(key == 2U &&
                position >= -STANDALONE_SOFT_LIMIT_STEPS +
                            (int32_t) STANDALONE_PULSES_PER_PRESS)
        {
            standalone_stepper_show(position, "RUN:CCW");
            standalone_stepper_pulse(1U);
            position -= (int32_t) STANDALONE_PULSES_PER_PRESS;
            standalone_stepper_show(position, "DCC:SLEEP");
        }

        delay_ms(10U);
    }
}

static uint8_t e2025_corner_event_from_pattern(uint8_t pattern)
{
    static track_corner_filter_t corner_filter = {1U, 0U, 0U};
    return track_corner_filter_step(&corner_filter, pattern);
}

static uint8_t e2025_status_was_tracking(const e2025_status_t *status)
{
    if(status->state != E2025_STATE_RUNNING)
    {
        return 0U;
    }
    return (status->task == E2025_TASK_TRACK_N_LAPS ||
            status->task == E2025_TASK_MOVE_AIM_1_LAP ||
            status->task == E2025_TASK_MOVE_AIM_2_LAPS ||
            status->task == E2025_TASK_MOVE_DRAW_CIRCLE) ? 1U : 0U;
}

static uint8_t e2025_status_was_aiming(const e2025_status_t *status)
{
    if(status->state != E2025_STATE_RUNNING)
    {
        return 0U;
    }
    return (status->task == E2025_TASK_AIM_2S ||
            status->task == E2025_TASK_AIM_SEARCH_4S ||
            status->task == E2025_TASK_MOVE_AIM_1_LAP ||
            status->task == E2025_TASK_MOVE_AIM_2_LAPS ||
            status->task == E2025_TASK_MOVE_DRAW_CIRCLE) ? 1U : 0U;
}

static void e2025_make_hardware_input(e2025_input_t *input)
{
    static uint8_t last_vision_sequence = 0;
    static uint8_t have_vision_sequence = 0;
    aim_vision_sample_t sample;
    uint8_t pattern = track_read_pattern();

    memset(input, 0, sizeof(*input));
    input->track_pattern = pattern;
    input->corner_event = e2025_corner_event_from_pattern(pattern);
    /* SW4 is also the menu decrement key, but becomes an immediate stop while running. */
    input->emergency_stop =
        (gpio_get(GPIOB, DL_GPIO_PIN_27) == 0U) ? 1U : 0U;

    if(k230_get_vision_sample(&sample))
    {
        input->target_valid = (sample.flags & AIM_FLAG_TARGET_VALID) ? 1U : 0U;
        input->spot_valid = (sample.flags & AIM_FLAG_SPOT_VALID) ? 1U : 0U;
        input->circle_complete =
            (sample.flags & AIM_FLAG_CIRCLE_COMPLETE) ? 1U : 0U;
        input->target_x = sample.target_x;
        input->target_y = sample.target_y;
        input->spot_x = sample.spot_x;
        input->spot_y = sample.spot_y;

        if(!have_vision_sequence || sample.sequence != last_vision_sequence)
        {
            input->vision_fresh = 1;
            last_vision_sequence = sample.sequence;
            have_vision_sequence = 1;
        }
    }
    else
    {
        /* After a link timeout, accept the first frame after reconnection even
         * if the K230 restarted with the same sequence number. */
        have_vision_sequence = 0U;
    }
}

static dual_motor_diag_fault_t e2025_apply_cascade_output(
    const e2025_output_t *output,
    const e2025_input_t *input,
    uint8_t track_was_active)
{
    static uint8_t was_running = 0U;
    e2025_drive_output_t drive;
    uint8_t track_request = output->track_enable;

    /* The app clears track_enable while handling SW4; retain the pre-tick state
     * so the adapter sees and permanently latches an in-motion emergency stop. */
    if(input->emergency_stop && track_was_active)
    {
        track_request = 1U;
    }

    e2025_drive_adapter_tick(
        &g_e2025_drive_adapter,
        track_request,
        input->emergency_stop,
        input->track_pattern,
        (int32_t) track_error_from_pattern(input->track_pattern),
        (int32_t) left_encoder,
        (int32_t) right_encoder,
        &drive);

    if(!drive.active || drive.fault != DUAL_MOTOR_FAULT_NONE)
    {
        motor_set_standby(0U);
        Set_left_pwm(0);
        Set_right_pwm(0);
        was_running = 0U;
        return drive.fault;
    }

    Set_left_pwm((int) drive.left_pwm);
    Set_right_pwm((int) drive.right_pwm);
    if(!was_running)
    {
        /* PWM/direction are established before the driver leaves standby. */
        motor_set_standby(1U);
        was_running = 1U;
    }
    return drive.fault;
}

static e2025_aim_fault_t e2025_apply_aim_adapter_output(
    const e2025_output_t *output,
    const e2025_input_t *input,
    uint8_t aim_was_active)
{
    e2025_aim_output_t aim;
    uint8_t aim_request = output->aim_enable;

    /* Preserve the pre-tick state so SW4 is latched even though the app has
     * already cleared aim_enable while aborting the task. */
    if(input->emergency_stop && aim_was_active)
    {
        aim_request = 1U;
    }

    e2025_aim_adapter_tick(
        &g_e2025_aim_adapter,
        aim_request,
        input->emergency_stop,
        input->vision_fresh,
        input->target_valid,
        input->spot_valid,
        output->laser_enable,
        output->pan_command,
        output->tilt_command,
        &aim);

    /* aim.pan_steps, aim.tilt_steps and aim.laser_allowed intentionally stop
     * here. The final two-axis pins, limit switches and laser polarity are not
     * confirmed, so this adapter must not touch GPIO. */
    return aim.fault;
}

static void e2025_apply_output(const e2025_output_t *output,
                               const e2025_input_t *input,
                               uint8_t track_was_active,
                               uint8_t aim_was_active,
                               dual_motor_diag_fault_t *drive_fault,
                               e2025_aim_fault_t *aim_fault)
{
    *drive_fault = DUAL_MOTOR_FAULT_NONE;
    *aim_fault = E2025_AIM_FAULT_NONE;
#if E2025_ENABLE_AIM_ADAPTER
    *aim_fault = e2025_apply_aim_adapter_output(output, input, aim_was_active);
#else
    (void) aim_was_active;
#endif

#if E2025_OFFLINE_SIMULATION
    (void) output;
    (void) input;
    (void) track_was_active;
    motor_set_standby(0U);
    Set_left_pwm(0);
    Set_right_pwm(0);
#else
#if E2025_ENABLE_CASCADE_TRACKING
    *drive_fault = e2025_apply_cascade_output(output, input, track_was_active);
#else
    (void) input;
    (void) track_was_active;
    if(output->track_enable)
    {
        track1();
    }
    else
    {
        Set_left_pwm(0);
        Set_right_pwm(0);
    }
#endif

    /* Physical pan/tilt/laser output remains disabled until its pins, limit
     * switches, driver timing and active polarity are confirmed on the PCB. */
#endif
}

static void e2025_show_status(void)
{
    e2025_status_t status;

    e2025_get_status(&status);
#if E2025_OFFLINE_SIMULATION
    OLED_ShowString(1, 1, "E2025 SIM SAFE ");
#else
    OLED_ShowString(1, 1, "E2025 HARDWARE ");
#endif
    OLED_ShowString(2, 1, "                ");
    OLED_ShowString(2, 1, (char *) e2025_task_name(status.task));
    OLED_ShowString(3, 1, "L:");
    OLED_ShowNum(3, 3, status.completed_laps, 1);
    OLED_ShowString(3, 4, "/");
    OLED_ShowNum(3, 5, status.requested_laps, 1);
    OLED_ShowString(3, 7, "T:");
    OLED_ShowNum(3, 9, status.elapsed_ticks / 100U, 2);
    OLED_ShowString(4, 1, "                ");
    OLED_ShowString(4, 1, (char *) e2025_state_name(status.state));
    if(status.fault != E2025_FAULT_NONE)
    {
        OLED_ShowString(4, 7, (char *) e2025_fault_name(status.fault));
    }
    else if(status.state == E2025_STATE_RUNNING &&
            status.task != E2025_TASK_TRACK_N_LAPS)
    {
        OLED_ShowString(4, 9, status.aim_locked ? "LOCK" : "SEEK");
    }
}

int main(void)
{
    system_init();
    delay_ms(500);

#if APP_PROFILE_LED_TEST
    gpio_init(BOARD_LED_PORT, BOARD_LED_PIN, BOARD_LED_IOMUX, OUT);
    while(1)
    {
        gpio_set(BOARD_LED_PORT, BOARD_LED_PIN, 1);
        delay_ms(500);
        gpio_set(BOARD_LED_PORT, BOARD_LED_PIN, 0);
        delay_ms(500);
    }
#endif

#if APP_PROFILE_TOF_TEST
    tof_bringup_run();
#endif

#if APP_PROFILE_TRACK_DIAG
    track_diag_run();
#endif

#if APP_PROFILE_TRACK_RUN
    track_run();
#endif

#if APP_PROFILE_ENCODER_DIAG
    encoder_diag_run();
#endif

#if APP_PROFILE_MOTOR_DIAG
    motor_diag_run();
#endif

#if APP_PROFILE_DUAL_MOTOR_DIAG
    dual_motor_diag_run();
#endif

#if APP_PROFILE_DUAL_SPEED_DIAG
    dual_speed_diag_run();
#endif

#if APP_PROFILE_AIM_DIAG
    aim_diag_run();
#endif

#if APP_PROFILE_STEPPER_STANDALONE
    standalone_stepper_run();
#endif

#if APP_PROFILE_IR_ADC_DIAG
    ir_adc_diag_run();
#endif

#if APP_PROFILE_H2026_B_UART_DIAG
    h2026_b_uart_diag_run();
#endif

#if APP_PROFILE_H2026_B_STEPPER_DIAG
    h2026_b_stepper_diag_run();
#endif

#if APP_PROFILE_HW_ACCEPT
    hardware_acceptance_run();
#endif

#if APP_PROFILE_E2025
    OLED_Init();
    imu_init();
    key_init();

#if E2025_OFFLINE_SIMULATION || E2025_ENABLE_CASCADE_TRACKING
    motor_safe_init_disabled();
#else
    motor_init();
#endif
    encoder_init();
    k230_link_init();
    uart0_init();
#if E2025_ENABLE_K230_UART2
    uart_init(UART2, 115200, 1);
    g_e2025_k230_command_due = 1U;
#endif
    xunji_init();
    buzzer_light_init();
    e2025_app_init();
    e2025_drive_adapter_init(&g_e2025_drive_adapter);
    e2025_aim_adapter_init(&g_e2025_aim_adapter);
    e2025_fault_manager_init(&g_e2025_fault_manager);
    e2025_tof_runtime_init();
    tim_interrupt_ms_init(TIMG8, 10, 1);

    pid_init(&motorA, DELTA_PID, 10, 10, 5);
    pid_init(&motorB, DELTA_PID, 10, 10, 5);
    pid_init(&angle, POSITION_PID, 0.04117, 0, 0.102);
    menu_init();

    while(1)
    {
        e2025_tof_runtime_service();
#if E2025_ENABLE_K230_UART2
        e2025_k230_command_service();
#endif

        if(imu_flag)
        {
            imu_flag = 0;
            imu_analysis();
        }

        KeyNum = key_GetNum();
        menu_handle_key(KeyNum);
        menu_update();

        if(menu_is_running())
        {
            e2025_show_status();
        }
        else
        {
            menu_show();
        }
    }
#endif
}

void TIMG8_IRQHandler()
{
    if(DL_TimerG_getPendingInterrupt(TIMG8) == DL_TIMER_IIDX_LOAD)
    {
#if APP_PROFILE_E2025
        e2025_status_t status;
        e2025_input_t input;
        e2025_output_t output;
        uint8_t track_was_active;
        uint8_t aim_was_active;
        uint8_t k230_connected;
        dual_motor_diag_fault_t drive_fault;
        e2025_aim_fault_t aim_fault;
        e2025_fault_t app_fault;

#if E2025_ENABLE_TOF200F
        static uint8_t tof_poll_divider = 0U;

        tof_poll_divider++;
        if(tof_poll_divider >= 5U)
        {
            tof_poll_divider = 0U;
            g_e2025_tof_poll_due = 1U;
        }
#endif
#if E2025_ENABLE_K230_UART2
        static uint8_t k230_command_divider = 0U;

        k230_command_divider++;
        if(k230_command_divider >= 100U)
        {
            k230_command_divider = 0U;
            g_e2025_k230_command_due = 1U;
        }
#endif

        k230_link_tick_10ms();

#if !E2025_OFFLINE_SIMULATION
        int32_t encoder1_count;
        int32_t encoder2_count;
        encoder_read_and_reset(&encoder1_count, &encoder2_count);
        left_encoder = (int) encoder_left_apply_sign(encoder1_count);
        right_encoder = (int) encoder_right_apply_sign(encoder2_count);
        display_left_encoder += left_encoder;
        display_right_encoder += right_encoder;
#endif

        e2025_get_status(&status);
        track_was_active = e2025_status_was_tracking(&status);
        aim_was_active = e2025_status_was_aiming(&status);
#if E2025_OFFLINE_SIMULATION
        e2025_sim_make_input(&status, &input);
#else
        e2025_make_hardware_input(&input);
#endif
#if E2025_ENABLE_TOF200F
        e2025_tof_snapshot_tick(&g_e2025_tof_mailbox,
                                &g_e2025_tof_reader, &input);
#endif
        e2025_tick_10ms(&input);
        e2025_get_output(&output);
        e2025_apply_output(&output, &input, track_was_active, aim_was_active,
                           &drive_fault, &aim_fault);

#if E2025_OFFLINE_SIMULATION
        k230_connected = 1U;
#else
        {
            k230_link_status_t link_status;
            (void) k230_link_get_snapshot(0, &link_status);
            k230_connected = link_status.connected;
        }
#endif
        e2025_get_status(&status);
        app_fault = e2025_fault_manager_step(
            &g_e2025_fault_manager,
            &status,
            &input,
            drive_fault,
            aim_fault,
            k230_connected);
        if(app_fault != E2025_FAULT_NONE)
        {
            e2025_request_fault(app_fault);
        }
#endif
    }
}
