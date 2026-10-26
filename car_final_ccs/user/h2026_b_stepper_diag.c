#include "headfile.h"
#include "h2026_b_stepper_diag.h"

#define B_STEP_SLP_PORT       GPIOA
#define B_STEP_SLP_PIN        DL_GPIO_PIN_7
#define B_STEP_SLP_IOMUX      PA7
#define B_STEP_PULSE_PORT     GPIOA
#define B_STEP_PULSE_PIN      DL_GPIO_PIN_12
#define B_STEP_PULSE_IOMUX    PA12
#define B_STEP_DIR_PORT       GPIOA
#define B_STEP_DIR_PIN        DL_GPIO_PIN_13
#define B_STEP_DIR_IOMUX      PA13

#define B_STEP_KEY_CW_PORT    GPIOA
#define B_STEP_KEY_CW_PIN     DL_GPIO_PIN_15
#define B_STEP_KEY_CW_IOMUX   PA15
#define B_STEP_KEY_CCW_PORT   GPIOA
#define B_STEP_KEY_CCW_PIN    DL_GPIO_PIN_16
#define B_STEP_KEY_CCW_IOMUX  PA16
#define B_STEP_KEY_STOP_PORT  GPIOA
#define B_STEP_KEY_STOP_PIN   DL_GPIO_PIN_17
#define B_STEP_KEY_STOP_IOMUX PA17

#define B_STEP_PULSES_PER_MOVE 400U
#define B_STEP_WAKE_TICKS       2U
#define B_STEP_LOW_WAIT_TICKS   3U
#define B_STEP_KEY_DEBOUNCE_MS  20U
#define B_STEP_DISPLAY_MS       100U

typedef enum
{
    B_STEP_STATE_IDLE = 0,
    B_STEP_STATE_RUN_CW,
    B_STEP_STATE_RUN_CCW,
    B_STEP_STATE_DONE_CW,
    B_STEP_STATE_DONE_CCW,
    B_STEP_STATE_ABORTED
} b_step_state_t;

typedef struct
{
    uint8_t stable_level;
    uint8_t candidate_level;
    uint8_t candidate_ticks;
    uint8_t pressed_event;
} b_step_button_t;

static volatile b_step_state_t g_b_step_state;
static volatile uint8_t g_b_step_running;
static volatile uint8_t g_b_step_reverse;
static volatile uint8_t g_b_step_pulse_high;
static volatile uint8_t g_b_step_wake_ticks;
static volatile uint8_t g_b_step_low_wait_ticks;
static volatile uint16_t g_b_step_completed_pulses;
static volatile int32_t g_b_step_position;

static void b_step_safe_outputs(void)
{
    gpio_set(B_STEP_PULSE_PORT, B_STEP_PULSE_PIN, 0U);
    gpio_set(B_STEP_SLP_PORT, B_STEP_SLP_PIN, 0U);
}

static void b_step_button_init(
    b_step_button_t *button,
    uint8_t raw_level)
{
    button->stable_level = raw_level;
    button->candidate_level = raw_level;
    button->candidate_ticks = 0U;
    button->pressed_event = 0U;
}

static void b_step_button_update(
    b_step_button_t *button,
    uint8_t raw_level)
{
    button->pressed_event = 0U;

    if(raw_level != button->candidate_level)
    {
        button->candidate_level = raw_level;
        button->candidate_ticks = 1U;
        return;
    }

    if(button->candidate_ticks < B_STEP_KEY_DEBOUNCE_MS)
    {
        button->candidate_ticks++;
    }

    if(button->candidate_ticks >= B_STEP_KEY_DEBOUNCE_MS &&
        button->stable_level != button->candidate_level)
    {
        button->stable_level = button->candidate_level;
        if(button->stable_level == 0U)
        {
            button->pressed_event = 1U;
        }
    }
}

