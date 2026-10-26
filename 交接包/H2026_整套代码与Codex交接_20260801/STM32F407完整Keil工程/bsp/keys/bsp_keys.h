#ifndef __BSP_KEYS_H__
#define __BSP_KEYS_H__

#include "stm32f4xx.h"

/*
 * Four external keys are active low:
 *   SW1 -> PE7
 *   SW2 -> PE8
 *   SW3 -> PE9
 *   SW4 -> PE10
 *
 * Each key is wired between its GPIO pin and GND.  The MCU internal pull-up
 * keeps an unpressed input high, so no external resistor is required.
 */
#define KEY_SW1_MASK  ((uint8_t)0x01U)
#define KEY_SW2_MASK  ((uint8_t)0x02U)
#define KEY_SW3_MASK  ((uint8_t)0x04U)
#define KEY_SW4_MASK  ((uint8_t)0x08U)

void keys_init(void);
uint8_t keys_read_pressed_mask(void);

#endif
