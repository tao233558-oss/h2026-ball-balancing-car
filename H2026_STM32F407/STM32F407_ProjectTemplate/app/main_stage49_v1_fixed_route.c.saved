#include "board.h"
#include "bsp_encoder.h"
#include "bsp_keys.h"
#include "bsp_motor.h"
#include "bsp_stepper.h"
#include "oled.h"
#include "speed_pi.h"

/*
 * H2026 four-channel digital line-follow firmware, V1 fixed-route control.
 *
 * Sensor order on the vehicle, left to right:
 *   S1 -> PB1, S2 -> PB0, S3 -> PC5, S4 -> PC4
 *
 * Measured polarity:
 *   white = 0, black line = 1
 */

#define LINE_SENSOR_COUNT           4U

#define TRACK_CONTROL_PERIOD_MS     20U
#define TRACK_DISPLAY_PERIOD_MS     200U
#define TRACK_BASE_TARGET           34
#define TRACK_MEDIUM_BASE_TARGET    22
#define TRACK_SHARP_BASE_TARGET     17
#define TRACK_EDGE_OUTER_TARGET     18
#define TRACK_LOST_STRAIGHT_TARGET  10
#define TRACK_TARGET_MIN            4
#define TRACK_TARGET_MAX            38
#define TRACK_TURN_LIMIT            14
#define TRACK_LOST_STOP_TICKS       30U
#define TRACK_CENTER_MAX_ERROR      12
#define TRACK_ACCEL_STEP              2
#define TRACK_DECEL_STEP              4
#define TRACK_CURVE_HOLD_TICKS        25U
#define TRACK_CURVE_BIAS_TURN          4
#define ROUTE_CURVE1_BRAKE_START     8220U
#define ROUTE_CURVE1_END            20500U
#define ROUTE_CURVE2_BRAKE_START    28430U

#define SPEED_REFERENCE_TARGET      40
#define FWD_LEFT_FEEDFORWARD        253
#define FWD_RIGHT_FEEDFORWARD       252

static const int8_t sensor_weights[LINE_SENSOR_COUNT] =
{
    -30, -10, 10, 30
};

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

static void line_sensor_read(uint8_t states[LINE_SENSOR_COUNT])
{
    states[0] =
        (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) != Bit_RESET) ?
        1U : 0U;
    states[1] =
        (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) != Bit_RESET) ?
        1U : 0U;
    states[2] =
        (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_5) != Bit_RESET) ?
        1U : 0U;
    states[3] =
        (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_4) != Bit_RESET) ?
        1U : 0U;
}

