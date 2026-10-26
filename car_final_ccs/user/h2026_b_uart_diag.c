#include "headfile.h"
#include "h2026_b_uart_diag.h"

#define H2026_B_STEPPER_SLEEP_PORT GPIOA
#define H2026_B_STEPPER_SLEEP_PIN  DL_GPIO_PIN_7
#define H2026_B_STEPPER_SLEEP_IOMUX PA7
#define H2026_B_STEPPER_STEP_PORT  GPIOA
#define H2026_B_STEPPER_STEP_PIN   DL_GPIO_PIN_12
#define H2026_B_STEPPER_STEP_IOMUX PA12
#define H2026_B_STEPPER_DIR_PORT   GPIOA
#define H2026_B_STEPPER_DIR_PIN    DL_GPIO_PIN_13
#define H2026_B_STEPPER_DIR_IOMUX  PA13

#define H2026_B_SW1_PORT           GPIOA
#define H2026_B_SW1_PIN            DL_GPIO_PIN_15
#define H2026_B_SW1_IOMUX          PA15
#define H2026_B_SW2_PORT           GPIOA
#define H2026_B_SW2_PIN            DL_GPIO_PIN_16
#define H2026_B_SW2_IOMUX          PA16
#define H2026_B_SW4_PORT           GPIOA
#define H2026_B_SW4_PIN            DL_GPIO_PIN_17
#define H2026_B_SW4_IOMUX          PA17

#define H2026_B_UART_BAUD_RATE     115200U
#define H2026_B_RX_RING_SIZE       512U
#define H2026_B_RX_RING_MASK       (H2026_B_RX_RING_SIZE - 1U)
#define H2026_B_DISPLAY_PERIOD_MS  200U
#define H2026_B_MAX_DISPLAY_AGE_MS 999U
#define H2026_B_TEXT_PACKET_MAX     40U
#define H2026_B_CAL_SAMPLE_COUNT    10U
#define H2026_B_CAL_SAMPLE_CAPACITY H2026_B_CAL_SAMPLE_COUNT
#define H2026_B_CAL_MIN_CONFIDENCE  40U
#define H2026_B_CAL_MIN_SIDE_PX     30
#define H2026_B_PIPE_HALF_LENGTH_MM 125
#define H2026_B_KEY_DEBOUNCE_MS     20U
#define H2026_TEXT_FLAG_MODEL_READY  (1U << 4)
#define H2026_TEXT_FLAG_CAMERA_READY (1U << 5)

typedef enum
{
    H2026_B_CAL_IDLE = 0,
    H2026_B_CAL_LEFT_WAIT,
    H2026_B_CAL_LEFT_SAMPLE,
    H2026_B_CAL_CENTER_WAIT,
    H2026_B_CAL_CENTER_SAMPLE,
    H2026_B_CAL_RIGHT_WAIT,
    H2026_B_CAL_RIGHT_SAMPLE,
    H2026_B_CAL_PASS,
    H2026_B_CAL_FAIL_ORDER,
    H2026_B_CAL_FAIL_SPAN
} h2026_b_cal_state_t;

typedef struct
{
    uint8_t stable_level;
    uint8_t candidate_level;
    uint8_t candidate_ticks;
    uint8_t pressed_event;
} h2026_b_button_t;

/*
 * UART3 IRQ only places bytes in this ring. Text packet parsing happens in
 * the main loop, so the ISR remains short.
 */
static volatile uint8_t g_h2026_b_rx_ring[H2026_B_RX_RING_SIZE];
static volatile uint16_t g_h2026_b_rx_head;
static volatile uint16_t g_h2026_b_rx_tail;
static volatile uint32_t g_h2026_b_rx_overflow_count;

static int16_t g_h2026_b_ball_x_px;
static uint8_t g_h2026_b_confidence;
static uint8_t g_h2026_b_flags;
static uint32_t g_h2026_b_last_frame_ms;
static uint8_t g_h2026_b_have_frame;
static char g_h2026_b_text_packet[H2026_B_TEXT_PACKET_MAX];
static uint8_t g_h2026_b_text_length;
static uint8_t g_h2026_b_text_receiving;
static uint32_t g_h2026_b_text_ok_count;
static uint32_t g_h2026_b_text_bad_count;

