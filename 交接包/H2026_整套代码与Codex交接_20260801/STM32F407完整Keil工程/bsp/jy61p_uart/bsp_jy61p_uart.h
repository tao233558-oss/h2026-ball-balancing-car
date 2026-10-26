#ifndef BSP_JY61P_UART_H
#define BSP_JY61P_UART_H

#include "board.h"

typedef struct
{
    int16_t roll_cdeg;
    int16_t pitch_cdeg;
    int16_t yaw_cdeg;
    uint32_t angle_frame_count;
    uint32_t checksum_error_count;
    uint32_t uart_error_count;
    uint32_t last_angle_ms;
    uint8_t valid;
} jy61p_sample_t;

/* JY61P TX -> PC11 / UART4_RX, default 9600 baud. */
void jy61p_uart_init(uint32_t baud_rate);
uint8_t jy61p_get_sample(jy61p_sample_t *sample);
uint32_t jy61p_sample_age_ms(uint32_t now_ms);

void UART4_IRQHandler(void);

#endif
