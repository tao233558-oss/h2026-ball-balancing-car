#ifndef BSP_TIMEBASE_H
#define BSP_TIMEBASE_H

#include "board.h"

void timebase_init_1ms(void);
uint32_t timebase_millis(void);
void TIM7_IRQHandler(void);

#endif
