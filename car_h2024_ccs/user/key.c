#include "headfile.h"

extern int mode;
extern int set;

void key_init()
{
   set=0;
	 mode=0;
	 gpio_init(GPIOA, DL_GPIO_PIN_27, PA27, IN_UP); /* SW1 / select */
	 gpio_init(GPIOA, DL_GPIO_PIN_17, PA17, IN_UP); /* SW2 / confirm/start */
	 gpio_init(GPIOB, DL_GPIO_PIN_23, PB23, IN_UP); /* SW3 / parameter + */
	 gpio_init(GPIOB, DL_GPIO_PIN_27, PB27, IN_UP); /* SW4 / parameter - */
}


uint8_t key_GetNum(void)
{
	uint8_t KeyNum = KEY_NONE;		
	
	if (gpio_get(GPIOA, DL_GPIO_PIN_27) == 0)			
	{
		delay_ms(20);										
		while (gpio_get(GPIOA, DL_GPIO_PIN_27) == 0);	
		delay_ms(20);										
		KeyNum = KEY_MODE;											
	}
	
	if (gpio_get(GPIOA, DL_GPIO_PIN_17) == 0)			
	{
		delay_ms(20);									
		while (gpio_get(GPIOA, DL_GPIO_PIN_17) == 0);	
		delay_ms(20);										
		KeyNum = KEY_START;										
	}

	if (gpio_get(GPIOB, DL_GPIO_PIN_23) == 0)
	{
		delay_ms(20);
		while (gpio_get(GPIOB, DL_GPIO_PIN_23) == 0);
		delay_ms(20);
		KeyNum = KEY_SW3;
	}

	if (gpio_get(GPIOB, DL_GPIO_PIN_27) == 0)
	{
		delay_ms(20);
		while (gpio_get(GPIOB, DL_GPIO_PIN_27) == 0);
		delay_ms(20);
		KeyNum = KEY_SW4;
	}
	
	  return KeyNum;		
}