static void b_step_gpio_init(void)
{
    gpio_init(
        B_STEP_SLP_PORT,
        B_STEP_SLP_PIN,
        B_STEP_SLP_IOMUX,
        OUT);
    gpio_init(
        B_STEP_PULSE_PORT,
        B_STEP_PULSE_PIN,
        B_STEP_PULSE_IOMUX,
        OUT);
    gpio_init(
        B_STEP_DIR_PORT,
        B_STEP_DIR_PIN,
        B_STEP_DIR_IOMUX,
        OUT);
    gpio_init(
        B_STEP_KEY_CW_PORT,
        B_STEP_KEY_CW_PIN,
        B_STEP_KEY_CW_IOMUX,
        IN_UP);
    gpio_init(
        B_STEP_KEY_CCW_PORT,
        B_STEP_KEY_CCW_PIN,
        B_STEP_KEY_CCW_IOMUX,
        IN_UP);
    gpio_init(
        B_STEP_KEY_STOP_PORT,
        B_STEP_KEY_STOP_PIN,
        B_STEP_KEY_STOP_IOMUX,
        IN_UP);

    gpio_set(B_STEP_DIR_PORT, B_STEP_DIR_PIN, 0U);
    b_step_safe_outputs();
}

static void b_step_start(uint8_t reverse)
{
    NVIC_DisableIRQ(TIMG0_INT_IRQn);

    b_step_safe_outputs();
    g_b_step_running = 0U;
    g_b_step_reverse = reverse ? 1U : 0U;
    g_b_step_pulse_high = 0U;
    g_b_step_wake_ticks = B_STEP_WAKE_TICKS;
    g_b_step_low_wait_ticks = 0U;
    g_b_step_completed_pulses = 0U;

    gpio_set(
        B_STEP_DIR_PORT,
        B_STEP_DIR_PIN,
        g_b_step_reverse);
    gpio_set(B_STEP_SLP_PORT, B_STEP_SLP_PIN, 1U);

    g_b_step_state = g_b_step_reverse ?
        B_STEP_STATE_RUN_CCW : B_STEP_STATE_RUN_CW;
    g_b_step_running = 1U;

    NVIC_ClearPendingIRQ(TIMG0_INT_IRQn);
    NVIC_EnableIRQ(TIMG0_INT_IRQn);
}

static void b_step_abort(void)
{
    NVIC_DisableIRQ(TIMG0_INT_IRQn);
    g_b_step_running = 0U;
    g_b_step_pulse_high = 0U;
    b_step_safe_outputs();
    g_b_step_state = B_STEP_STATE_ABORTED;
    NVIC_ClearPendingIRQ(TIMG0_INT_IRQn);
    NVIC_EnableIRQ(TIMG0_INT_IRQn);
}

void h2026_b_stepper_diag_on_1ms_tick(void)
{
    if(g_b_step_running == 0U)
    {
        return;
    }

    if(g_b_step_wake_ticks > 0U)
    {
        g_b_step_wake_ticks--;
        return;
    }

    if(g_b_step_pulse_high != 0U)
    {
        gpio_set(B_STEP_PULSE_PORT, B_STEP_PULSE_PIN, 0U);
        g_b_step_pulse_high = 0U;
        g_b_step_completed_pulses++;
        g_b_step_position += g_b_step_reverse ? -1 : 1;

        if(g_b_step_completed_pulses >= B_STEP_PULSES_PER_MOVE)
        {
            g_b_step_running = 0U;
            gpio_set(B_STEP_SLP_PORT, B_STEP_SLP_PIN, 0U);
            g_b_step_state = g_b_step_reverse ?
                B_STEP_STATE_DONE_CCW : B_STEP_STATE_DONE_CW;
        }
        else
        {
            g_b_step_low_wait_ticks = B_STEP_LOW_WAIT_TICKS;
        }
        return;
    }

    if(g_b_step_low_wait_ticks > 0U)
    {
        g_b_step_low_wait_ticks--;
        return;
    }

    gpio_set(B_STEP_PULSE_PORT, B_STEP_PULSE_PIN, 1U);
    g_b_step_pulse_high = 1U;
}

