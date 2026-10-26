#include "bsp_vision_uart.h"

#define VISION_UART_BUFFER_SIZE  256U
#define VISION_UART_BUFFER_MASK  (VISION_UART_BUFFER_SIZE - 1U)

static volatile uint8_t g_rx_buffer[VISION_UART_BUFFER_SIZE];
static volatile uint16_t g_rx_head;
static volatile uint16_t g_rx_tail;
static volatile uint32_t g_rx_overflow_count;

void vision_uart_init(uint32_t baud_rate)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef uart;
    NVIC_InitTypeDef nvic;

    g_rx_head = 0U;
    g_rx_tail = 0U;
    g_rx_overflow_count = 0U;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_3;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_100MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &gpio);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

    USART_StructInit(&uart);
    uart.USART_BaudRate = baud_rate;
    uart.USART_WordLength = USART_WordLength_8b;
    uart.USART_StopBits = USART_StopBits_1;
    uart.USART_Parity = USART_Parity_No;
    uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    uart.USART_Mode = USART_Mode_Rx;
    USART_Init(USART2, &uart);

    USART_ClearFlag(USART2, USART_FLAG_RXNE);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    nvic.NVIC_IRQChannel = USART2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2U;
    nvic.NVIC_IRQChannelSubPriority = 0U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    USART_Cmd(USART2, ENABLE);
}

uint8_t vision_uart_read_byte(uint8_t *byte)
{
    uint16_t tail;

    if(g_rx_tail == g_rx_head)
    {
        return 0U;
    }

    tail = g_rx_tail;
    *byte = g_rx_buffer[tail];
    g_rx_tail = (uint16_t)((tail + 1U) & VISION_UART_BUFFER_MASK);
    return 1U;
}

uint32_t vision_uart_get_overflow_count(void)
{
    return g_rx_overflow_count;
}

uint16_t vision_uart_get_buffered_count(void)
{
    return (uint16_t)(
        (g_rx_head - g_rx_tail) & VISION_UART_BUFFER_MASK);
}

void USART2_IRQHandler(void)
{
    uint16_t next_head;
    uint8_t byte;

    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        byte = (uint8_t)USART_ReceiveData(USART2);
        next_head =
            (uint16_t)((g_rx_head + 1U) & VISION_UART_BUFFER_MASK);

        if(next_head == g_rx_tail)
        {
            g_rx_overflow_count++;
        }
        else
        {
            g_rx_buffer[g_rx_head] = byte;
            g_rx_head = next_head;
        }
    }
    else if(USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET)
    {
        volatile uint16_t discarded;
        discarded = USART_ReceiveData(USART2);
        (void)discarded;
        g_rx_overflow_count++;
    }
}
