/* H2026 V17 restored: reliable +5 cm to -5 cm phase handoff. */
#include "ball_balance.h"
#include "ball_beam_controller.h"
#include "board.h"
#include "bsp_keys.h"
#include "bsp_motor.h"
#include "bsp_stepper.h"
#include "bsp_timebase.h"
#include "bsp_vision_uart.h"
#include "oled.h"
#include "vision_ascii_parser.h"

#define VISION_TIMEOUT_MS             300U
#define CONTROL_PERIOD_MS              10U
#define OLED_UPDATE_PERIOD_MS         250U
#define KEY_EDGE_LOCKOUT_MS            80U

#define MANUAL_JOG_SPEED_STEPS_S      160
#define LEVEL_RETURN_SPEED_STEPS_S    180
#define ACTIVE_SPEED_LIMIT_STEPS_S    600
#define Q3_SPEED_LIMIT_STEPS_S        1000
#define CENTER_SPEED_ACCEL_PER_CONTROL 50
#define Q3_SPEED_ACCEL_PER_CONTROL     100
#define POSITION_CONTROL_GAIN           8
#define POSITION_DEADBAND_STEPS          1
#define ACTUATOR_SOFT_LIMIT_STEPS      420
#define Q3_TARGET_MM                    50
#define Q3_REACH_BAND_MM                10
#define Q3_FINAL_SPEED_MM_S              5
#define Q3_REACH_FRAMES                  1U
#define Q3_FINAL_STABLE_FRAMES           3U
#define Q3_TIME_LIMIT_MS              5000U
#define Q3_ABORT_TIME_MS              7000U
#define Q3_POSITION_KP_STEPS_MM          8
#define Q3_VELOCITY_KD_STEPS_MM_S         6
#define Q3_MOVING_TOWARD_MM_S            8
#define Q3_BREAKAWAY_COMMAND_STEPS      330
#define Q3_COMMAND_LIMIT_STEPS          380

/*
 * Measured on this mechanism with the JY61P fixed to the pipe:
 * +80 steps -> roll -0.27 deg, -80 steps -> roll +0.35 deg.
 * Therefore 1 degree is about 258 steps.  The values below request at most
 * The assembled pipe needs roughly +288/-253 steps to break static friction.
 * Add that effort gradually while the ball is stuck, then remove it quickly
 * as soon as the ball is moving toward the centre.
 */
static const ball_beam_params_t g_balance_params =
{
    4, 1,       /* position: 4 steps/mm */
    8, 1,       /* velocity damping: 8 steps/(mm/s) */
    4,          /* quiet position band, mm */
    3,          /* quiet velocity band, mm/s */
    120,        /* adaptive assist build, steps/s */
    300,        /* adaptive assist decay, steps/s */
    280,        /* adaptive assist maximum, steps */
    380,        /* target limit, steps (about 1.47 deg) */
    500         /* target slew, steps/s */
};

typedef enum
{
    APP_LEVEL_SETUP = 0,
    APP_READY,
    APP_ACTIVE,
    APP_VISION_LOST,
    APP_SOFT_LIMIT,
    APP_ESTOP
} app_state_t;

typedef enum
{
    MODE_CENTER_HOLD = 0,
    MODE_Q3_SEQUENCE
} control_mode_t;

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

static void oled_show_padded_line(uint8_t y, const uint8_t *text)
{
    uint8_t line[22];
    uint8_t index;

    for(index = 0U; index < 21U; index++)
    {
        line[index] = (uint8_t)' ';
    }
    index = 0U;
    while((index < 21U) && (text[index] != (uint8_t)'\0'))
    {
        line[index] = text[index];
        index++;
    }
    line[21] = (uint8_t)'\0';
    OLED_ShowString(0U, y, line, 8U, 1U);
}

static void format_unsigned(uint8_t *line,
                            uint8_t start,
                            uint32_t value,
                            uint8_t digits)
{
    uint8_t index;

    for(index = 0U; index < digits; index++)
    {
        uint8_t output =
            (uint8_t)(start + digits - 1U - index);
        line[output] = (uint8_t)('0' + (value % 10U));
        value /= 10U;
    }
}

