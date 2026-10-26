#include "board.h"
#include "bsp_encoder.h"
#include "bsp_keys.h"
#include "bsp_motor.h"
#include "bsp_stepper.h"
#include "oled.h"
#include "speed_pi.h"

/*
 * H2026 four-channel digital line-follow firmware, V20 flowing curve.
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
#define TRACK_LOST_STRAIGHT_TARGET  16
#define TRACK_EDGE_INNER_TARGET      14
#define TRACK_SEARCH_SPIN_PWM        180
#define TRACK_SEARCH_SPIN_DELAY_TICKS  3U
#define TRACK_CURVE_MEMORY_TICKS      25U
#define TRACK_GENTLE_LOST_INNER       22
#define TRACK_TARGET_MIN            4
#define TRACK_TARGET_MAX            100
#define TRACK_LOST_STOP_TICKS       30U
#define TRACK_CENTER_MAX_ERROR       1
#define TRACK_SPEED_LEVEL_COUNT      4U
#define TRACK_DEFAULT_SPEED_LEVEL    0U
#define LINE_SAMPLE_COUNT            5U
#define LINE_SAMPLE_REMAINDER_MS    15U
#define TRACK_ACCEL_STEP              2
#define TRACK_DECEL_STEP              8

#define SPEED_REFERENCE_TARGET      40
#define FWD_LEFT_FEEDFORWARD        253
#define FWD_RIGHT_FEEDFORWARD       252

/*
 * Level 0 preserves the first stable four-channel settings.  Higher levels
 * mainly raise straight speed while retaining conservative sharp-turn and
 * edge-recovery speeds.
 */
static const int16_t track_base_targets[TRACK_SPEED_LEVEL_COUNT] =
{
    32, 50, 68, 85
};

static const int16_t track_medium_targets[TRACK_SPEED_LEVEL_COUNT] =
{
    22, 28, 34, 40
};

static const int16_t track_sharp_targets[TRACK_SPEED_LEVEL_COUNT] =
{
    16, 20, 24, 28
};

static const int16_t track_edge_targets[TRACK_SPEED_LEVEL_COUNT] =
{
    34, 40, 46, 52
};

static const int16_t track_turn_limits[TRACK_SPEED_LEVEL_COUNT] =
{
    14, 22, 30, 38
};