static h2026_b_cal_state_t g_h2026_b_cal_state;
static h2026_b_cal_state_t g_h2026_b_screen_state;
static int16_t g_h2026_b_cal_left_px;
static int16_t g_h2026_b_cal_center_px;
static int16_t g_h2026_b_cal_right_px;
static int16_t g_h2026_b_cal_samples[H2026_B_CAL_SAMPLE_CAPACITY];
static uint8_t g_h2026_b_cal_sample_head;
static uint8_t g_h2026_b_cal_sample_count;
static h2026_b_button_t g_h2026_b_sw1;
static h2026_b_button_t g_h2026_b_sw2;
static h2026_b_button_t g_h2026_b_sw4;

static int32_t h2026_b_limit_i32(
    int32_t value,
    int32_t minimum,
    int32_t maximum)
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

static void h2026_b_button_init(h2026_b_button_t *button)
{
    button->stable_level = 1U;
    button->candidate_level = 1U;
    button->candidate_ticks = 0U;
    button->pressed_event = 0U;
}

static void h2026_b_button_update(
    h2026_b_button_t *button,
    uint8_t raw_level)
{
    button->pressed_event = 0U;

    if(raw_level != button->candidate_level)
    {
        button->candidate_level = raw_level;
        button->candidate_ticks = 1U;
        return;
    }

    if(button->candidate_ticks < H2026_B_KEY_DEBOUNCE_MS)
    {
        button->candidate_ticks++;
    }

    if(button->candidate_ticks >= H2026_B_KEY_DEBOUNCE_MS &&
        button->stable_level != button->candidate_level)
    {
        button->stable_level = button->candidate_level;
        if(button->stable_level == 0U)
        {
            button->pressed_event = 1U;
        }
    }
}

static void h2026_b_scan_buttons(void)
{
    h2026_b_button_update(
        &g_h2026_b_sw1,
        (uint8_t) gpio_get(H2026_B_SW1_PORT, H2026_B_SW1_PIN));
    h2026_b_button_update(
        &g_h2026_b_sw2,
        (uint8_t) gpio_get(H2026_B_SW2_PORT, H2026_B_SW2_PIN));
    h2026_b_button_update(
        &g_h2026_b_sw4,
        (uint8_t) gpio_get(H2026_B_SW4_PORT, H2026_B_SW4_PIN));
}

static void h2026_b_init_button_gpio(void)
{
    gpio_init(
        H2026_B_SW1_PORT,
        H2026_B_SW1_PIN,
        H2026_B_SW1_IOMUX,
        IN_UP);
    gpio_init(
        H2026_B_SW2_PORT,
        H2026_B_SW2_PIN,
        H2026_B_SW2_IOMUX,
        IN_UP);
    gpio_init(
        H2026_B_SW4_PORT,
        H2026_B_SW4_PIN,
        H2026_B_SW4_IOMUX,
        IN_UP);
}

static void h2026_b_cal_clear_samples(void)
{
    g_h2026_b_cal_sample_head = 0U;
    g_h2026_b_cal_sample_count = 0U;
}

static uint8_t h2026_b_cal_sample_is_usable(void)
{
    uint8_t ready_mask =
        H2026_TEXT_FLAG_MODEL_READY | H2026_TEXT_FLAG_CAMERA_READY;

    return (uint8_t) (
        g_h2026_b_have_frame != 0U &&
        g_h2026_b_confidence >= H2026_B_CAL_MIN_CONFIDENCE &&
        (g_h2026_b_flags & ready_mask) == ready_mask);
}

static void h2026_b_cal_push_sample(int16_t ball_x_px)
{
    if(g_h2026_b_cal_state != H2026_B_CAL_LEFT_SAMPLE &&
        g_h2026_b_cal_state != H2026_B_CAL_CENTER_SAMPLE &&
        g_h2026_b_cal_state != H2026_B_CAL_RIGHT_SAMPLE)
    {
        return;
    }

    g_h2026_b_cal_samples[g_h2026_b_cal_sample_head] = ball_x_px;
    g_h2026_b_cal_sample_head =
        (uint8_t) ((g_h2026_b_cal_sample_head + 1U) %
            H2026_B_CAL_SAMPLE_CAPACITY);

    g_h2026_b_cal_sample_count++;
}

