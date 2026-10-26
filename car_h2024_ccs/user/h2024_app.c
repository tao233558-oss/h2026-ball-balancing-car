#include "headfile.h"
#include "h2024_app.h"

#define H2024_TASK_MIN                 1U
#define H2024_TASK_MAX                 4U
#define H2024_TASK1_TIMEOUT_TICKS   1500U
#define H2024_IMU_STALE_TICKS         50U
#define H2024_DISPLAY_PERIOD_TICKS    10U

typedef enum
{
    H2024_STATE_READY = 0,
    H2024_STATE_RUNNING,
    H2024_STATE_DONE,
    H2024_STATE_ABORTED,
    H2024_STATE_NO_IMU,
    H2024_STATE_IMU_LOST,
    H2024_STATE_TIMEOUT,
    H2024_STATE_LOCKED,
} h2024_state_t;

static volatile h2024_state_t g_h2024_state;
static volatile uint16_t g_h2024_elapsed_ticks;
static volatile uint8_t g_h2024_cue_pending;
static volatile uint8_t g_h2024_display_due;
static uint8_t g_h2024_selected_task;
static uint16_t g_h2024_last_imu_frame;
static uint16_t g_h2024_imu_stale_ticks;
static volatile int32_t g_h2024_left_total;
static volatile int32_t g_h2024_right_total;

static int32_t h2024_limit_encoder_display(int32_t value)
{
    if(value > 9999)
    {
        return 9999;
    }
    if(value < -9999)
    {
        return -9999;
    }
    return value;
}

static void h2024_stop_from_isr(h2024_state_t state)
{
    Set_left_pwm(0);
    Set_right_pwm(0);
    motor_set_standby(0U);
    set = 0;
    g_h2024_state = state;
    g_h2024_cue_pending = 1U;
    g_h2024_display_due = 1U;
}

static void h2024_start_selected_task(void)
{
    if(g_h2024_selected_task != 1U)
    {
        g_h2024_state = H2024_STATE_LOCKED;
        g_h2024_cue_pending = 1U;
        g_h2024_display_due = 1U;
        return;
    }

    if(imu_frame_count == 0U)
    {
        g_h2024_state = H2024_STATE_NO_IMU;
        g_h2024_cue_pending = 1U;
        g_h2024_display_due = 1U;
        return;
    }

    Set_left_pwm(0);
    Set_right_pwm(0);
    h_task_reset();
    g_h2024_elapsed_ticks = 0U;
    g_h2024_last_imu_frame = imu_frame_count;
    g_h2024_imu_stale_ticks = 0U;
    g_h2024_left_total = 0;
    g_h2024_right_total = 0;
    mode = 1;
    set = 1;
    motor_set_standby(1U);
    g_h2024_state = H2024_STATE_RUNNING;
    g_h2024_display_due = 1U;
}

static const char *h2024_state_text(h2024_state_t state)
{
    switch(state)
    {
        case H2024_STATE_READY:    return "READY           ";
        case H2024_STATE_RUNNING:  return "RUNNING         ";
        case H2024_STATE_DONE:     return "DONE            ";
        case H2024_STATE_ABORTED:  return "ABORTED         ";
        case H2024_STATE_NO_IMU:   return "WAIT IMU DATA   ";
        case H2024_STATE_IMU_LOST: return "IMU LOST        ";
        case H2024_STATE_TIMEOUT:  return "TIMEOUT STOP    ";
        case H2024_STATE_LOCKED:   return "TASK NOT READY  ";
        default:                   return "UNKNOWN         ";
    }
}

static void h2024_show(void)
{
    h2024_state_t state = g_h2024_state;
    uint16_t elapsed_ticks = g_h2024_elapsed_ticks;

    OLED_ShowString(1, 1, "H2024 TASK:     ");
    OLED_ShowNum(1, 12, g_h2024_selected_task, 1);
    OLED_ShowString(2, 1, (char *) h2024_state_text(state));
    OLED_ShowString(3, 1, "T:");
    OLED_ShowNum(3, 3, elapsed_ticks / 100U, 2);
    OLED_ShowString(3, 5, "s Y:");
    OLED_ShowSignedNum(3, 9, yaw_angle_int, 3);
    if(state == H2024_STATE_RUNNING)
    {
        OLED_ShowString(4, 1, "ANY KEY STOP   ");
    }
    else
    {
        OLED_ShowString(4, 1, "L:");
        OLED_ShowSignedNum(4, 3,
            h2024_limit_encoder_display(g_h2024_left_total), 4);
        OLED_ShowString(4, 8, " R:");
        OLED_ShowSignedNum(4, 11,
            h2024_limit_encoder_display(g_h2024_right_total), 4);
    }
}

