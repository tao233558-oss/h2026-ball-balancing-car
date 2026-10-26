#include "headfile.h"

extern int mode;
extern int set;

void key_init()
{
   set=0;
	 mode=0;
	 gpio_init(GPIOA, DL_GPIO_PIN_27, PA27, IN_UP); /* sw1 */
	 gpio_init(GPIOA, DL_GPIO_PIN_17, PA17, IN_UP); /* sw2 */
	 gpio_init(GPIOB, DL_GPIO_PIN_23, PB23, IN_UP); /* sw3 */
	 gpio_init(GPIOB, DL_GPIO_PIN_27, PB27, IN_UP); /* sw4 */
}


uint8_t key_GetNum(void)
{
	uint8_t KeyNum = KEY_NONE;		
	
	if (gpio_get(GPIOA, DL_GPIO_PIN_27) == 0)			
	{
		delay_ms(20);											
		while (gpio_get(GPIOA, DL_GPIO_PIN_27) == 0);	
		delay_ms(20);											
		KeyNum = KEY_SW1;												
	}
	
	if (gpio_get(GPIOA, DL_GPIO_PIN_17) == 0)			
	{
		delay_ms(20);										
		while (gpio_get(GPIOA, DL_GPIO_PIN_17) == 0);	
		delay_ms(20);											
		KeyNum = KEY_SW2;											
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