static void b_step_draw_layout(b_step_state_t state)
{
    if(state == B_STEP_STATE_RUN_CW)
    {
        OLED_ShowString(1U, 1U, "RUN CW 200HZ    ");
        OLED_ShowString(4U, 1U, "SLP:1 SW4:STOP  ");
    }
    else if(state == B_STEP_STATE_RUN_CCW)
    {
        OLED_ShowString(1U, 1U, "RUN CCW 200HZ   ");
        OLED_ShowString(4U, 1U, "SLP:1 SW4:STOP  ");
    }
    else if(state == B_STEP_STATE_DONE_CW)
    {
        OLED_ShowString(1U, 1U, "DONE CW         ");
        OLED_ShowString(4U, 1U, "SLP:0 READY     ");
    }
    else if(state == B_STEP_STATE_DONE_CCW)
    {
        OLED_ShowString(1U, 1U, "DONE CCW        ");
        OLED_ShowString(4U, 1U, "SLP:0 READY     ");
    }
    else if(state == B_STEP_STATE_ABORTED)
    {
        OLED_ShowString(1U, 1U, "STOPPED         ");
        OLED_ShowString(4U, 1U, "SLP:0 READY     ");
    }
    else
    {
        OLED_ShowString(1U, 1U, "B STEP 200HZ    ");
        OLED_ShowString(4U, 1U, "SLP:0 SW4:STOP  ");
    }

    OLED_ShowString(2U, 1U, "P:000/400       ");
    OLED_ShowString(3U, 1U, "POS:+000000     ");
}

static void b_step_update_numbers(void)
{
    uint16_t pulses = g_b_step_completed_pulses;
    int32_t position = g_b_step_position;

    OLED_ShowNum(2U, 3U, pulses, 3U);
    OLED_ShowSignedNum(3U, 5U, position, 6U);
}

void h2026_b_stepper_diag_run(void)
{
    b_step_button_t sw1;
    b_step_button_t sw2;
    b_step_button_t sw4;
    b_step_state_t displayed_state = (b_step_state_t) 0xFFU;
    uint32_t now_ms = 0U;
    uint32_t next_display_ms = 0U;

    b_step_gpio_init();
    g_b_step_state = B_STEP_STATE_IDLE;
    g_b_step_running = 0U;
    g_b_step_reverse = 0U;
    g_b_step_pulse_high = 0U;
    g_b_step_wake_ticks = 0U;
    g_b_step_low_wait_ticks = 0U;
    g_b_step_completed_pulses = 0U;
    g_b_step_position = 0;

    b_step_button_init(
        &sw1,
        (uint8_t) gpio_get(B_STEP_KEY_CW_PORT, B_STEP_KEY_CW_PIN));
    b_step_button_init(
        &sw2,
        (uint8_t) gpio_get(B_STEP_KEY_CCW_PORT, B_STEP_KEY_CCW_PIN));
    b_step_button_init(
        &sw4,
        (uint8_t) gpio_get(B_STEP_KEY_STOP_PORT, B_STEP_KEY_STOP_PIN));

    OLED_Init();
    OLED_Clear();
    b_step_draw_layout(B_STEP_STATE_IDLE);
    b_step_update_numbers();

    tim_interrupt_ms_init(TIMG0, 1U, 0U);

    while(1)
    {
        b_step_button_update(
            &sw1,
            (uint8_t) gpio_get(B_STEP_KEY_CW_PORT, B_STEP_KEY_CW_PIN));
        b_step_button_update(
            &sw2,
            (uint8_t) gpio_get(B_STEP_KEY_CCW_PORT, B_STEP_KEY_CCW_PIN));
        b_step_button_update(
            &sw4,
            (uint8_t) gpio_get(B_STEP_KEY_STOP_PORT, B_STEP_KEY_STOP_PIN));

        if(sw4.pressed_event != 0U)
        {
            b_step_abort();
        }
        else if(g_b_step_running == 0U && sw1.pressed_event != 0U)
        {
            b_step_start(0U);
        }
        else if(g_b_step_running == 0U && sw2.pressed_event != 0U)
        {
            b_step_start(1U);
        }

        if(displayed_state != g_b_step_state)
        {
            displayed_state = g_b_step_state;
            b_step_draw_layout(displayed_state);
            b_step_update_numbers();
            next_display_ms = now_ms + B_STEP_DISPLAY_MS;
        }
        else if(now_ms >= next_display_ms)
        {
            b_step_update_numbers();
            next_display_ms = now_ms + B_STEP_DISPLAY_MS;
        }

        delay_ms(1U);
        now_ms++;
    }
}
