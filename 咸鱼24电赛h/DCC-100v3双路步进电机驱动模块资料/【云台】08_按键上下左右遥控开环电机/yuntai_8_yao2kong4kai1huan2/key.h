#ifndef KEY_H
#define KEY_H

#include "ti_msp_dl_config.h"

uint8_t get_key_state(GPIO_Regs * const port, uint32_t key);

#endif


