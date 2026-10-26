#include "headfile.h"
#include "track_diag.h"

#define TRACK_DIAG_REFRESH_TICKS 10U

static void track_diag_uart_puts(const char *text)
{
    while(*text != '\0')
    {
        uart_sendbyte(UART0, (uint8_t) *text++);
    }
}

static void track_diag_uart_hex(uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    uart_sendbyte(UART0, (uint8_t) hex[(value >> 4) & 0x0FU]);
    uart_sendbyte(UART0, (uint8_t) hex[value & 0x0FU]);
}

static void track_diag_uart_u16(uint16_t value)
{
    char buffer[5];
    uint8_t length = 0;

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

static void track_diag_uart_i16(int16_t value)
{
    if(value < 0)
    {
        uart_sendbyte(UART0, '-');
        value = (int16_t) -value;
    }
    else
    {
        uart_sendbyte(UART0, '+');
    }
    track_diag_uart_u16((uint16_t) value);
}

static void track_diag_uart_bits_d1_to_d8(uint8_t value)
{
    uint8_t channel;

    for(channel = 0U; channel < 8U; channel++)
    {
        uart_sendbyte(UART0, (value & (uint8_t) (1U << channel)) ? '1' : '0');
    }
}

static uint8_t track_diag_read_raw_pattern(void)
{
    uint8_t raw = 0U;
    uint8_t channel;

    for(channel = 1U; channel <= 8U; channel++)
    {
        if(digtal(channel))
        {
            raw |= (uint8_t) (1U << (channel - 1U));
        }
    }
    return raw;
}

static void track_diag_show_oled(uint8_t raw,
                                 uint8_t black_pattern,
                                 int16_t error,
                                 uint16_t corner_count)
{
    uint8_t channel;

    OLED_ShowString(1, 1, "TRACK DIAG SAFE ");
    OLED_ShowString(2, 1, "R:");
    OLED_ShowString(3, 1, "B:");
    for(channel = 0U; channel < 8U; channel++)
    {
        OLED_ShowChar(2, (uint8_t) (3U + channel),
                      (raw & (uint8_t) (1U << channel)) ? '1' : '0');
        OLED_ShowChar(3, (uint8_t) (3U + channel),
                      (black_pattern & (uint8_t) (1U << channel)) ? '1' : '0');
    }
    OLED_ShowString(4, 1, "                ");
    OLED_ShowString(4, 1, "E:");
    OLED_ShowSignedNum(4, 3, error, 3);
    OLED_ShowString(4, 8, "N:");
    OLED_ShowNum(4, 10, corner_count % 100U, 2);
}

static void track_diag_send_serial(uint8_t raw,
                                   uint8_t black_pattern,
                                   int16_t error,
                                   uint8_t corner_event,
                                   uint16_t corner_count)
{
    track_diag_uart_puts("TRACK R=0x");
    track_diag_uart_hex(raw);
    track_diag_uart_puts(" D1..D8=");
    track_diag_uart_bits_d1_to_d8(raw);
    track_diag_uart_puts(" B=0x");
    track_diag_uart_hex(black_pattern);
    track_diag_uart_puts(" D1..D8=");
    track_diag_uart_bits_d1_to_d8(black_pattern);
    track_diag_uart_puts(" E=");
    track_diag_uart_i16(error);
    track_diag_uart_puts(" EVT=");
    uart_sendbyte(UART0, corner_event ? '1' : '0');
    track_diag_uart_puts(" N=");
    track_diag_uart_u16(corner_count);
    track_diag_uart_puts("\r\n");
}

void track_diag_run(void)
{
    track_corner_filter_t corner_filter;
    uint16_t corner_count = 0U;
    uint8_t refresh_ticks = 0U;
    uint8_t pending_corner_event = 0U;

    /* Initialize the driver only to put every motor output into a known state. */
    motor_safe_init_disabled();
    Set_left_pwm(0);
    Set_right_pwm(0);
    motor_set_standby(0U);

    xunji_init();
    OLED_Init();
    OLED_Clear();
    uart0_init();
    track_corner_filter_init(&corner_filter);
    track_diag_uart_puts("TRACK DIAG SAFE: motors=0, PB16/STBY=0, order=D1..D8\r\n");

    while(1)
    {
        uint8_t raw;
        uint8_t black_pattern;
        uint8_t corner_event;
        int16_t error;

        /* Reassert the safe state every 10 ms. */
        Set_left_pwm(0);
        Set_right_pwm(0);
        motor_set_standby(0U);

        raw = track_diag_read_raw_pattern();
        black_pattern = track_read_pattern();
        error = (int16_t) track_error_from_pattern(black_pattern);
        corner_event = track_corner_filter_step(&corner_filter, black_pattern);
        if(corner_event && corner_count < 65535U)
        {
            corner_count++;
            pending_corner_event = 1U;
        }

        if(refresh_ticks == 0U)
        {
            track_diag_show_oled(raw, black_pattern, error, corner_count);
            track_diag_send_serial(raw, black_pattern, error,
                                   pending_corner_event, corner_count);
            pending_corner_event = 0U;
        }
        refresh_ticks++;
        if(refresh_ticks >= TRACK_DIAG_REFRESH_TICKS)
        {
            refresh_ticks = 0U;
        }
        delay_ms(10);
    }
}
