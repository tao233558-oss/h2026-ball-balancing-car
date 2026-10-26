#ifndef BSP_VISION_UART_H
#define BSP_VISION_UART_H

#include "board.h"

/*
 * K230 UART2_TX (3.3 V TTL) -> STM32 PA3 / USART2_RX.
 * This link is receive-only during the first integration stage.
 */

void vision_uart_init(uint32_t baud_rate);
uint8_t vision_uart_read_byte(uint8_t *byte);
uint32_t vision_uart_get_overflow_count(void);
uint16_t vision_uart_get_buffered_count(void);

void USART2_IRQHandler(void);

#endif