static uint8_t h2026_b_cal_get_median(int16_t *median_px)
{
    int16_t sorted[H2026_B_CAL_SAMPLE_CAPACITY];
    uint8_t i;

    if(g_h2026_b_cal_sample_count < H2026_B_CAL_SAMPLE_COUNT)
    {
        return 0U;
    }

    for(i = 0U; i < g_h2026_b_cal_sample_count; i++)
    {
        sorted[i] = g_h2026_b_cal_samples[i];
    }

    for(i = 1U; i < g_h2026_b_cal_sample_count; i++)
    {
        int16_t value = sorted[i];
        uint8_t j = i;

        while(j > 0U && sorted[j - 1U] > value)
        {
            sorted[j] = sorted[j - 1U];
            j--;
        }
        sorted[j] = value;
    }

    *median_px = (int16_t) (
        ((int32_t) sorted[(H2026_B_CAL_SAMPLE_COUNT / 2U) - 1U] +
         (int32_t) sorted[H2026_B_CAL_SAMPLE_COUNT / 2U]) / 2);
    return 1U;
}

static void h2026_b_cal_start(void)
{
    g_h2026_b_cal_state = H2026_B_CAL_LEFT_WAIT;
    h2026_b_cal_clear_samples();
}

static void h2026_b_cal_begin_sampling(void)
{
    if(g_h2026_b_cal_state == H2026_B_CAL_LEFT_WAIT)
    {
        g_h2026_b_cal_state = H2026_B_CAL_LEFT_SAMPLE;
        h2026_b_cal_clear_samples();
    }
    else if(g_h2026_b_cal_state == H2026_B_CAL_CENTER_WAIT)
    {
        g_h2026_b_cal_state = H2026_B_CAL_CENTER_SAMPLE;
        h2026_b_cal_clear_samples();
    }
    else if(g_h2026_b_cal_state == H2026_B_CAL_RIGHT_WAIT)
    {
        g_h2026_b_cal_state = H2026_B_CAL_RIGHT_SAMPLE;
        h2026_b_cal_clear_samples();
    }
}

static void h2026_b_cal_finish_sampling(void)
{
    int16_t median_px;

    if(h2026_b_cal_get_median(&median_px) == 0U)
    {
        return;
    }

    if(g_h2026_b_cal_state == H2026_B_CAL_LEFT_SAMPLE)
    {
        g_h2026_b_cal_left_px = median_px;
        g_h2026_b_cal_state = H2026_B_CAL_CENTER_WAIT;
        h2026_b_cal_clear_samples();
    }
    else if(g_h2026_b_cal_state == H2026_B_CAL_CENTER_SAMPLE)
    {
        g_h2026_b_cal_center_px = median_px;
        g_h2026_b_cal_state = H2026_B_CAL_RIGHT_WAIT;
        h2026_b_cal_clear_samples();
    }
    else if(g_h2026_b_cal_state == H2026_B_CAL_RIGHT_SAMPLE)
    {
        int32_t left_delta;
        int32_t right_delta;
        int32_t left_span;
        int32_t right_span;

        g_h2026_b_cal_right_px = median_px;
        h2026_b_cal_clear_samples();
        left_delta =
            (int32_t) g_h2026_b_cal_center_px -
            g_h2026_b_cal_left_px;
        right_delta =
            (int32_t) g_h2026_b_cal_right_px -
            g_h2026_b_cal_center_px;
        left_span = (left_delta < 0) ? -left_delta : left_delta;
        right_span = (right_delta < 0) ? -right_delta : right_delta;

        /*
         * Both normal and mirrored camera coordinates are valid. The only
         * invalid order is a center point that is not between both ends.
         */
        if(!((left_delta > 0 && right_delta > 0) ||
             (left_delta < 0 && right_delta < 0)))
        {
            g_h2026_b_cal_state = H2026_B_CAL_FAIL_ORDER;
        }
        else if(left_span < H2026_B_CAL_MIN_SIDE_PX ||
            right_span < H2026_B_CAL_MIN_SIDE_PX)
        {
            g_h2026_b_cal_state = H2026_B_CAL_FAIL_SPAN;
        }
        else
        {
            g_h2026_b_cal_state = H2026_B_CAL_PASS;
        }
    }
}

