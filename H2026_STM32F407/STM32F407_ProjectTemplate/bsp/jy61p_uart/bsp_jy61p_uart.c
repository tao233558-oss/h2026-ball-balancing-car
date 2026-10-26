#include "bsp_jy61p_uart.h"

#include "bsp_timebase.h"

#define JY61P_FRAME_LENGTH       11U
#define JY61P_FRAME_HEADER       0x55U
#define JY61P_ANGLE_FRAME_TYPE   0x53U

static volatile jy61p_sample_t g_sample;
static uint8_t g_frame[JY61P_FRAME_LENGTH];
static uint8_t g_frame_index;

static int16_t read_i16_le(const uint8_t *bytes)
{
    return (int16_t)((uint16_t)bytes[0] |
                     ((uint16_t)bytes[1] << 8U));
}

static int16_t angle_raw_to_cdeg(int16_t raw)
{
    return (int16_t)(((int32_t)raw * 18000L) / 32768L);
}

static void parser_reset_with_possible_header(uint8_t byte)
{
    if(byte == JY61P_FRAME_HEADER)
    {
        g_frame[0] = byte;
        g_frame_index = 1U;
    }
    else
    {
        g_frame_index = 0U;
    }
}

static void parser_push(uint8_t byte)
{
    uint8_t index;
    uint8_t checksum = 0U;

    if(g_frame_index == 0U)
    {
        parser_reset_with_possible_header(byte);
        return;
    }

    g_frame[g_frame_index] = byte;
    g_frame_index++;

    if(g_frame_index < JY61P_FRAME_LENGTH)
    {
        return;
    }

    for(index = 0U; index < (JY61P_FRAME_LENGTH - 1U); index++)
    {
        checksum = (uint8_t)(checksum + g_frame[index]);
    }

    if(checksum == g_frame[JY61P_FRAME_LENGTH - 1U])
    {
        if(g_frame[1] == JY61P_ANGLE_FRAME_TYPE)
        {
            g_sample.roll_cdeg =
                angle_raw_to_cdeg(read_i16_le(&g_frame[2]));
            g_sample.pitch_cdeg =
                angle_raw_to_cdeg(read_i16_le(&g_frame[4]));
            g_sample.yaw_cdeg =
                angle_raw_to_cdeg(read_i16_le(&g_frame[6]));
            g_sample.last_angle_ms = timebase_millis();
            g_sample.angle_frame_count++;
            g_sample.valid = 1U;
        }
        g_frame_index = 0U;
    }
    else
    {
        g_sample.checksum_error_count++;
        parser_reset_with_possible_header(byte);
    }
}

void jy61p_uart_init(uint32_t baud_rate)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef uart;
    NVIC_InitTypeDef nvic;

    g_sample.roll_cdeg = 0;
    g_sample.pitch_cdeg = 0;
    g_sample.yaw_cdeg = 0;
    g_sample.angle_frame_count = 0U;
    g_sample.checksum_error_count = 0U;
    g_sample.uart_error_count = 0U;
    g_sample.last_angle_ms = 0U;
    g_sample.valid = 0U;
    g_frame_index = 0U;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_11;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOC, &gpio);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_UART4);

    USART_StructInit(&uart);
    uart.USART_BaudRate = baud_rate;
    uart.USART_WordLength = USART_WordLength_8b;
    uart.USART_StopBits = USART_StopBits_1;
    uart.USART_Parity = USART_Parity_No;
    uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    uart.USART_Mode = USART_Mode_Rx;
    USART_Init(UART4, &uart);

    USART_ClearFlag(UART4, USART_FLAG_RXNE);
    USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);

    nvic.NVIC_IRQChannel = UART4_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2U;
    nvic.NVIC_IRQChannelSubPriority = 1U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    USART_Cmd(UART4, ENABLE);
}

uint8_t jy61p_get_sample(jy61p_sample_t *sample)
{
    uint8_t valid;

    if(sample == 0)
    {
        return 0U;
    }

    NVIC_DisableIRQ(UART4_IRQn);
    *sample = g_sample;
    valid = g_sample.valid;
    NVIC_EnableIRQ(UART4_IRQn);
    return valid;
}

uint32_t jy61p_sample_age_ms(uint32_t now_ms)
{
    uint32_t last_angle_ms;
    uint8_t valid;

    NVIC_DisableIRQ(UART4_IRQn);
    last_angle_ms = g_sample.last_angle_ms;
    valid = g_sample.valid;
    NVIC_EnableIRQ(UART4_IRQn);

    if(valid == 0U)
    {
        return 0xFFFFFFFFUL;
    }
    return now_ms - last_angle_ms;
}

void UART4_IRQHandler(void)
{
    if(USART_GetITStatus(UART4, USART_IT_RXNE) != RESET)
    {
        parser_push((uint8_t)USART_ReceiveData(UART4));
    }
    else if(USART_GetFlagStatus(UART4, USART_FLAG_ORE) != RESET)
    {
        volatile uint16_t discarded = USART_ReceiveData(UART4);
        (void)discarded;
        g_sample.uart_error_count++;
        g_frame_index = 0U;
    }
}