static void format_signed(uint8_t *line,
                          uint8_t sign_index,
                          int32_t value,
                          uint8_t digits)
{
    uint32_t magnitude;

    if(value < 0)
    {
        line[sign_index] = (uint8_t)'-';
        magnitude = (uint32_t)(-value);
    }
    else
    {
        line[sign_index] = (uint8_t)'+';
        magnitude = (uint32_t)value;
    }
    format_unsigned(line,
                    (uint8_t)(sign_index + 1U),
                    magnitude,
                    digits);
}

static int16_t clamp_speed(int32_t speed, int16_t limit)
{
    if(speed > limit)
    {
        return limit;
    }
    if(speed < -limit)
    {
        return (int16_t)(-limit);
    }
    return (int16_t)speed;
}

static int16_t ramp_speed(int16_t current,
                          int16_t target,
                          int16_t increment)
{
    if(current < target)
    {
        int32_t next = (int32_t)current +
                       increment;
        return (next > target) ? target : (int16_t)next;
    }
    if(current > target)
    {
        int32_t next = (int32_t)current -
                       increment;
        return (next < target) ? target : (int16_t)next;
    }
    return current;
}

static int16_t q3_fast_target_steps(const ball_balance_t *balance,
                                    int16_t setpoint_mm)
{
    int32_t error_mm =
        (int32_t)balance->x_mm - setpoint_mm;
    int32_t velocity_mm_s = balance->velocity_mm_s;
    int32_t absolute_error =
        (error_mm < 0) ? -error_mm : error_mm;
    uint8_t moving_toward_target =
        (((error_mm > 0) &&
          (velocity_mm_s < -Q3_MOVING_TOWARD_MM_S)) ||
         ((error_mm < 0) &&
          (velocity_mm_s > Q3_MOVING_TOWARD_MM_S))) ? 1U : 0U;
    int32_t output;

    /* A stopped or wrongly-moving ball needs one decisive breakaway command.
     * Once it is moving toward the target, the velocity term takes over and
     * starts braking well before the target rather than carrying the ball to
     * an end stop. */
    if((absolute_error > Q3_REACH_BAND_MM) &&
       (moving_toward_target == 0U))
    {
        output = (error_mm > 0) ?
                 Q3_BREAKAWAY_COMMAND_STEPS :
                 -Q3_BREAKAWAY_COMMAND_STEPS;
    }
    else
    {
        output =
            error_mm * Q3_POSITION_KP_STEPS_MM +
            velocity_mm_s * Q3_VELOCITY_KD_STEPS_MM_S;
    }

    return clamp_speed(output, Q3_COMMAND_LIMIT_STEPS);
}

static int16_t actuator_position_speed(int16_t target_position,
                                       int16_t speed_limit)
{
    int32_t error =
        (int32_t)target_position - stepper_get_position();

    if((error >= -POSITION_DEADBAND_STEPS) &&
       (error <= POSITION_DEADBAND_STEPS))
    {
        return 0;
    }
    return clamp_speed(error * POSITION_CONTROL_GAIN,
                       speed_limit);
}