static int16_t h2026_b_cal_px_to_mm(int16_t ball_x_px)
{
    int32_t position_mm;
    uint8_t ball_on_left_segment;

    if(g_h2026_b_cal_state != H2026_B_CAL_PASS)
    {
        return 0;
    }

    ball_on_left_segment = (uint8_t) (
        (g_h2026_b_cal_left_px < g_h2026_b_cal_center_px &&
         ball_x_px <= g_h2026_b_cal_center_px) ||
        (g_h2026_b_cal_left_px > g_h2026_b_cal_center_px &&
         ball_x_px >= g_h2026_b_cal_center_px));

    if(ball_on_left_segment != 0U)
    {
        position_mm =
            ((int32_t) ball_x_px - g_h2026_b_cal_center_px) *
                H2026_B_PIPE_HALF_LENGTH_MM /
            (g_h2026_b_cal_center_px - g_h2026_b_cal_left_px);
    }
    else
    {
        position_mm =
            ((int32_t) ball_x_px - g_h2026_b_cal_center_px) *
                H2026_B_PIPE_HALF_LENGTH_MM /
            (g_h2026_b_cal_right_px - g_h2026_b_cal_center_px);
    }

    return (int16_t) h2026_b_limit_i32(position_mm, -199, 199);
}

static void h2026_b_handle_buttons(void)
{
    if(g_h2026_b_sw1.pressed_event != 0U)
    {
        h2026_b_cal_start();
        return;
    }

    if(g_h2026_b_sw2.pressed_event != 0U)
    {
        h2026_b_cal_begin_sampling();
    }
}

static void h2026_b_accept_vision_measurement(
    int16_t ball_x_px,
    uint8_t confidence,
    uint8_t flags,
    uint32_t now_ms)
{
    g_h2026_b_ball_x_px = ball_x_px;
    g_h2026_b_confidence = confidence;
    g_h2026_b_flags = flags;
    g_h2026_b_last_frame_ms = now_ms;
    g_h2026_b_have_frame = 1U;

    if(h2026_b_cal_sample_is_usable() != 0U)
    {
        h2026_b_cal_push_sample(g_h2026_b_ball_x_px);
        if(g_h2026_b_cal_sample_count >= H2026_B_CAL_SAMPLE_COUNT)
        {
            h2026_b_cal_finish_sampling();
        }
    }
}

static uint8_t h2026_b_text_parse_decimal(
    const char **cursor,
    char terminator,
    int32_t *value)
{
    const char *text = *cursor;
    int32_t magnitude = 0;
    int32_t sign = 1;
    uint8_t digit_seen = 0U;

    if(*text == '-' || *text == '+')
    {
        if(*text == '-')
        {
            sign = -1;
        }
        text++;
    }

    while(*text >= '0' && *text <= '9')
    {
        digit_seen = 1U;
        magnitude = magnitude * 10 + (*text - '0');
        if(magnitude > 100000L)
        {
            return 0U;
        }
        text++;
    }

    if(digit_seen == 0U || *text != terminator)
    {
        return 0U;
    }

    *value = magnitude * sign;
    *cursor = (terminator == '\0') ? text : text + 1;
    return 1U;
}

static uint8_t h2026_b_text_parse_ball_packet(
    const char *packet,
    int16_t *ball_x_px,
    uint8_t *confidence,
    uint8_t *flags)
{
    const char *cursor;
    int32_t x_value;
    int32_t confidence_value;
    int32_t flags_value;

    if(packet[0] != 'b' || packet[1] != 'a' ||
        packet[2] != 'l' || packet[3] != 'l' ||
        packet[4] != ',')
    {
        return 0U;
    }

    cursor = &packet[5];
    if(h2026_b_text_parse_decimal(&cursor, ',', &x_value) == 0U ||
        h2026_b_text_parse_decimal(
            &cursor,
            ',',
            &confidence_value) == 0U ||
        h2026_b_text_parse_decimal(
            &cursor,
            '\0',
            &flags_value) == 0U)
    {
        return 0U;
    }

    if(x_value < -32768L || x_value > 32767L ||
        confidence_value < 0L || confidence_value > 100L ||
        flags_value < 0L || flags_value > 255L)
    {
        return 0U;
    }

    *ball_x_px = (int16_t) x_value;
    *confidence = (uint8_t) confidence_value;
    *flags = (uint8_t) flags_value;
    return 1U;
}

