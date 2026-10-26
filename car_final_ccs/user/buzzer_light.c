#include "headfile.h"

void buzzer_light_init()
{
	  gpio_init(GPIOB, DL_GPIO_PIN_12, PB12, OUT);    // buzzer
  	gpio_init(GPIOB, DL_GPIO_PIN_13, PB13, OUT);    // light
    gpio_set(GPIOB, DL_GPIO_PIN_12, 1);
	  gpio_set(GPIOB, DL_GPIO_PIN_13, 0);	            
}

void beep()
{
    gpio_set(GPIOB, DL_GPIO_PIN_12, 0);
	  gpio_set(GPIOB, DL_GPIO_PIN_13, 1);
    tim_interrupt_ms_init(TIMG7, 500, 4);
}
