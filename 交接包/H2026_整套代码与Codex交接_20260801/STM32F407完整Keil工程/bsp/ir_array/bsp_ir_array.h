#ifndef __BSP_IR_ARRAY_H__
#define __BSP_IR_ARRAY_H__

#include "stm32f4xx.h"

#define IR_ARRAY_CHANNEL_COUNT  8U

/*
 * Eight-channel analog infrared array:
 *   D1 -> PC0 / ADC1_IN10
 *   D2 -> PC1 / ADC1_IN11
 *   D3 -> PC2 / ADC1_IN12
 *   D4 -> PC3 / ADC1_IN13
 *   D5 -> PC4 / ADC1_IN14
 *   D6 -> PC5 / ADC1_IN15
 *   D7 -> PB0 / ADC1_IN8
 *   D8 -> PB1 / ADC1_IN9
 *   IR -> PE2 (high enables the emitters)
 */

void ir_array_init(void);
void ir_array_set_enabled(uint8_t enabled);
void ir_array_read_all(uint16_t values[IR_ARRAY_CHANNEL_COUNT]);

#endif