static void h2026_b_text_feed_byte(uint8_t byte, uint32_t now_ms)
{
    if(byte == (uint8_t) '[')
    {
        g_h2026_b_text_receiving = 1U;
        g_h2026_b_text_length = 0U;
        return;
    }

    if(g_h2026_b_text_receiving == 0U)
    {
        return;
    }

    if(byte == (uint8_t) ']')
    {
        int16_t ball_x_px;
        uint8_t confidence;
        uint8_t flags;

        g_h2026_b_text_packet[g_h2026_b_text_length] = '\0';
        g_h2026_b_text_receiving = 0U;

        if(h2026_b_text_parse_ball_packet(
            g_h2026_b_text_packet,
            &ball_x_px,
            &confidence,
            &flags) != 0U)
        {
            g_h2026_b_text_ok_count++;
            h2026_b_accept_vision_measurement(
                ball_x_px,
                confidence,
                flags,
                now_ms);
        }
        else
        {
            g_h2026_b_text_bad_count++;
        }
        return;
    }

    if(byte < 0x20U || byte > 0x7EU ||
        g_h2026_b_text_length >= (H2026_B_TEXT_PACKET_MAX - 1U))
    {
        g_h2026_b_text_receiving = 0U;
        g_h2026_b_text_length = 0U;
        g_h2026_b_text_bad_count++;
        return;
    }

    g_h2026_b_text_packet[g_h2026_b_text_length++] = (char) byte;
}

static void h2026_b_force_stepper_sleep(void)
{
    gpio_init(
        H2026_B_STEPPER_SLEEP_PORT,
        H2026_B_STEPPER_SLEEP_PIN,
        H2026_B_STEPPER_SLEEP_IOMUX,
        OUT);
    gpio_init(
        H2026_B_STEPPER_STEP_PORT,
        H2026_B_STEPPER_STEP_PIN,
        H2026_B_STEPPER_STEP_IOMUX,
        OUT);
    gpio_init(
        H2026_B_STEPPER_DIR_PORT,
        H2026_B_STEPPER_DIR_PIN,
        H2026_B_STEPPER_DIR_IOMUX,
        OUT);
    gpio_set(
        H2026_B_STEPPER_SLEEP_PORT,
        H2026_B_STEPPER_SLEEP_PIN |
            H2026_B_STEPPER_STEP_PIN |
            H2026_B_STEPPER_DIR_PIN,
        0U);
}

void h2026_b_uart_diag_on_uart3_byte(uint8_t byte)
{
    uint16_t head = g_h2026_b_rx_head;
    uint16_t next =
        (uint16_t) ((head + 1U) & H2026_B_RX_RING_MASK);

    if(next == g_h2026_b_rx_tail)
    {
        g_h2026_b_rx_overflow_count++;
        return;
    }

    g_h2026_b_rx_ring[head] = byte;
    g_h2026_b_rx_head = next;
}

static uint8_t h2026_b_rx_pop(uint8_t *byte)
{
    uint16_t tail = g_h2026_b_rx_tail;

    if(tail == g_h2026_b_rx_head)
    {
        return 0U;
    }

    *byte = g_h2026_b_rx_ring[tail];
    g_h2026_b_rx_tail =
        (uint16_t) ((tail + 1U) & H2026_B_RX_RING_MASK);
    return 1U;
}

static void h2026_b_process_rx(uint32_t now_ms)
{
    uint8_t byte;

    while(h2026_b_rx_pop(&byte) != 0U)
    {
        h2026_b_text_feed_byte(byte, now_ms);
    }
}

