#ifndef __BLUETOOTH_DIAG_H__
#define __BLUETOOTH_DIAG_H__

#include <stdint.h>

void bluetooth_diag_init(void);
void bluetooth_diag_service(void);
void bluetooth_diag_rx_isr(uint8_t byte);

#endif
