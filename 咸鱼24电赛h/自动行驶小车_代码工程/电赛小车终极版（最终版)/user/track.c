#include "headfile.h"

void track_init(void)
{
  gpio_init(GPIOA, DL_GPIO_PIN_0,  PA0,  IN_UP);   // D1
	gpio_init(GPIOA, DL_GPIO_PIN_25, PA25, IN_UP);   // D2 / L3
	gpio_init(GPIOA, DL_GPIO_PIN_31, PA31, IN_UP);   // D3
	gpio_init(GPIOA, DL_GPIO_PIN_28, PA28, IN_UP);   // D4
	gpio_init(GPIOA, DL_GPIO_PIN_1,  PA1,  IN_UP);   // D5
	gpio_init(GPIOA, DL_GPIO_PIN_7,  PA7,  IN_UP);   // D6
	gpio_init(GPIOA, DL_GPIO_PIN_2,  PA2,  IN_UP);   // D7
	gpio_init(GPIOA, DL_GPIO_PIN_16, PA16, IN_UP);   // D8
}

int get_line_color(void)
{
	 if ((!gpio_get(GPIOA, DL_GPIO_PIN_0))||(!gpio_get(GPIOA, DL_GPIO_PIN_25))||(!gpio_get(GPIOA, DL_GPIO_PIN_31))||(!gpio_get(GPIOA, DL_GPIO_PIN_28))||(!gpio_get(GPIOA, DL_GPIO_PIN_1))||(!gpio_get(GPIOA, DL_GPIO_PIN_7))||(!gpio_get(GPIOA, DL_GPIO_PIN_2))||(!gpio_get(GPIOA, DL_GPIO_PIN_16)))
	 {
		   return 1;
	 }
	 return 0;
}