static void h2026_b_show_snapshot(uint32_t now_ms)
{
    uint8_t link_ok = (uint8_t) (
        g_h2026_b_have_frame != 0U &&
        (now_ms - g_h2026_b_last_frame_ms) <=
            H2026_B_MAX_DISPLAY_AGE_MS);

    if(g_h2026_b_screen_state != g_h2026_b_cal_state)
    {
        if(g_h2026_b_cal_state == H2026_B_CAL_IDLE)
        {
            OLED_ShowString(1U, 1U, "3PT CAL SW1     ");
            OLED_ShowString(2U, 1U, "P:+0000 C:000   ");
            OLED_ShowString(3U, 1U, "LINK:0          ");
            OLED_ShowString(4U, 1U, "SW1 START       ");
        }
        else if(g_h2026_b_cal_state == H2026_B_CAL_LEFT_WAIT)
        {
            OLED_ShowString(1U, 1U, "LEFT WAIT       ");
            OLED_ShowString(2U, 1U, "P:+0000 C:000   ");
            OLED_ShowString(3U, 1U, "LINK:0          ");
            OLED_ShowString(4U, 1U, "MOVE LEFT SW2   ");
        }
        else if(g_h2026_b_cal_state == H2026_B_CAL_LEFT_SAMPLE)
        {
            OLED_ShowString(1U, 1U, "LEFT SAMPLING   ");
            OLED_ShowString(2U, 1U, "P:+0000 C:000   ");
            OLED_ShowString(3U, 1U, "N:00/10 LINK:0  ");
            OLED_ShowString(4U, 1U, "HOLD BALL STILL ");
        }
        else if(g_h2026_b_cal_state == H2026_B_CAL_CENTER_WAIT)
        {
            OLED_ShowString(1U, 1U, "LEFT DONE       ");
            OLED_ShowString(2U, 1U, "P:+0000 C:000   ");
            OLED_ShowString(3U, 1U, "L:+0000 LINK:0  ");
            OLED_ShowString(4U, 1U, "MOVE CENTER SW2 ");
        }
        else if(g_h2026_b_cal_state == H2026_B_CAL_CENTER_SAMPLE)
        {
            OLED_ShowString(1U, 1U, "CENTER SAMPLING ");
            OLED_ShowString(2U, 1U, "P:+0000 C:000   ");
            OLED_ShowString(3U, 1U, "N:00/10 LINK:0  ");
            OLED_ShowString(4U, 1U, "HOLD BALL STILL ");
        }
        else if(g_h2026_b_cal_state == H2026_B_CAL_RIGHT_WAIT)
        {
            OLED_ShowString(1U, 1U, "CENTER DONE     ");
            OLED_ShowString(2U, 1U, "P:+0000 C:000   ");
            OLED_ShowString(3U, 1U, "C:+0000 LINK:0  ");
            OLED_ShowString(4U, 1U, "MOVE RIGHT SW2  ");
        }
        else if(g_h2026_b_cal_state == H2026_B_CAL_RIGHT_SAMPLE)
        {
            OLED_ShowString(1U, 1U, "RIGHT SAMPLING  ");
            OLED_ShowString(2U, 1U, "P:+0000 C:000   ");
            OLED_ShowString(3U, 1U, "N:00/10 LINK:0  ");
            OLED_ShowString(4U, 1U, "HOLD BALL STILL ");
        }
        else if(g_h2026_b_cal_state == H2026_B_CAL_PASS)
        {
            OLED_ShowString(1U, 1U, "CAL PASS X MM   ");
            OLED_ShowString(2U, 1U, "P:+0000 X:+0000 ");
            OLED_ShowString(3U, 1U, "L:+0000 C:+0000 ");
            OLED_ShowString(4U, 1U, "R:+0000 SW1:RST ");
        }
        else
        {
            OLED_ShowString(
                1U,
                1U,
                (g_h2026_b_cal_state == H2026_B_CAL_FAIL_ORDER) ?
                    "CAL FAIL ORDER  " : "CAL FAIL SPAN   ");
            OLED_ShowString(2U, 1U, "L:+0000 C:+0000 ");
            OLED_ShowString(3U, 1U, "R:+0000         ");
            OLED_ShowString(4U, 1U, "SW1 RETRY       ");
        }

        g_h2026_b_screen_state = g_h2026_b_cal_state;
    }

    if(g_h2026_b_cal_state == H2026_B_CAL_PASS)
    {
        OLED_ShowSignedNum(
            2U,
            3U,
            h2026_b_limit_i32(g_h2026_b_ball_x_px, -1999, 1999),
            4U);
        OLED_ShowSignedNum(
            2U,
            11U,
            h2026_b_cal_px_to_mm(g_h2026_b_ball_x_px),
            4U);
        OLED_ShowSignedNum(3U, 3U, g_h2026_b_cal_left_px, 4U);
        OLED_ShowSignedNum(3U, 11U, g_h2026_b_cal_center_px, 4U);
        OLED_ShowSignedNum(4U, 3U, g_h2026_b_cal_right_px, 4U);
    }
    else if(g_h2026_b_cal_state == H2026_B_CAL_FAIL_ORDER ||
        g_h2026_b_cal_state == H2026_B_CAL_FAIL_SPAN)
    {
        OLED_ShowSignedNum(2U, 3U, g_h2026_b_cal_left_px, 4U);
        OLED_ShowSignedNum(2U, 11U, g_h2026_b_cal_center_px, 4U);
        OLED_ShowSignedNum(3U, 3U, g_h2026_b_cal_right_px, 4U);
    }
    else
    {
        OLED_ShowSignedNum(
            2U,
            3U,
            h2026_b_limit_i32(g_h2026_b_ball_x_px, -1999, 1999),
            4U);
        OLED_ShowNum(2U, 11U, g_h2026_b_confidence, 3U);

        if(g_h2026_b_cal_state == H2026_B_CAL_LEFT_SAMPLE ||
            g_h2026_b_cal_state == H2026_B_CAL_CENTER_SAMPLE ||
            g_h2026_b_cal_state == H2026_B_CAL_RIGHT_SAMPLE)
        {
            OLED_ShowNum(
                3U,
                3U,
                g_h2026_b_cal_sample_count,
                2U);
            OLED_ShowNum(3U, 14U, link_ok, 1U);
        }
        else if(g_h2026_b_cal_state == H2026_B_CAL_CENTER_WAIT)
        {
            OLED_ShowSignedNum(3U, 3U, g_h2026_b_cal_left_px, 4U);
            OLED_ShowNum(3U, 14U, link_ok, 1U);
        }
        else if(g_h2026_b_cal_state == H2026_B_CAL_RIGHT_WAIT)
        {
            OLED_ShowSignedNum(3U, 3U, g_h2026_b_cal_center_px, 4U);
            OLED_ShowNum(3U, 14U, link_ok, 1U);
        }
        else
        {
            OLED_ShowNum(3U, 6U, link_ok, 1U);
        }
    }
}