static int8_t select_line_group(
    const uint8_t raw_states[LINE_SENSOR_COUNT],
    int8_t preferred_position,
    uint8_t selected_states[LINE_SENSOR_COUNT],
    uint8_t *selected_count)
{
    uint8_t best_start = 0U;
    uint8_t best_end = 0U;
    uint8_t best_count = 0U;
    uint8_t best_found = 0U;
    uint16_t best_distance = 0xFFFFU;
    int16_t best_sum = 0;
    uint8_t channel;

    for(channel = 0U; channel < LINE_SENSOR_COUNT; channel++)
    {
        selected_states[channel] = 0U;
    }
    *selected_count = 0U;

    channel = 0U;
    while(channel < LINE_SENSOR_COUNT)
    {
        uint8_t start;
        uint8_t end;
        uint8_t count = 0U;
        int16_t sum = 0;
        int16_t position;
        int16_t distance;

        while((channel < LINE_SENSOR_COUNT) &&
              (raw_states[channel] == 0U))
        {
            channel++;
        }
        if(channel >= LINE_SENSOR_COUNT)
        {
            break;
        }

        start = channel;
        while((channel < LINE_SENSOR_COUNT) &&
              (raw_states[channel] != 0U))
        {
            sum = (int16_t)(sum + sensor_weights[channel]);
            count++;
            channel++;
        }
        end = (uint8_t)(channel - 1U);
        position = (int16_t)(sum / (int16_t)count);
        distance = (int16_t)(position -
                             (int16_t)preferred_position);
        if(distance < 0)
        {
            distance = (int16_t)(-distance);
        }

        if((best_found == 0U) ||
           ((uint16_t)distance < best_distance) ||
           (((uint16_t)distance == best_distance) &&
            (count > best_count)))
        {
            best_found = 1U;
            best_start = start;
            best_end = end;
            best_count = count;
            best_distance = (uint16_t)distance;
            best_sum = sum;
        }
    }

    if(best_found == 0U)
    {
        return 0;
    }

    for(channel = best_start; channel <= best_end; channel++)
    {
        selected_states[channel] = 1U;
        (*selected_count)++;
    }

    return (int8_t)(best_sum / (int16_t)best_count);
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

static void format_state_line(
    uint8_t line[15],
    const uint8_t states[LINE_SENSOR_COUNT],
    int8_t error)
{
    uint8_t magnitude;
    uint8_t channel;

    for(channel = 0U; channel < LINE_SENSOR_COUNT; channel++)
    {
        line[2U + channel] =
            (states[channel] != 0U) ? (uint8_t)'1' : (uint8_t)'0';
    }

    if(error < 0)
    {
        line[9] = (uint8_t)'-';
        magnitude = (uint8_t)(-error);
    }
    else
    {
        line[9] = (uint8_t)'+';
        magnitude = (uint8_t)error;
    }
    line[10] = (uint8_t)('0' + (magnitude / 10U));
    line[11] = (uint8_t)('0' + (magnitude % 10U));
}

static void show_line_status(
    const uint8_t states[LINE_SENSOR_COUNT],
    int8_t error,
    uint8_t active_count,
    int16_t left_target,
    int16_t right_target,
    uint8_t mode,
    uint8_t tracking,
    uint32_t route_distance)
{
    uint8_t state_line[] = "B:0000 E:+00";
    uint8_t target_line[] = "N:0 T:00/00";
    uint8_t mode_line[] = "D:00000 M:N";
    uint8_t digit;

    format_state_line(state_line, states, error);
    target_line[2] = (uint8_t)('0' + active_count);

    if(left_target < 0)
    {
        left_target = 0;
    }
    if(right_target < 0)
    {
        right_target = 0;
    }
    if(left_target > 99)
    {
        left_target = 99;
    }
    if(right_target > 99)
    {
        right_target = 99;
    }
    target_line[6] =
        (uint8_t)('0' + ((uint16_t)left_target / 10U));
    target_line[7] =
        (uint8_t)('0' + ((uint16_t)left_target % 10U));
    target_line[9] =
        (uint8_t)('0' + ((uint16_t)right_target / 10U));
    target_line[10] =
        (uint8_t)('0' + ((uint16_t)right_target % 10U));
    if(route_distance > 99999U)
    {
        route_distance = 99999U;
    }
    for(digit = 0U; digit < 5U; digit++)
    {
        mode_line[6U - digit] =
            (uint8_t)('0' + (route_distance % 10U));
        route_distance /= 10U;
    }
    mode_line[10] = mode;

    oled_show_padded_line(0U, state_line);
    oled_show_padded_line(16U, target_line);
    oled_show_padded_line(32U, mode_line);
    oled_show_padded_line(
        48U,
        (tracking != 0U) ?
        (uint8_t *)"SW3/4: STOP" :
        (uint8_t *)"SW3: START  SW4:STOP");
    OLED_Refresh();
}

static void show_start_banner(void)
{
    oled_show_padded_line(0U,  (uint8_t *)"4CH V1S8 ROUTE READY");
    oled_show_padded_line(16U, (uint8_t *)"BLACK=1 WHITE=0");
    oled_show_padded_line(32U, (uint8_t *)"CENTER LINE FIRST");
    oled_show_padded_line(48U, (uint8_t *)"MOTOR DISABLED");
    OLED_Refresh();
}

static void show_stop_reason(const uint8_t *reason)
{
    oled_show_padded_line(0U, reason);
    oled_show_padded_line(16U, (uint8_t *)"MOTORS DISABLED");
    oled_show_padded_line(32U, (uint8_t *)"CENTER LINE AGAIN");
    oled_show_padded_line(48U, (uint8_t *)"SW3 TO RESTART");
    OLED_Refresh();
}

static int16_t clamp_target(int16_t value)
{
    if(value < TRACK_TARGET_MIN)
    {
        return TRACK_TARGET_MIN;
    }
    if(value > TRACK_TARGET_MAX)
    {
        return TRACK_TARGET_MAX;
    }
    return value;
}

static int16_t clamp_turn(int16_t value)
{
    if(value < -TRACK_TURN_LIMIT)
    {
        return -TRACK_TURN_LIMIT;
    }
    if(value > TRACK_TURN_LIMIT)
    {
        return TRACK_TURN_LIMIT;
    }
    return value;
}

static int16_t smooth_target(int16_t current, int16_t desired)
{
    /* An edge sensor must still be able to stop the inner wheel at once. */
    if(desired == 0)
    {
        return 0;
    }

    if(desired > current)
    {
        current = (int16_t)(current + TRACK_ACCEL_STEP);
        if(current > desired)
        {
            current = desired;
        }
    }
    else if(desired < current)
    {
        current = (int16_t)(current - TRACK_DECEL_STEP);
        if(current < desired)
        {
            current = desired;
        }
    }

    return current;
}

static int16_t scaled_feedforward(int16_t reference_feedforward,
                                  int16_t target)
{
    int32_t result =
        ((int32_t)reference_feedforward * target) /
        SPEED_REFERENCE_TARGET;

    if(result < 100)
    {
        result = 100;
    }
    if(result > 400)
    {
        result = 400;
    }
    return (int16_t)result;
}

static int16_t encoder_delta(uint16_t current, uint16_t previous)
{
    uint16_t wrapped_delta = (uint16_t)(current - previous);
    return (int16_t)wrapped_delta;
}

static uint8_t route_is_right_curve(uint32_t route_distance)
{
    if(((route_distance >= ROUTE_CURVE1_BRAKE_START) &&
        (route_distance < ROUTE_CURVE1_END)) ||
       (route_distance >= ROUTE_CURVE2_BRAKE_START))
    {
        return 1U;
    }
    return 0U;
}

int main(void)
{
    uint8_t raw_states[LINE_SENSOR_COUNT] = {0U};
    uint8_t selected_states[LINE_SENSOR_COUNT] = {0U};
    uint8_t active_count = 0U;
    uint8_t key_mask;
    uint8_t key_edges;
    uint8_t previous_key_mask = 0U;
    uint8_t tracking = 0U;
    uint8_t lost_ticks = 0U;
    int8_t error = 0;
    int8_t previous_error = 0;
    int8_t center_bias = 0;
    int8_t curve_direction = 0;
    uint8_t curve_hold_ticks = 0U;
    int8_t previous_motion_mode = 0;
    uint16_t previous_left_count = 0U;
    uint16_t previous_right_count = 0U;
    uint16_t heartbeat_ms = 0U;
    uint16_t display_ms = 0U;
    uint32_t route_distance = 0U;
    int16_t commanded_left_target = 0;
    int16_t commanded_right_target = 0;
    speed_pi_t left_pi;
    speed_pi_t right_pi;

    board_init();
    heartbeat_led_init();
    keys_init();
    motor_init_safe();
    motor_all_stop();
    stepper_init_safe();
    stepper_emergency_stop();
    encoder_init();
    encoder_reset();
    line_sensor_gpio_init();
    OLED_Init();
    OLED_Clear();
    show_start_banner();
    delay_ms(800U);

    while(1)
    {
        key_mask = keys_read_pressed_mask();
        key_edges = (uint8_t)(key_mask &
                              (uint8_t)(~previous_key_mask));
        previous_key_mask = key_mask;

        line_sensor_read(raw_states);
        error = select_line_group(
            raw_states,
            (tracking != 0U) ?
            (int8_t)(previous_error + center_bias) : 0,
            selected_states,
            &active_count);

        if(tracking == 0U)
        {
            motor_all_stop();

            if((key_edges & KEY_SW3_MASK) != 0U)
            {
                if(active_count == 0U)
                {
                    show_stop_reason((uint8_t *)"NO LINE: NO START");
                    delay_ms(500U);
                }
                else if((error < -TRACK_CENTER_MAX_ERROR) ||
                        (error > TRACK_CENTER_MAX_ERROR))
                {
                    show_stop_reason(
                        (uint8_t *)"CENTER LINE FIRST");
                    delay_ms(500U);
                }
                else if((key_mask & KEY_SW4_MASK) == 0U)
                {
                    center_bias = error;
                    previous_error = 0;
                    curve_direction = 0;
                    curve_hold_ticks = 0U;
                    previous_motion_mode = 0;
                    lost_ticks = 0U;
                    display_ms = 0U;
                    commanded_left_target = 0;
                    commanded_right_target = 0;
                    route_distance = 0U;
                    encoder_reset();
                    previous_left_count = 0U;
                    previous_right_count = 0U;
                    speed_pi_reset(&left_pi, TRACK_BASE_TARGET);
                    speed_pi_reset(&right_pi, TRACK_BASE_TARGET);
                    motor_set_both(
                        scaled_feedforward(FWD_LEFT_FEEDFORWARD,
                                           TRACK_BASE_TARGET),
                        scaled_feedforward(FWD_RIGHT_FEEDFORWARD,
                                           TRACK_BASE_TARGET));
                    tracking = 1U;
                }
            }

            if(tracking == 0U)
            {
                show_line_status(selected_states,
                                 error,
                                 active_count,
                                 0,
                                 0,
                                 (uint8_t)'N',
                                 0U,
                                 route_distance);
            }
        }
        else
        {
            uint8_t stop_requested =
                ((key_edges & KEY_SW3_MASK) != 0U) ? 1U : 0U;
            uint8_t emergency =
                ((key_mask & KEY_SW4_MASK) != 0U) ? 1U : 0U;

            if((stop_requested != 0U) || (emergency != 0U))
            {
                motor_all_stop();
                tracking = 0U;
                show_stop_reason(
                    (emergency != 0U) ?
                    (uint8_t *)"EMERGENCY STOP" :
                    (uint8_t *)"TRACK STOPPED");
                delay_ms(500U);
            }
            else
            {
                int16_t control_error;
                int16_t turn = 0;
                int16_t base_target = TRACK_BASE_TARGET;
                int16_t left_target;
                int16_t right_target;
                int16_t left_measured;
                int16_t right_measured;
                int16_t left_pwm;
                int16_t right_pwm;
                uint16_t current_left;
                uint16_t current_right;
                uint8_t route_curve =
                    route_is_right_curve(route_distance);
                int8_t motion_mode = 0;
                uint8_t display_mode = (uint8_t)'N';

                if(active_count == 0U)
                {
                    lost_ticks++;
                    control_error = previous_error;

                    if(route_curve != 0U)
                    {
                        left_target = TRACK_EDGE_OUTER_TARGET;
                        right_target = 0;
                        motion_mode = 1;
                        display_mode = (uint8_t)'R';
                    }
                    else if(previous_error > 3)
                    {
                        left_target = TRACK_EDGE_OUTER_TARGET;
                        right_target = 0;
                        motion_mode = 1;
                        display_mode = (uint8_t)'R';
                    }
                    else if(previous_error < -3)
                    {
                        left_target = 0;
                        right_target = TRACK_EDGE_OUTER_TARGET;
                        motion_mode = -1;
                        display_mode = (uint8_t)'L';
                    }
                    else
                    {
                        left_target = TRACK_LOST_STRAIGHT_TARGET;
                        right_target = TRACK_LOST_STRAIGHT_TARGET;
                        motion_mode = 2;
                        display_mode = (uint8_t)'S';
                    }
                }
                else
                {
                    int16_t magnitude;

                    lost_ticks = 0U;
                    control_error = (int16_t)error -
                                    (int16_t)center_bias;
                    if((control_error >= -2) &&
                       (control_error <= 2))
                    {
                        control_error = 0;
                    }

                    if((active_count <= 2U) &&
                       (selected_states[3] != 0U))
                    {
                        curve_direction = 1;
                        curve_hold_ticks = TRACK_CURVE_HOLD_TICKS;
                    }
                    else if((active_count <= 2U) &&
                            (selected_states[0] != 0U))
                    {
                        curve_direction = -1;
                        curve_hold_ticks = TRACK_CURVE_HOLD_TICKS;
                    }
                    else if(((curve_direction > 0) &&
                             (control_error < 0)) ||
                            ((curve_direction < 0) &&
                             (control_error > 0)))
                    {
                        curve_direction = 0;
                        curve_hold_ticks = 0U;
                    }
                    else if(curve_hold_ticks > 0U)
                    {
                        curve_hold_ticks--;
                        if(curve_hold_ticks == 0U)
                        {
                            curve_direction = 0;
                        }
                    }

                    if((active_count == 1U) &&
                       (selected_states[0] != 0U))
                    {
                        left_target = 0;
                        right_target = TRACK_EDGE_OUTER_TARGET;
                        motion_mode = -1;
                        display_mode = (uint8_t)'L';
                    }
                    else if((active_count == 1U) &&
                            (selected_states[3] != 0U))
                    {
                        left_target = TRACK_EDGE_OUTER_TARGET;
                        right_target = 0;
                        motion_mode = 1;
                        display_mode = (uint8_t)'R';
                    }
                    else
                    {
                        magnitude = control_error;
                        if(magnitude < 0)
                        {
                            magnitude = (int16_t)(-magnitude);
                        }

                        if((active_count == 1U) &&
                           ((selected_states[1] != 0U) ||
                            (selected_states[2] != 0U)))
                        {
                            /*
                             * On this vehicle S2-only/S3-only commonly occurs
                             * while travelling along a straight.  Keep the
                             * full straight speed and apply a small correction
                             * without the derivative kick caused by switching
                             * between the two inner sensors.
                             */
                            base_target = TRACK_BASE_TARGET;
                            turn = (int16_t)(control_error / 3);
                        }
                        else if(magnitude >= 18)
                        {
                            base_target = TRACK_SHARP_BASE_TARGET;
                        }
                        else if(magnitude >= 8)
                        {
                            base_target = TRACK_MEDIUM_BASE_TARGET;
                        }

                        if(!((active_count == 1U) &&
                             ((selected_states[1] != 0U) ||
                              (selected_states[2] != 0U))))
                        {
                            turn = (int16_t)(
                                (3 * control_error) / 10);
                        }

                        if((curve_hold_ticks > 0U) &&
                           (((curve_direction > 0) &&
                             (control_error >= 0)) ||
                            ((curve_direction < 0) &&
                             (control_error <= 0))))
                        {
                            if(base_target > TRACK_MEDIUM_BASE_TARGET)
                            {
                                base_target = TRACK_MEDIUM_BASE_TARGET;
                            }

                            if((curve_direction > 0) &&
                               (turn < TRACK_CURVE_BIAS_TURN))
                            {
                                turn = TRACK_CURVE_BIAS_TURN;
                                display_mode = (uint8_t)'R';
                            }
                            else if((curve_direction < 0) &&
                                    (turn > -TRACK_CURVE_BIAS_TURN))
                            {
                                turn = -TRACK_CURVE_BIAS_TURN;
                                display_mode = (uint8_t)'L';
                            }
                        }

                        if(route_curve != 0U)
                        {
                            if(base_target > TRACK_MEDIUM_BASE_TARGET)
                            {
                                base_target = TRACK_MEDIUM_BASE_TARGET;
                            }
                            if((control_error >= 0) &&
                               (turn < TRACK_CURVE_BIAS_TURN))
                            {
                                turn = TRACK_CURVE_BIAS_TURN;
                            }
                            display_mode = (uint8_t)'R';
                        }
                        turn = clamp_turn(turn);

                        left_target = clamp_target(
                            (int16_t)(base_target + turn));
                        right_target = clamp_target(
                            (int16_t)(base_target - turn));
                    }
                }

                left_target = smooth_target(commanded_left_target,
                                            left_target);
                right_target = smooth_target(commanded_right_target,
                                             right_target);
                commanded_left_target = left_target;
                commanded_right_target = right_target;

                if(lost_ticks >= TRACK_LOST_STOP_TICKS)
                {
                    motor_all_stop();
                    tracking = 0U;
                    show_stop_reason(
                        (uint8_t *)"LINE LOST: STOP");
                    delay_ms(500U);
                }
                else
                {
                    if(motion_mode != previous_motion_mode)
                    {
                        speed_pi_reset(&left_pi, left_target);
                        speed_pi_reset(&right_pi, right_target);
                        previous_motion_mode = motion_mode;
                    }

                    left_pi.target_counts = left_target;
                    right_pi.target_counts = right_target;

                    current_left =
                        (uint16_t)encoder_left_count();
                    current_right =
                        (uint16_t)encoder_right_count();
                    left_measured =
                        encoder_delta(current_left,
                                      previous_left_count);
                    right_measured =
                        encoder_delta(current_right,
                                      previous_right_count);
                    previous_left_count = current_left;
                    previous_right_count = current_right;

                    {
                        int16_t left_distance = left_measured;
                        int16_t right_distance = right_measured;

                        if(left_distance < 0)
                        {
                            left_distance = (int16_t)(-left_distance);
                        }
                        if(right_distance < 0)
                        {
                            right_distance = (int16_t)(-right_distance);
                        }
                        route_distance += (uint32_t)(
                            (left_distance + right_distance) / 2);
                    }

                    if(left_target == 0)
                    {
                        speed_pi_reset(&left_pi, 0);
                        left_pwm = 0;
                    }
                    else
                    {
                        left_pwm = speed_pi_step(
                            &left_pi,
                            left_measured,
                            scaled_feedforward(
                                FWD_LEFT_FEEDFORWARD,
                                left_target));
                    }

                    if(right_target == 0)
                    {
                        speed_pi_reset(&right_pi, 0);
                        right_pwm = 0;
                    }
                    else
                    {
                        right_pwm = speed_pi_step(
                            &right_pi,
                            right_measured,
                            scaled_feedforward(
                                FWD_RIGHT_FEEDFORWARD,
                                right_target));
                    }
                    motor_set_both(left_pwm, right_pwm);

                    if(active_count != 0U)
                    {
                        previous_error =
                            (int8_t)control_error;
                    }

                    display_ms =
                        (uint16_t)(display_ms +
                                   TRACK_CONTROL_PERIOD_MS);
                    if(display_ms >= TRACK_DISPLAY_PERIOD_MS)
                    {
                        show_line_status(selected_states,
                                         error,
                                         active_count,
                                         left_target,
                                         right_target,
                                         display_mode,
                                         1U,
                                         route_distance);
                        display_ms = 0U;
                    }
                }
            }
        }

        delay_ms(TRACK_CONTROL_PERIOD_MS);
        heartbeat_ms =
            (uint16_t)(heartbeat_ms + TRACK_CONTROL_PERIOD_MS);
        if(heartbeat_ms >= 500U)
        {
            GPIO_ToggleBits(GPIOB, GPIO_Pin_2);
            heartbeat_ms = 0U;
        }
    }
}


