#ifndef __MENU_H__
#define __MENU_H__

#include <stdint.h>

void menu_init(void);
void menu_handle_key(uint8_t key);
void menu_update(void);
void menu_show(void);
uint8_t menu_is_running(void);

#endif