void h2026_b_uart_diag_run(void)
{
    uint32_t now_ms = 0U;
    uint32_t next_display_ms = 0U;

    h2026_b_force_stepper_sleep();

    g_h2026_b_rx_head = 0U;
    g_h2026_b_rx_tail = 0U;
    g_h2026_b_rx_overflow_count = 0U;
    g_h2026_b_ball_x_px = 0;
    g_h2026_b_confidence = 0U;
    g_h2026_b_flags = 0U;
    g_h2026_b_last_frame_ms = 0U;
    g_h2026_b_have_frame = 0U;
    g_h2026_b_text_length = 0U;
    g_h2026_b_text_receiving = 0U;
    g_h2026_b_text_ok_count = 0U;
    g_h2026_b_text_bad_count = 0U;
    g_h2026_b_cal_state = H2026_B_CAL_IDLE;
    g_h2026_b_screen_state = (h2026_b_cal_state_t) 0xFFU;
    g_h2026_b_cal_left_px = 0;
    g_h2026_b_cal_center_px = 0;
    g_h2026_b_cal_right_px = 0;
    h2026_b_cal_clear_samples();
    h2026_b_button_init(&g_h2026_b_sw1);
    h2026_b_button_init(&g_h2026_b_sw2);
    h2026_b_button_init(&g_h2026_b_sw4);

    OLED_Init();
    OLED_Clear();
    OLED_ShowString(1U, 1U, "3PT CAL TEST    ");
    OLED_ShowString(2U, 1U, "SW1 START       ");
    OLED_ShowString(3U, 1U, "SW2 SAVE POINT  ");
    OLED_ShowString(4U, 1U, "STEP:SLEEP      ");
    delay_ms(1000U);

    h2026_b_init_button_gpio();
    uart_init(UART3, H2026_B_UART_BAUD_RATE, 1U);

    while(1)
    {
        h2026_b_process_rx(now_ms);
        h2026_b_scan_buttons();
        h2026_b_handle_buttons();

        if(now_ms >= next_display_ms)
        {
            h2026_b_show_snapshot(now_ms);
            next_display_ms = now_ms + H2026_B_DISPLAY_PERIOD_MS;
        }

        delay_ms(1U);
        now_ms++;
    }
}