/* 10 means 1.0 times the original steering gain. */
static const int16_t track_turn_scale_tenths[TRACK_SPEED_LEVEL_COUNT] =
{
    10, 15, 20, 25
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

static void line_sensor_read_window(
    uint8_t states[LINE_SENSOR_COUNT])
{
    uint8_t instant_states[LINE_SENSOR_COUNT];
    uint8_t high_votes[LINE_SENSOR_COUNT] = {0U};
    uint8_t sample;
    uint8_t channel;

    /*
     * The official example reads the four GPIO levels directly.  Use a short
     * majority vote to reject one comparator glitch without retaining an old
     * black pulse into a later control period.
     */
    for(sample = 0U; sample < LINE_SAMPLE_COUNT; sample++)
    {
        line_sensor_read(instant_states);
        for(channel = 0U; channel < LINE_SENSOR_COUNT; channel++)
        {
            if(instant_states[channel] != 0U)
            {
                high_votes[channel]++;
            }
        }
        delay_ms(1U);
    }

    for(channel = 0U; channel < LINE_SENSOR_COUNT; channel++)
    {
        states[channel] =
            (high_votes[channel] >= 3U) ? 1U : 0U;
    }
}

static void line_sensor_filter_update(
    const uint8_t raw_states[LINE_SENSOR_COUNT],
    uint8_t filtered_states[LINE_SENSOR_COUNT],
    uint8_t *dropout_ticks)
{
    uint8_t channel;

    for(channel = 0U; channel < LINE_SENSOR_COUNT; channel++)
    {
        filtered_states[channel] = raw_states[channel];
    }
    *dropout_ticks = 0U;
}

static int8_t select_line_group(
    const uint8_t raw_states[LINE_SENSOR_COUNT],
    int8_t preferred_position,
    uint8_t selected_states[LINE_SENSOR_COUNT],
    uint8_t *selected_count)
{
    uint8_t pattern = 0U;
    uint8_t channel;

    (void)preferred_position;
    *selected_count = 0U;

    for(channel = 0U; channel < LINE_SENSOR_COUNT; channel++)
    {
        selected_states[channel] = raw_states[channel];
        if(raw_states[channel] != 0U)
        {
            (*selected_count)++;
            pattern = (uint8_t)(pattern |
                (uint8_t)(1U << (3U - channel)));
        }
    }

    /* Official STM32 example state table, S1 on the vehicle left. */
    switch(pattern)
    {
        case 0x04U: return -1; /* 0100 */
        case 0x0CU: return -2; /* 1100 */
        case 0x08U: return -6; /* 1000 */
        case 0x02U: return  1; /* 0010 */
        case 0x03U: return  2; /* 0011 */
        case 0x01U: return  6; /* 0001 */
        default:    return  0; /* 0110, 0000 and crossings */
    }
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
    int16_t left_measured,
    int16_t right_measured,
    uint8_t mode,
    uint8_t speed_level,
    uint8_t tracking,
    uint32_t route_distance)
{
    uint8_t state_line[] = "B:0000 E:+00";
    uint8_t target_line[] = "N:0 T:00/00";
    uint8_t actual_line[] = "D:00000 A:00/00 S0";
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

    if(left_measured < 0)
    {
        left_measured = (int16_t)(-left_measured);
    }
    if(right_measured < 0)
    {
        right_measured = (int16_t)(-right_measured);
    }
    if(left_measured > 99)
    {
        left_measured = 99;
    }
    if(right_measured > 99)
    {
        right_measured = 99;
    }
    if(route_distance > 99999U)
    {
        route_distance = 99999U;
    }
    for(digit = 0U; digit < 5U; digit++)
    {
        actual_line[6U - digit] =
            (uint8_t)('0' + (route_distance % 10U));
        route_distance /= 10U;
    }

    actual_line[10] =
        (uint8_t)('0' + ((uint16_t)left_measured / 10U));
    actual_line[11] =
        (uint8_t)('0' + ((uint16_t)left_measured % 10U));
    actual_line[13] =
        (uint8_t)('0' + ((uint16_t)right_measured / 10U));
    actual_line[14] =
        (uint8_t)('0' + ((uint16_t)right_measured % 10U));
    actual_line[17] = (uint8_t)('0' + speed_level);

    (void)mode;

    oled_show_padded_line(0U, state_line);
    oled_show_padded_line(16U, target_line);
    oled_show_padded_line(32U, actual_line);
    oled_show_padded_line(
        48U,
        (tracking != 0U) ?
        (uint8_t *)"SW3/4: STOP" :
        (uint8_t *)"SW3: START  SW4:STOP");
    OLED_Refresh();
}

static void show_start_banner(void)
{
    oled_show_padded_line(0U,  (uint8_t *)"4CH LINE V20 READY");
    oled_show_padded_line(16U, (uint8_t *)"BLACK=1 WHITE=0");
    oled_show_padded_line(32U, (uint8_t *)"SW1- SW2+ SPEED");
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

static int16_t slew_target(int16_t current, int16_t desired)
{
    if(desired == 0)
    {
        /* Preserve immediate inner-wheel stopping for edge recovery. */
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

static int16_t clamp_turn(int16_t value, int16_t limit)
{
    if(value < -limit)
    {
        return (int16_t)(-limit);
    }
    if(value > limit)
    {
        return limit;
    }
    return value;
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
    if(result > 750)
    {
        result = 750;
    }
    return (int16_t)result;
}

static int16_t encoder_delta(uint16_t current, uint16_t previous)
{
    uint16_t wrapped_delta = (uint16_t)(current - previous);
    return (int16_t)wrapped_delta;
}

int main(void)
{
    uint8_t raw_states[LINE_SENSOR_COUNT] = {0U};
    uint8_t filtered_states[LINE_SENSOR_COUNT] = {0U};
    uint8_t dropout_ticks = 0U;
    uint8_t selected_states[LINE_SENSOR_COUNT] = {0U};
    uint8_t active_count = 0U;
    uint8_t key_mask;
    uint8_t key_edges;
    uint8_t previous_key_mask = 0U;
    uint8_t tracking = 0U;
    uint8_t lost_ticks = 0U;
    uint8_t speed_level = TRACK_DEFAULT_SPEED_LEVEL;
    int8_t error = 0;
    int8_t previous_error = 0;
    int8_t last_line_direction = 0;
    int8_t curve_recovery_direction = 0;
    uint8_t curve_memory_ticks = 0U;
    int8_t center_bias = 0;
    int8_t previous_motion_mode = 0;
    uint16_t previous_left_count = 0U;
    uint16_t previous_right_count = 0U;
    uint16_t heartbeat_ms = 0U;
    uint16_t display_ms = 0U;
    uint32_t route_distance = 0U;
    int16_t ramped_left_target = 0;
    int16_t ramped_right_target = 0;
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

        line_sensor_read_window(raw_states);
        line_sensor_filter_update(raw_states,
                                  filtered_states,
                                  &dropout_ticks);
        error = select_line_group(
            filtered_states,
            (tracking != 0U) ?
            (int8_t)(previous_error + center_bias) : 0,
            selected_states,
            &active_count);

        if(tracking == 0U)
        {
            motor_all_stop();

            if((key_edges & KEY_SW1_MASK) != 0U)
            {
                if(speed_level > 0U)
                {
                    speed_level--;
                }
            }

            if((key_edges & KEY_SW2_MASK) != 0U)
            {
                if(speed_level < (TRACK_SPEED_LEVEL_COUNT - 1U))
                {
                    speed_level++;
                }
            }

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
                    /*
                     * The four digital sensors have a fixed symmetric
                     * geometry.  Capturing a single S2/S3 reading as the
                     * centre would turn E=-5 or E=+5 into a permanent bias,
                     * so always use the physical centre as zero.
                     */
                    center_bias = 0;
                    previous_error = 0;
                    last_line_direction = 0;
                    curve_recovery_direction = 0;
                    curve_memory_ticks = 0U;
                    previous_motion_mode = 0;
                    lost_ticks = 0U;
                    display_ms = 0U;
                    ramped_left_target = 0;
                    ramped_right_target = 0;
                    encoder_reset();
                    previous_left_count = 0U;
                    previous_right_count = 0U;
                    route_distance = 0U;
                    speed_pi_reset(&left_pi, 0);
                    speed_pi_reset(&right_pi, 0);
                    motor_all_stop();
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
                                 0,
                                 0,
                                 (uint8_t)'N',
                                 speed_level,
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
                int16_t base_target =
                    track_base_targets[speed_level];
                int16_t left_target;
                int16_t right_target;
                int16_t left_measured;
                int16_t right_measured;
                int16_t left_pwm;
                int16_t right_pwm;
                uint16_t current_left;
                uint16_t current_right;
                int8_t motion_mode = 0;
                int8_t search_spin_direction = 0;
                uint8_t display_mode = (uint8_t)'N';

                if(active_count == 0U)
                {
                    lost_ticks++;
                    control_error = previous_error;

                    if((curve_memory_ticks > 0U) &&
                       (curve_recovery_direction > 0))
                    {
                        if(lost_ticks >=
                           TRACK_SEARCH_SPIN_DELAY_TICKS)
                        {
                            left_target = 0;
                            right_target = 0;
                            search_spin_direction = 1;
                            motion_mode = 3;
                        }
                        else
                        {
                            left_target =
                                track_edge_targets[speed_level];
                            right_target = TRACK_EDGE_INNER_TARGET;
                            motion_mode = 1;
                        }
                        display_mode = (uint8_t)'R';
                    }
                    else if((curve_memory_ticks > 0U) &&
                            (curve_recovery_direction < 0))
                    {
                        if(lost_ticks >=
                           TRACK_SEARCH_SPIN_DELAY_TICKS)
                        {
                            left_target = 0;
                            right_target = 0;
                            search_spin_direction = -1;
                            motion_mode = -3;
                        }
                        else
                        {
                            left_target = TRACK_EDGE_INNER_TARGET;
                            right_target =
                                track_edge_targets[speed_level];
                            motion_mode = -1;
                        }
                        display_mode = (uint8_t)'L';
                    }
                    else if(last_line_direction > 0)
                    {
                        left_target =
                            track_base_targets[speed_level];
                        right_target = TRACK_GENTLE_LOST_INNER;
                        motion_mode = 0;
                        display_mode = (uint8_t)'R';
                    }
                    else if(last_line_direction < 0)
                    {
                        left_target = TRACK_GENTLE_LOST_INNER;
                        right_target =
                            track_base_targets[speed_level];
                        motion_mode = 0;
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
                    if(control_error > 0)
                    {
                        last_line_direction = 1;
                    }
                    else if(control_error < 0)
                    {
                        last_line_direction = -1;
                    }

                    if(control_error >= 2)
                    {
                        curve_recovery_direction = 1;
                        curve_memory_ticks =
                            TRACK_CURVE_MEMORY_TICKS;
                    }
                    else if(control_error <= -2)
                    {
                        curve_recovery_direction = -1;
                        curve_memory_ticks =
                            TRACK_CURVE_MEMORY_TICKS;
                    }
                    else if(curve_memory_ticks > 0U)
                    {
                        curve_memory_ticks--;
                        if(curve_memory_ticks == 0U)
                        {
                            curve_recovery_direction = 0;
                        }
                    }

                    if((active_count == 1U) &&
                       (selected_states[0] != 0U))
                    {
                        left_target = TRACK_EDGE_INNER_TARGET;
                        right_target =
                            track_edge_targets[speed_level];
                        motion_mode = -1;
                        display_mode = (uint8_t)'L';
                    }
                    else if((active_count == 1U) &&
                            (selected_states[3] != 0U))
                    {
                        left_target =
                            track_edge_targets[speed_level];
                        right_target = TRACK_EDGE_INNER_TARGET;
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

                        if(magnitude >= 6)
                        {
                            base_target =
                                track_sharp_targets[speed_level];
                        }
                        else if(magnitude >= 2)
                        {
                            base_target =
                                track_medium_targets[speed_level];
                        }

                        /*
                         * Use the official discrete error scale.  E=+/-1 is
                         * an inner-sensor correction at full straight speed;
                         * E=+/-2 is an entering-bend correction at reduced
                         * speed; E=+/-6 is the outer-sensor recovery state.
                         */
                        if(magnitude < 2)
                        {
                            turn = (int16_t)(
                                ((3 * control_error) *
                                 track_turn_scale_tenths[speed_level]) /
                                10);
                        }
                        else if(magnitude < 6)
                        {
                            turn = (int16_t)(
                                ((5 * control_error) *
                                 track_turn_scale_tenths[speed_level]) /
                                10);
                        }
                        else
                        {
                            turn = (int16_t)(
                                ((5 * control_error) *
                                 track_turn_scale_tenths[speed_level]) /
                                10);
                        }
                        turn = clamp_turn(
                            turn,
                            track_turn_limits[speed_level]);

                        left_target = clamp_target(
                            (int16_t)(base_target + turn));
                        right_target = clamp_target(
                            (int16_t)(base_target - turn));
                    }
                }

                if((motion_mode == 1) || (motion_mode == -1))
                {
                    /*
                     * The line is already on an outer sensor or temporarily
                     * lost in a known direction.  Apply the forward-only
                     * recovery differential immediately; waiting for the
                     * normal deceleration ramp adds about 80 ms of outward
                     * travel at SPD0.
                     */
                    ramped_left_target = left_target;
                    ramped_right_target = right_target;
                }
                else if((motion_mode == 3) ||
                        (motion_mode == -3))
                {
                    /* Keep the previous forward ramp while pivot-searching. */
                }
                else
                {
                    left_target = slew_target(ramped_left_target,
                                              left_target);
                    right_target = slew_target(ramped_right_target,
                                               right_target);
                    ramped_left_target = left_target;
                    ramped_right_target = right_target;
                }

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

                    if(search_spin_direction == 0)
                    {
                        int16_t left_distance_delta = left_measured;
                        int16_t right_distance_delta = right_measured;

                        if(left_distance_delta < 0)
                        {
                            left_distance_delta =
                                (int16_t)(-left_distance_delta);
                        }
                        if(right_distance_delta < 0)
                        {
                            right_distance_delta =
                                (int16_t)(-right_distance_delta);
                        }
                        route_distance += (uint32_t)(
                            (left_distance_delta +
                             right_distance_delta) / 2);
                    }

                    if(search_spin_direction > 0)
                    {
                        speed_pi_reset(&left_pi, 0);
                        speed_pi_reset(&right_pi, 0);
                        left_pwm = TRACK_SEARCH_SPIN_PWM;
                        right_pwm = -TRACK_SEARCH_SPIN_PWM;
                    }
                    else if(search_spin_direction < 0)
                    {
                        speed_pi_reset(&left_pi, 0);
                        speed_pi_reset(&right_pi, 0);
                        left_pwm = -TRACK_SEARCH_SPIN_PWM;
                        right_pwm = TRACK_SEARCH_SPIN_PWM;
                    }
                    else
                    {
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
                                         left_measured,
                                         right_measured,
                                         display_mode,
                                         speed_level,
                                         1U,
                                         route_distance);
                        display_ms = 0U;
                    }
                }
            }
        }

        delay_ms(LINE_SAMPLE_REMAINDER_MS);
        heartbeat_ms =
            (uint16_t)(heartbeat_ms + TRACK_CONTROL_PERIOD_MS);
        if(heartbeat_ms >= 500U)
        {
            GPIO_ToggleBits(GPIOB, GPIO_Pin_2);
            heartbeat_ms = 0U;
        }
    }
}

