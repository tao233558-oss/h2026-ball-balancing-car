#include "headfile.h"

#define ENCODER_ENABLE_LEGACY_KEY_ISR 0

static volatile int32_t Encoder_count1 = 0;
static volatile int32_t Encoder_count2 = 0;

int mode,set;

void encoder_init(void)
{ 
		Encoder_count1 = 0;
		Encoder_count2 = 0;
//		exti_init(EXTI_PA12, FALLING, 1);
//		gpio_init(GPIOA, DL_GPIO_PIN_13, PA13, IN_UP);
//		exti_init(EXTI_PA14, FALLING, 1);
//		gpio_init(GPIOA, DL_GPIO_PIN_15, PA15, IN_UP);
	
	  exti_init(EXTI_PA12, RISING, 0);                          //左轮A   PA12
	  exti_init(EXTI_PA13, FALLING,  0);			                     //左轮B  PA13
		gpio_init(GPIOA, DL_GPIO_PIN_12, PA12, IN_UP);           //上升沿 上拉
	  gpio_init(GPIOA, DL_GPIO_PIN_13, PA13, IN_DOWN);  			 //下降沿  下拉
	
		exti_init(EXTI_PA24, RISING, 0);													 //右轮A    PA24
	  exti_init(EXTI_PA15, FALLING, 0);													 //右轮B	  PA15
		gpio_init(GPIOA, DL_GPIO_PIN_24, PA24, IN_UP);            
	  gpio_init(GPIOA, DL_GPIO_PIN_15, PA15, IN_DOWN);                  
}

//读取编码器计数值
int read_encoder1(void)
{
	int32_t num;
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	num = Encoder_count1;
	Encoder_count1 = 0;
	if(primask == 0U)
	{
		__enable_irq();
	}
	return (int) num;
}

int read_encoder2(void)
{
	int32_t num;
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	num = Encoder_count2;
	Encoder_count2 = 0;
	if(primask == 0U)
	{
		__enable_irq();
	}
	return (int) num;
}

void encoder_read_and_reset(int32_t *left_raw, int32_t *right_raw)
{
	int32_t count1;
	int32_t count2;
	uint32_t primask = __get_PRIMASK();

	__disable_irq();
	count1 = Encoder_count1;
	count2 = Encoder_count2;
	Encoder_count1 = 0;
	Encoder_count2 = 0;
	if(primask == 0U)
	{
		__enable_irq();
	}

	/* PCB v4.0: channel 1 (PA12/PA13) is left; channel 2 (PA24/PA15) is right. */
	if(left_raw != 0)
	{
		*left_raw = count1;
	}
	if(right_raw != 0)
	{
		*right_raw = count2;
	}
}

int32_t encoder_left_apply_sign(int32_t raw_count)
{
	return raw_count * ENCODER_LEFT_APP_SIGN;
}

int32_t encoder_right_apply_sign(int32_t raw_count)
{
	return raw_count * ENCODER_RIGHT_APP_SIGN;
}



// 外部中断
void GROUP1_IRQHandler(void)//Group1的中断服务函数
{
	
  if(GPIOA->CPU_INT.MIS & DL_GPIO_PIN_12) // PA12外部中断   A上升沿
		{
			if(gpio_get(GPIOA, DL_GPIO_PIN_13)==0)       //B 逻辑低
			{
				Encoder_count1 ++;
			}
			else
			{
				Encoder_count1 --;
			}

			GPIOA->CPU_INT.ICLR |= DL_GPIO_PIN_12; // 清除中断标志位
		}
		
		  if(GPIOA->CPU_INT.MIS & DL_GPIO_PIN_13) // PA13外部中断   B相下降沿
		{
			if(gpio_get(GPIOA, DL_GPIO_PIN_12)==0)   // A 逻辑低
			{
				Encoder_count1 ++;
			}
			else
			{
				Encoder_count1 --;
			}

			GPIOA->CPU_INT.ICLR |= DL_GPIO_PIN_13; // 清除中断标志位
		}
		
		if(GPIOA->CPU_INT.MIS & DL_GPIO_PIN_24) // PA24外部中断   A相上升沿
		{
			if(gpio_get(GPIOA, DL_GPIO_PIN_15)==0)      	//B逻辑低
			{
				Encoder_count2 ++;
			}
			else
			{
				Encoder_count2 --;
			}

			
			GPIOA->CPU_INT.ICLR |= DL_GPIO_PIN_24; // 清除中断标志位
		}
		
			if(GPIOA->CPU_INT.MIS & DL_GPIO_PIN_15) // PA15外部中断   B相下降沿
		{
			if(gpio_get(GPIOA, DL_GPIO_PIN_24)==0)    //A逻辑低
			{
				Encoder_count2 ++;
			}
			else
			{
				Encoder_count2 --;
			}
			
			GPIOA->CPU_INT.ICLR |= DL_GPIO_PIN_15; // 清除中断标志位
		}
		
#if ENCODER_ENABLE_LEGACY_KEY_ISR
		if(GPIOA->CPU_INT.MIS & DL_GPIO_PIN_17)
		 {
         delay_ms(10);
			   mode++;
			   set=0;
			   if(mode==5)
		    {
			     mode=1;
			  }
		  
			GPIOA->CPU_INT.ICLR |= DL_GPIO_PIN_17; // 清除中断标志位
		}
		
		if(GPIOA->CPU_INT.MIS & DL_GPIO_PIN_27)
		{
			delay_ms(10);
			if(set==0)
        set=1;
			else set=0;
			GPIOA->CPU_INT.ICLR |= DL_GPIO_PIN_27; // 清除中断标志位
		}
#else
		/* SW2/SW1 use polling now; never delay inside the encoder ISR. */
		if(GPIOA->CPU_INT.MIS & DL_GPIO_PIN_17)
		{
			GPIOA->CPU_INT.ICLR |= DL_GPIO_PIN_17;
		}
		if(GPIOA->CPU_INT.MIS & DL_GPIO_PIN_27)
		{
			GPIOA->CPU_INT.ICLR |= DL_GPIO_PIN_27;
		}
#endif
}

	
	
