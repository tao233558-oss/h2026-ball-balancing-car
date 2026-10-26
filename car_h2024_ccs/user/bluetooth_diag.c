#include "headfile.h"
#include "bluetooth_diag.h"

#define BT_RX_RING_SIZE 128U
#define BT_FRAME_SIZE   100U

static volatile uint8_t g_rx_ring[BT_RX_RING_SIZE];
static volatile uint8_t g_rx_head;
static volatile uint8_t g_rx_tail;
static volatile uint16_t g_rx_overflow_count;

static char g_frame[BT_FRAME_SIZE];
static uint8_t g_frame_length;
static uint8_t g_in_frame;
static uint32_t g_rx_count;
static uint16_t g_packet_count;
static uint8_t g_light_on;

static void bt_send_text(const char *text)
{
    while(*text != '\0')
    {
        uart_sendbyte(UART3, (uint8_t) *text++);
    }
}

static uint8_t bt_pop_byte(uint8_t *byte)
{
    uint8_t tail = g_rx_tail;

    if(tail == g_rx_head)
    {
        return 0U;
    }
    *byte = g_rx_ring[tail];
    g_rx_tail = (uint8_t) ((tail + 1U) & (BT_RX_RING_SIZE - 1U));
    return 1U;
}

static void bt_show_packet(void)
{
    char display[17];
    uint8_t i;

    for(i = 0U; i < 16U; i++)
    {
        char ch = g_frame[i];

        if(i >= g_frame_length)
        {
            display[i] = ' ';
        }
        else if(ch >= 32 && ch <= 126)
        {
            display[i] = ch;
        }
        else
        {
            display[i] = '.';
        }
    }
    display[16] = '\0';

    OLED_ShowString(2, 1, "RX:     PK:    ");
    OLED_ShowNum(2, 4, g_rx_count % 100000U, 5);
    OLED_ShowNum(2, 13, g_packet_count % 1000U, 3);
    OLED_ShowString(3, 1, display);
    OLED_ShowString(4, 1, g_light_on ? "LIGHT ON  K2 OFF" :
                                           "K1 ON  LIGHT OFF");
}

static void bt_handle_packet(void)
{
    g_frame[g_frame_length] = '\0';
    g_packet_count++;

    if(strcmp(g_frame, "key,1,up") == 0)
    {
        g_light_on = 1U;
        gpio_set(GPIOB, DL_GPIO_PIN_13, 1U);
    }
    else if(strcmp(g_frame, "key,2,down") == 0)
    {
        g_light_on = 0U;
        gpio_set(GPIOB, DL_GPIO_PIN_13, 0U);
    }

    /* Match the STM32 example: reply with payload, without [ and ]. */
    bt_send_text(g_frame);
    bt_send_text("\r\n");
    bt_show_packet();
}

void bluetooth_diag_init(void)
{
    g_rx_head = 0U;
    g_rx_tail = 0U;
    g_rx_overflow_count = 0U;
    g_frame_length = 0U;
    g_in_frame = 0U;
    g_rx_count = 0U;
    g_packet_count = 0U;
    g_light_on = 0U;

    uart_init(UART3, 9600U, 1U);
    OLED_Clear();
    OLED_ShowString(1, 1, "BT UART3 9600   ");
    OLED_ShowString(2, 1, "RX:00000 PK:000 ");
    OLED_ShowString(3, 1, "WAIT [packet]   ");
    OLED_ShowString(4, 1, "K1 ON  LIGHT OFF");
    bt_send_text("MSPM0 BT READY\r\n");
}

void bluetooth_diag_service(void)
{
    uint8_t byte;

    motor_set_standby(0U);

    while(bt_pop_byte(&byte))
    {
        g_rx_count++;
        if(byte == '[')
        {
            g_in_frame = 1U;
            g_frame_length = 0U;
        }
        else if(g_in_frame && byte == ']')
        {
            g_in_frame = 0U;
            bt_handle_packet();
        }
        else if(g_in_frame)
        {
            if(g_frame_length < (BT_FRAME_SIZE - 1U))
            {
                g_frame[g_frame_length++] = (char) byte;
            }
            else
            {
                g_in_frame = 0U;
                g_frame_length = 0U;
                bt_send_text("ERR FRAME TOO LONG\r\n");
            }
        }
    }

    if(g_rx_overflow_count != 0U)
    {
        g_rx_overflow_count = 0U;
        bt_send_text("ERR RX OVERFLOW\r\n");
    }
}

void bluetooth_diag_rx_isr(uint8_t byte)
{
    uint8_t next = (uint8_t) ((g_rx_head + 1U) &
                              (BT_RX_RING_SIZE - 1U));

    if(next == g_rx_tail)
    {
        g_rx_overflow_count++;
        return;
    }
    g_rx_ring[g_rx_head] = byte;
    g_rx_head = next;
}
