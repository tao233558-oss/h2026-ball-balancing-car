#ifndef __KEY_H__
#define __KEY_H__

extern int mode;
extern int set;

#define KEY_NONE 0
#define KEY_SW1  1
#define KEY_SW2  2
#define KEY_SW3  3
#define KEY_SW4  4

void key_init();
uint8_t key_GetNum(void);

#endif
