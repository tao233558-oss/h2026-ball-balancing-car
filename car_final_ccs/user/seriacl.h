#ifndef __SERIAL_H__
#define __SERIAL_H__
#include <stdint.h>
#include "aim_protocol.h"

void uart0_init(void);
void Read_Data2(uint8_t addr, uint32_t *Data);	     

extern volatile uint8_t k230_state;
extern volatile uint8_t k230_rx_ready;
extern volatile uint8_t k230_rx_len;
extern char k230_rx_msg[16];

uint8_t k230_get_vision_sample(aim_vision_sample_t *sample);

#endif

