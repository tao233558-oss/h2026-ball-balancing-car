#include "headfile.h"

uint8_t motorA_dir=1;   //1为正转 0为反转
uint8_t motorB_dir=1;


static void motor_init_common(void)
{
		pwm_init(TIMG6,DL_TIMER_CC_0_INDEX,1000);	       // A左轮 PWM=PA29, DIR=PB20/PB24
		gpio_init(GPIOB,DL_GPIO_PIN_24,PB24,OUT);
		gpio_init(GPIOB,DL_GPIO_PIN_20,PB20,OUT);
	
	  pwm_init(TIMG6,DL_TIMER_CC_1_INDEX,1000);	      // B右轮 PWM=PA30, DIR=PB18/PB19
		gpio_init(GPIOB,DL_GPIO_PIN_19,PB19,OUT);    
		gpio_init(GPIOB,DL_GPIO_PIN_18,PB18,OUT);
	
	  gpio_init(GPIOB,DL_GPIO_PIN_16,PB16,OUT);     //Stay   PB16
	  Set_left_pwm(0);
	  Set_right_pwm(0);
	  gpio_set(GPIOB, DL_GPIO_PIN_16, 0);
}

void motor_init(void)    //初始化
{
	motor_init_common();
	motor_set_standby(1U);
}

void motor_safe_init_disabled(void)
{
	motor_init_common();
	motor_set_standby(0U);
}

void motor_set_standby(uint8_t enable)
{
	gpio_set(GPIOB, DL_GPIO_PIN_16, enable ? 1U : 0U);
}



void Set_left_pwm(int pwm)
{
	  if(pwm>0)
	  {
		gpio_set(GPIOB,DL_GPIO_PIN_24,1);
		gpio_set(GPIOB,DL_GPIO_PIN_20,0);
		pwm_update(TIMG6,DL_TIMER_CC_0_INDEX,pwm);
	  }
	  else if(pwm==0)
	  {
		pwm_update(TIMG6,DL_TIMER_CC_0_INDEX,pwm);	
	  }
	  else
	  {
		gpio_set(GPIOB,DL_GPIO_PIN_24,0);
		gpio_set(GPIOB,DL_GPIO_PIN_20,1);
		pwm_update(TIMG6,DL_TIMER_CC_0_INDEX,-pwm);
	  }
}


void Set_right_pwm(int pwm)
{
	if(pwm>0)
	{
		gpio_set(GPIOB,DL_GPIO_PIN_19,1);
		gpio_set(GPIOB,DL_GPIO_PIN_18,0);
		pwm_update(TIMG6,DL_TIMER_CC_1_INDEX,pwm);	
	}
	else if(pwm==0)
	{
		pwm_update(TIMG6,DL_TIMER_CC_1_INDEX,pwm);	
	}
	else{
		gpio_set(GPIOB,DL_GPIO_PIN_19,0);
		gpio_set(GPIOB,DL_GPIO_PIN_18,1);
		pwm_update(TIMG6,DL_TIMER_CC_1_INDEX,-pwm);
	}
}
	
	