static void show_state(app_state_t state,
                       control_mode_t mode,
                       uint8_t q3_phase,
                       const ball_balance_t *balance,
                       uint8_t vision_fresh,
                       int16_t setpoint_mm,
                       int16_t target_steps,
                       int16_t assist_steps,
                       uint32_t q3_elapsed_ms)
{
    uint8_t coordinate_line[] = "X:+000 V:+000 F:0";
    uint8_t motion_line[] = "P:+0000 T:+0000";
    uint8_t control_line[] = "A:+000 S:+0000";
    uint8_t q3_line[] = "Q:+000 E:+000 T:0000";

    format_signed(coordinate_line, 2U, balance->x_mm, 3U);
    format_signed(coordinate_line,
                  9U,
                  balance->velocity_mm_s,
                  3U);
    coordinate_line[16] =
        (vision_fresh != 0U) ? (uint8_t)'1' : (uint8_t)'0';

    format_signed(motion_line, 2U, stepper_get_position(), 4U);
    format_signed(motion_line, 10U, target_steps, 4U);
    format_signed(control_line, 2U, assist_steps, 3U);
    format_signed(control_line, 9U, stepper_get_speed(), 4U);
    format_signed(q3_line, 2U, setpoint_mm, 3U);
    format_signed(q3_line,
                  9U,
                  (int32_t)balance->x_mm - setpoint_mm,
                  3U);
    format_unsigned(q3_line,
                    16U,
                    (q3_elapsed_ms > 9999U) ?
                    9999U : q3_elapsed_ms,
                    4U);

    if(state == APP_LEVEL_SETUP)
    {
        oled_show_padded_line(0U, (uint8_t *)"V17 LEVEL SW1/SW2");
        oled_show_padded_line(16U, coordinate_line);
        oled_show_padded_line(32U, motion_line);
        oled_show_padded_line(48U, (uint8_t *)"SW3 SET LEVEL ZERO");
    }
    else if(state == APP_READY)
    {
        oled_show_padded_line(0U, (uint8_t *)"V17 BALANCE READY");
        oled_show_padded_line(16U, coordinate_line);
        oled_show_padded_line(32U, motion_line);
        oled_show_padded_line(48U, (uint8_t *)"SW3:CENTER SW1:Q3");
    }
    else if(state == APP_ACTIVE)
    {
        if(mode == MODE_Q3_SEQUENCE)
        {
            if(q3_phase == 0U)
            {
                oled_show_padded_line(0U,
                    (uint8_t *)"V17 Q3 TO +5CM");
            }
            else if(q3_phase == 1U)
            {
                oled_show_padded_line(0U,
                    (uint8_t *)"V17 Q3 TO -5CM");
            }
            else if(q3_phase == 3U)
            {
                oled_show_padded_line(0U,
                    (uint8_t *)"V17 Q3 TIMEOUT");
            }
            else if(q3_elapsed_ms <= Q3_TIME_LIMIT_MS)
            {
                oled_show_padded_line(0U,
                    (uint8_t *)"V17 Q3 PASS HOLD");
            }
            else
            {
                oled_show_padded_line(0U,
                    (uint8_t *)"V17 Q3 LATE HOLD");
            }
        }
        else
        {
            oled_show_padded_line(0U,
                (uint8_t *)"V17 CENTER ACTIVE");
        }
        oled_show_padded_line(16U, coordinate_line);
        oled_show_padded_line(32U, motion_line);
        oled_show_padded_line(48U,
            (mode == MODE_Q3_SEQUENCE) ? q3_line : control_line);
    }
    else if(state == APP_VISION_LOST)
    {
        oled_show_padded_line(0U, (uint8_t *)"V17 VISION LOST");
        oled_show_padded_line(16U, coordinate_line);
        oled_show_padded_line(32U, motion_line);
        oled_show_padded_line(48U, (uint8_t *)"VALID THEN PRESS SW3");
    }
    else if(state == APP_SOFT_LIMIT)
    {
        oled_show_padded_line(0U, (uint8_t *)"V17 SOFT LIMIT");
        oled_show_padded_line(16U, coordinate_line);
        oled_show_padded_line(32U, motion_line);
        oled_show_padded_line(48U, (uint8_t *)"RESET AND RELEVEL");
    }
    else
    {
        oled_show_padded_line(0U, (uint8_t *)"V17 EMERGENCY STOP");
        oled_show_padded_line(16U, coordinate_line);
        oled_show_padded_line(32U, motion_line);
        oled_show_padded_line(48U,
            (mode == MODE_Q3_SEQUENCE) ? q3_line :
            (uint8_t *)"RESET BOARD TO CLEAR");
    }
    OLED_Refresh();
}

