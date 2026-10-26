#ifndef __SERIAL_H__
#define __SERIAL_H__
#include "headfile.h"

void uart0_init(void);
void Read_Data2(uint8_t addr, uint32_t *Data);	     

extern volatile uint8_t k230_state;
extern volatile uint8_t k230_rx_ready;
extern volatile uint8_t k230_rx_len;
extern char k230_rx_msg[16];

#endif

