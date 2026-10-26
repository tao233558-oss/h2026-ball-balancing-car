#include "headfile.h"
#include "v2026_nav_app.h"
#include "v2026_nav_link.h"

typedef enum
{
    V2026_STATE_WAIT_LINK = 0,
    V2026_STATE_SAFE_READY,
    V2026_STATE_COMMAND_DISABLED,
    V2026_STATE_ESTOP,
    V2026_STATE_LINK_LOST,
} v2026_state_t;

static volatile v2026_state_t g_state;
static volatile uint8_t g_display_due;
static volatile uint8_t g_local_estop_latched;
static volatile int32_t g_left_total;
static volatile int32_t g_right_total;
static v2026_nav_command_t g_last_command;
static v2026_nav_link_status_t g_last_link;

static const char *v2026_state_text(v2026_state_t state)
{
    switch(state)
    {
        case V2026_STATE_WAIT_LINK:        return "WAIT VISION     ";
        case V2026_STATE_SAFE_READY:       return "SAFE READY      ";
        case V2026_STATE_COMMAND_DISABLED: return "CMD DISABLED    ";
        case V2026_STATE_ESTOP:            return "REMOTE ESTOP    ";
        case V2026_STATE_LINK_LOST:        return "LINK LOST       ";
        default:                           return "UNKNOWN         ";
    }
}

void v2026_nav_app_init(void)
{
    g_state = V2026_STATE_WAIT_LINK;
    g_display_due = 1U;
    g_local_estop_latched = 0U;
    g_left_total = 0;
    g_right_total = 0;
    memset(&g_last_command, 0, sizeof(g_last_command));
    memset(&g_last_link, 0, sizeof(g_last_link));
    Set_left_pwm(0);
    Set_right_pwm(0);
    motor_set_standby(0U);
}

void v2026_nav_app_service(void)
{
    if(key_GetNum() != KEY_NONE)
    {
        Set_left_pwm(0);
        Set_right_pwm(0);
        motor_set_standby(0U);
        g_local_estop_latched = 1U;
        g_state = V2026_STATE_ESTOP;
        g_display_due = 1U;
    }

    if(g_display_due != 0U)
    {
        g_display_due = 0U;
        OLED_ShowString(1, 1, "V2026 NAV SAFE  ");
        OLED_ShowString(2, 1, (char *) v2026_state_text(g_state));
        OLED_ShowString(3, 1, "V:");
        OLED_ShowSignedNum(3, 3, g_last_command.linear_mm_s, 4);
        OLED_ShowString(3, 8, " W:");
        OLED_ShowSignedNum(3, 11,
                           g_last_command.angular_mdeg_s / 1000, 3);
        OLED_ShowString(4, 1, "OK:");
        OLED_ShowNum(4, 4, g_last_link.frames_ok % 1000U, 3);
        OLED_ShowString(4, 8, "AGE:");
        OLED_ShowNum(4, 12, g_last_link.age_ms > 999U ? 999U :
                                               g_last_link.age_ms, 3);
    }
}

void v2026_nav_app_tick_10ms(void)
{
    static uint8_t display_divider;
    int32_t left_raw;
    int32_t right_raw;
    uint8_t connected;

    v2026_nav_link_tick_10ms();
    encoder_read_and_reset(&left_raw, &right_raw);
    g_left_total += encoder_left_apply_sign(left_raw);
    g_right_total += encoder_right_apply_sign(right_raw);
    connected = v2026_nav_link_get_snapshot(&g_last_command, &g_last_link);

    /* Stage 1 is deliberately read-only at the actuators. The velocity-loop
     * adapter is enabled only after counts/mm and wheel track are measured. */
    Set_left_pwm(0);
    Set_right_pwm(0);
    motor_set_standby(0U);

    if(g_local_estop_latched != 0U)
    {
        g_state = V2026_STATE_ESTOP;
    }
    else if(connected == 0U)
    {
        g_state = (g_last_link.frames_ok == 0U) ? V2026_STATE_WAIT_LINK :
                                                 V2026_STATE_LINK_LOST;
    }
    else if((g_last_command.flags & V2026_NAV_FLAG_ESTOP) != 0U)
    {
        g_state = V2026_STATE_ESTOP;
    }
    else if((g_last_command.flags & V2026_NAV_FLAG_ENABLE) == 0U)
    {
        g_state = V2026_STATE_COMMAND_DISABLED;
    }
    else
    {
        g_state = V2026_STATE_SAFE_READY;
    }

    display_divider++;
    if(display_divider >= 10U)
    {
        display_divider = 0U;
        g_display_due = 1U;
    }
}