int main(void)
{
    vision_ascii_parser_t parser;
    ball_balance_t balance;
    ball_beam_controller_t controller;
    app_state_t state = APP_LEVEL_SETUP;
    control_mode_t mode = MODE_CENTER_HOLD;
    uint32_t last_packet_ms = 0U;
    uint32_t last_controller_ms = 0U;
    uint32_t next_control_ms = 0U;
    uint32_t next_oled_ms = 0U;
    uint32_t last_heartbeat_ms = 0U;
    uint32_t last_key_edge_ms = 0U;
    uint8_t have_packet = 0U;
    uint8_t previous_key_mask = 0U;
    uint8_t stepper_enabled = 0U;
    uint8_t force_oled = 0U;
    uint8_t q3_phase = 0U;
    uint8_t q3_reach_count = 0U;
    int16_t target_steps = 0;
    int16_t commanded_speed = 0;
    int16_t setpoint_mm = 0;
    uint32_t q3_start_ms = 0U;
    uint32_t q3_elapsed_ms = 0U;

    board_init();
    heartbeat_led_init();
    keys_init();
    motor_init_safe();
    motor_all_stop();
    stepper_init_safe();
    stepper_emergency_stop();
    timebase_init_1ms();
    vision_ascii_parser_init(&parser);
    ball_balance_init(&balance);
    ball_beam_controller_init(&controller, 0);
    vision_uart_init(9600U);

    OLED_Init();
    OLED_Clear();
    show_state(state,
               mode,
               q3_phase,
               &balance,
               0U,
               setpoint_mm,
               0,
               0,
               0U);

    while(1)
    {
        uint8_t byte;
        uint8_t key_mask;
        uint8_t key_edges;
        uint8_t vision_fresh;
        uint8_t new_measurement = 0U;
        uint32_t now_ms = timebase_millis();

        while(vision_uart_read_byte(&byte) != 0U)
        {
            vision_ascii_result_t result =
                vision_ascii_parser_push(&parser, byte);
            if(result == VISION_ASCII_FRAME_OK)
            {
                have_packet = 1U;
                last_packet_ms = now_ms;
                ball_balance_update_measurement(&balance,
                                                parser.x,
                                                parser.frame_count,
                                                now_ms);
                new_measurement = 1U;
            }
        }

        vision_fresh =
            ((have_packet != 0U) &&
             ((now_ms - last_packet_ms) <=
              VISION_TIMEOUT_MS)) ? 1U : 0U;

        key_mask = keys_read_pressed_mask();
        key_edges =
            (uint8_t)(key_mask &
                      (uint8_t)(~previous_key_mask));
        previous_key_mask = key_mask;
        if(key_edges != 0U)
        {
            if((now_ms - last_key_edge_ms) <
               KEY_EDGE_LOCKOUT_MS)
            {
                key_edges = 0U;
            }
            else
            {
                last_key_edge_ms = now_ms;
            }
        }

        if((key_mask & KEY_SW4_MASK) != 0U)
        {
            stepper_emergency_stop();
            stepper_enabled = 0U;
            commanded_speed = 0;
            target_steps = 0;
            state = APP_ESTOP;
            force_oled = 1U;
        }
        else if(state == APP_LEVEL_SETUP)
        {
            if(((key_mask & KEY_SW1_MASK) != 0U) &&
               ((key_mask & KEY_SW2_MASK) == 0U))
            {
                if(stepper_enabled == 0U)
                {
                    stepper_enable(1U);
                    delay_ms(3U);
                    stepper_enabled = 1U;
                }
                commanded_speed = MANUAL_JOG_SPEED_STEPS_S;
                stepper_set_speed(commanded_speed);
            }
            else if(((key_mask & KEY_SW2_MASK) != 0U) &&
                    ((key_mask & KEY_SW1_MASK) == 0U))
            {
                if(stepper_enabled == 0U)
                {
                    stepper_enable(1U);
                    delay_ms(3U);
                    stepper_enabled = 1U;
                }
                commanded_speed = -MANUAL_JOG_SPEED_STEPS_S;
                stepper_set_speed(commanded_speed);
            }
            else
            {
                commanded_speed = 0;
                stepper_set_speed(0);
            }

            if((key_edges & KEY_SW3_MASK) != 0U)
            {
                if(stepper_enabled == 0U)
                {
                    stepper_enable(1U);
                    delay_ms(3U);
                    stepper_enabled = 1U;
                }
                stepper_set_speed(0);
                commanded_speed = 0;
                stepper_set_position(0);
                target_steps = 0;
                setpoint_mm = 0;
                mode = MODE_CENTER_HOLD;
                ball_beam_controller_reset_level(&controller);
                state = (vision_fresh != 0U) ?
                        APP_ACTIVE : APP_READY;
                last_controller_ms = now_ms;
                force_oled = 1U;
            }
        }
        else if((state == APP_ESTOP) ||
                (state == APP_SOFT_LIMIT))
        {
            /* Faults stay latched until board reset. */
        }
        else
        {
            if(((state == APP_READY) ||
                (state == APP_VISION_LOST)) &&
               ((key_edges & KEY_SW1_MASK) != 0U) &&
               (vision_fresh != 0U))
            {
                ball_balance_init(&balance);
                ball_balance_update_measurement(&balance,
                                                parser.x,
                                                parser.frame_count,
                                                now_ms);
                ball_beam_controller_reset_level(&controller);
                mode = MODE_Q3_SEQUENCE;
                q3_phase = 0U;
                q3_reach_count = 0U;
                q3_start_ms = now_ms;
                q3_elapsed_ms = 0U;
                setpoint_mm = Q3_TARGET_MM;
                target_steps = 0;
                last_controller_ms = now_ms;
                state = APP_ACTIVE;
                force_oled = 1U;
            }
            else if((key_edges & KEY_SW3_MASK) != 0U)
            {
                if(state == APP_ACTIVE)
                {
                    state = APP_READY;
                    target_steps = 0;
                    setpoint_mm = 0;
                    mode = MODE_CENTER_HOLD;
                    ball_beam_controller_reset_level(&controller);
                    force_oled = 1U;
                }
                else if(vision_fresh != 0U)
                {
                    ball_balance_init(&balance);
                    ball_balance_update_measurement(&balance,
                                                    parser.x,
                                                    parser.frame_count,
                                                    now_ms);
                    ball_beam_controller_reset_level(&controller);
                    target_steps = 0;
                    setpoint_mm = 0;
                    mode = MODE_CENTER_HOLD;
                    last_controller_ms = now_ms;
                    state = APP_ACTIVE;
                    force_oled = 1U;
                }
            }

            if((state == APP_ACTIVE) &&
               (vision_fresh == 0U))
            {
                state = APP_VISION_LOST;
                target_steps = 0;
                setpoint_mm = 0;
                mode = MODE_CENTER_HOLD;
                commanded_speed = 0;
                stepper_set_speed(0);
                balance.velocity_mm_s = 0;
                ball_beam_controller_reset_level(&controller);
                force_oled = 1U;
            }
        }

        if((state == APP_ACTIVE) &&
           (new_measurement != 0U))
        {
            uint32_t controller_dt;

            if(mode == MODE_Q3_SEQUENCE)
            {
                int32_t q3_error =
                    (int32_t)balance.x_mm - setpoint_mm;
                int32_t abs_q3_error =
                    (q3_error < 0) ? -q3_error : q3_error;
                int32_t abs_q3_velocity =
                    (balance.velocity_mm_s < 0) ?
                    -(int32_t)balance.velocity_mm_s :
                    balance.velocity_mm_s;

                if(q3_phase < 2U)
                {
                    q3_elapsed_ms = now_ms - q3_start_ms;
                }

                if((q3_phase < 2U) &&
                   (q3_elapsed_ms >= Q3_ABORT_TIME_MS))
                {
                    q3_phase = 3U;
                    q3_reach_count = 0U;
                    setpoint_mm = 0;
                    target_steps = 0;
                    ball_beam_controller_reset_level(&controller);
                    force_oled = 1U;
                }
                else if(q3_phase == 0U)
                {
                    if(abs_q3_error <= Q3_REACH_BAND_MM)
                    {
                        q3_reach_count++;
                    }
                    else
                    {
                        q3_reach_count = 0U;
                    }

                    if(q3_reach_count >= Q3_REACH_FRAMES)
                    {
                        q3_phase = 1U;
                        q3_reach_count = 0U;
                        setpoint_mm = -Q3_TARGET_MM;
                        /* Make the handoff visible and deterministic even
                         * when the K230 suppresses unchanged coordinates. */
                        force_oled = 1U;
                    }
                }
                else if(q3_phase == 1U)
                {
                    if((abs_q3_error <= Q3_REACH_BAND_MM) &&
                       (abs_q3_velocity <= Q3_FINAL_SPEED_MM_S))
                    {
                        q3_reach_count++;
                    }
                    else
                    {
                        q3_reach_count = 0U;
                    }

                    if(q3_reach_count >= Q3_FINAL_STABLE_FRAMES)
                    {
                        q3_phase = 2U;
                        q3_reach_count = 0U;
                        q3_elapsed_ms = now_ms - q3_start_ms;
                        force_oled = 1U;
                    }
                }
            }

            controller_dt = now_ms - last_controller_ms;
            if(last_controller_ms == 0U)
            {
                controller_dt = 50U;
            }
            if(mode == MODE_Q3_SEQUENCE)
            {
                /* Requirement 3 is a time-limited transfer, not the quiet
                 * centre-hold problem.  Use the measured breakaway effort
                 * immediately so motion starts without the slow adaptive
                 * build-up used by the centre controller. */
                if(q3_phase < 3U)
                {
                    target_steps =
                        q3_fast_target_steps(&balance,
                                             setpoint_mm);
                }
                else
                {
                    target_steps = 0;
                }
            }
            else
            {
                target_steps =
                    ball_beam_controller_update(
                        &controller,
                        &g_balance_params,
                        balance.x_mm,
                        balance.velocity_mm_s,
                        setpoint_mm,
                        (uint16_t)controller_dt);
            }
            last_controller_ms = now_ms;
        }

        if((int32_t)(now_ms - next_control_ms) >= 0)
        {
            next_control_ms = now_ms + CONTROL_PERIOD_MS;

            if((state == APP_READY) ||
               (state == APP_VISION_LOST))
            {
                target_steps = 0;
            }

            if((state == APP_READY) ||
               (state == APP_ACTIVE) ||
               (state == APP_VISION_LOST))
            {
                int16_t desired_speed;
                int16_t speed_limit =
                    ((state == APP_ACTIVE) &&
                     (mode == MODE_Q3_SEQUENCE) &&
                     (q3_phase < 2U)) ?
                    Q3_SPEED_LIMIT_STEPS_S :
                    ((state == APP_ACTIVE) ?
                     ACTIVE_SPEED_LIMIT_STEPS_S :
                     LEVEL_RETURN_SPEED_STEPS_S);
                int16_t speed_increment =
                    ((state == APP_ACTIVE) &&
                     (mode == MODE_Q3_SEQUENCE) &&
                     (q3_phase < 2U)) ?
                    Q3_SPEED_ACCEL_PER_CONTROL :
                    CENTER_SPEED_ACCEL_PER_CONTROL;

                if((stepper_get_position() >
                    ACTUATOR_SOFT_LIMIT_STEPS) ||
                   (stepper_get_position() <
                    -ACTUATOR_SOFT_LIMIT_STEPS))
                {
                    stepper_emergency_stop();
                    stepper_enabled = 0U;
                    commanded_speed = 0;
                    state = APP_SOFT_LIMIT;
                    force_oled = 1U;
                }
                else
                {
                    desired_speed =
                        actuator_position_speed(target_steps,
                                                speed_limit);
                    commanded_speed =
                        ramp_speed(commanded_speed,
                                   desired_speed,
                                   speed_increment);
                    stepper_set_speed(commanded_speed);
                }
            }
        }

        if(force_oled != 0U)
        {
            stepper_set_speed(0);
            commanded_speed = 0;
            show_state(state,
                       mode,
                       q3_phase,
                       &balance,
                       vision_fresh,
                       setpoint_mm,
                       target_steps,
                       ball_beam_controller_get_assist_steps(
                           &controller),
                       q3_elapsed_ms);
            force_oled = 0U;
            next_oled_ms = timebase_millis() +
                           OLED_UPDATE_PERIOD_MS;
        }
        else if(((int32_t)(now_ms - next_oled_ms) >= 0) &&
                (state != APP_ACTIVE) &&
                (commanded_speed == 0))
        {
            show_state(state,
                       mode,
                       q3_phase,
                       &balance,
                       vision_fresh,
                       setpoint_mm,
                       target_steps,
                       ball_beam_controller_get_assist_steps(
                           &controller),
                       q3_elapsed_ms);
            next_oled_ms = timebase_millis() +
                           OLED_UPDATE_PERIOD_MS;
        }

        if((now_ms - last_heartbeat_ms) >= 500U)
        {
            GPIO_ToggleBits(GPIOB, GPIO_Pin_2);
            last_heartbeat_ms = now_ms;
        }
    }
}