void h2024_app_init(void)
{
    g_h2024_selected_task = H2024_TASK_MIN;
    g_h2024_state = H2024_STATE_READY;
    g_h2024_elapsed_ticks = 0U;
    g_h2024_cue_pending = 0U;
    g_h2024_display_due = 1U;
    g_h2024_last_imu_frame = 0U;
    g_h2024_imu_stale_ticks = 0U;
    g_h2024_left_total = 0;
    g_h2024_right_total = 0;
    mode = 1;
    set = 0;
    Set_left_pwm(0);
    Set_right_pwm(0);
    motor_set_standby(0U);
}

void h2024_app_service(void)
{
    uint8_t key = key_GetNum();

    if(g_h2024_cue_pending != 0U)
    {
        g_h2024_cue_pending = 0U;
        beep();
    }

    if(g_h2024_state == H2024_STATE_RUNNING)
    {
        if(key == KEY_MODE || key == KEY_START)
        {
            uint32_t primask = __get_PRIMASK();
            __disable_irq();
            h2024_stop_from_isr(H2024_STATE_ABORTED);
            if(primask == 0U)
            {
                __enable_irq();
            }
        }
    }
    else if(key == KEY_MODE)
    {
        g_h2024_selected_task++;
        if(g_h2024_selected_task > H2024_TASK_MAX)
        {
            g_h2024_selected_task = H2024_TASK_MIN;
        }
        g_h2024_state = (g_h2024_selected_task == 1U) ?
                        H2024_STATE_READY : H2024_STATE_LOCKED;
        g_h2024_elapsed_ticks = 0U;
        g_h2024_display_due = 1U;
    }
    else if(key == KEY_START)
    {
        h2024_start_selected_task();
    }

    if(g_h2024_display_due != 0U)
    {
        g_h2024_display_due = 0U;
        h2024_show();
    }
}

void h2024_tick_10ms(void)
{
    static uint8_t display_divider = 0U;
    int32_t encoder1_count;
    int32_t encoder2_count;
    uint16_t imu_frame;

    encoder_read_and_reset(&encoder1_count, &encoder2_count);
    left_encoder = (int) encoder_left_apply_sign(encoder1_count);
    right_encoder = (int) encoder_right_apply_sign(encoder2_count);

    if(g_h2024_state == H2024_STATE_RUNNING)
    {
        g_h2024_left_total += left_encoder;
        g_h2024_right_total += right_encoder;
    }

    display_divider++;
    if(display_divider >= H2024_DISPLAY_PERIOD_TICKS)
    {
        display_divider = 0U;
        g_h2024_display_due = 1U;
    }

    if(g_h2024_state != H2024_STATE_RUNNING)
    {
        return;
    }

    /* The two-button PCB has no dedicated E-stop. Either key stops at ISR rate. */
    if(gpio_get(GPIOA, DL_GPIO_PIN_27) == 0U ||
       gpio_get(GPIOA, DL_GPIO_PIN_17) == 0U ||
       gpio_get(GPIOB, DL_GPIO_PIN_23) == 0U ||
       gpio_get(GPIOB, DL_GPIO_PIN_27) == 0U)
    {
        h2024_stop_from_isr(H2024_STATE_ABORTED);
        return;
    }

    g_h2024_elapsed_ticks++;
    imu_frame = imu_frame_count;
    if(imu_frame != g_h2024_last_imu_frame)
    {
        g_h2024_last_imu_frame = imu_frame;
        g_h2024_imu_stale_ticks = 0U;
    }
    else
    {
        g_h2024_imu_stale_ticks++;
        if(g_h2024_imu_stale_ticks >= H2024_IMU_STALE_TICKS)
        {
            h2024_stop_from_isr(H2024_STATE_IMU_LOST);
            return;
        }
    }

    if(g_h2024_elapsed_ticks >= H2024_TASK1_TIMEOUT_TICKS)
    {
        h2024_stop_from_isr(H2024_STATE_TIMEOUT);
        return;
    }

    h_task_update_distance(g_h2024_left_total, g_h2024_right_total);
    h_req1_run();
    if(h_task_is_done() != 0U)
    {
        h2024_stop_from_isr(H2024_STATE_DONE);
    }
}
