#ifndef BSP_USB_CDC_H
#define BSP_USB_CDC_H

#include <stdint.h>

/* Native USB FS: PA11=D-, PA12=D+. */
void usb_cdc_init(void);
uint8_t usb_cdc_is_configured(void);
uint8_t usb_cdc_write(const uint8_t *data, uint16_t length);
uint8_t usb_cdc_write_string(const char *text);
uint8_t usb_cdc_read_byte(uint8_t *byte);
uint32_t usb_cdc_rx_overflow_count(void);

void OTG_FS_IRQHandler(void);

#endif
