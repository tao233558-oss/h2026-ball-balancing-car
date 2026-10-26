#ifndef __KEY_H__
#define __KEY_H__

extern int mode;
extern int set;

#define KEY_NONE   0

/* PCB v4.0 human-interface connector, all active low. */
#define KEY_SW1  1  /* PA27: select */
#define KEY_SW2  2  /* PA17: confirm/start */
#define KEY_SW3  3  /* PB23: parameter + */
#define KEY_SW4  4  /* PB27: parameter - */

#define KEY_MODE  KEY_SW1
#define KEY_START KEY_SW2

void key_init();
uint8_t key_GetNum(void);

#endif
