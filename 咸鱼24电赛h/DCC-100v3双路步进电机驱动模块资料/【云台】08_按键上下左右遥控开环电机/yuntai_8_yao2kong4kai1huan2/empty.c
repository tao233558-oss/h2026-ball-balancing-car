/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"
#include "delay.h"
#include "oled.h"
#include "bmp.h"
#include "key.h"
#include "step_motor.h"

// 激光笔接线
// +   5V
// -   GND
// S   PA17

// 按键板接线
// 公共端 3.3V
// 10号   PB3
// 1      PB6
// 2      PB7
// 3      PA26
// 4      PA25

int main(void)
{
    SYSCFG_DL_init();

    // u8 t=' ';
	OLED_Init();
	OLED_ColorTurn(0);//0正常显示，1 反色显示
    OLED_DisplayTurn(0);//0正常显示 1 屏幕翻转显示
	int key_10_flag = 0;

	step_motor_init();
    step_motor_dir_set(0, 2); // 设置步进电机2的方向为正转
    step_motor_start(2); // 启动步进电机2
    step_set_speed(30, 2);

	step_motor_dir_set(0, 1); // 设置步进电机1的方向为正转
	step_motor_start(1); // 启动步进电机1
	step_set_speed(30, 1);

	while(1)
	{
		// step_motor_dir_set(0, 2);
        // step_motor_set_angle(90, 2); // 设置步进电机2转动90度
		// step_motor_dir_set(0, 1);
		// step_motor_set_angle(90, 1); // 设置步进电机1转动90度
        // delay_ms(3000); // 延时3秒

        // step_motor_dir_set(1, 2);
        // step_motor_set_angle(90, 2); // 设置步进电机2转动90度
		// step_motor_dir_set(1, 1);
		// step_motor_set_angle(90, 1); // 设置步进电机1转动90度
        // delay_ms(3000); // 延时3秒
		uint8_t key_1_state = get_key_state(KEY_KEY_1_PORT, KEY_KEY_1_PIN);
		uint8_t key_4_state = get_key_state(KEY_KEY_4_PORT, KEY_KEY_4_PIN);
		uint8_t key_2_state = get_key_state(KEY_KEY_2_PORT, KEY_KEY_2_PIN);
		uint8_t key_3_state = get_key_state(KEY_KEY_3_PORT, KEY_KEY_3_PIN);

		if (key_1_state - key_4_state == 1)
		{
			 step_motor_dir_set(1, 1);
			 step_motor_set_angle(0.1, 1);
		}
		else if (key_4_state - key_1_state == 1)
		{
			 step_motor_dir_set(0, 1);
			 step_motor_set_angle(0.1, 1);
		}
		if (key_2_state - key_3_state == 1)
		{
			 step_motor_dir_set(1, 2);
			 step_motor_set_angle(0.1, 2);
		}
		else if (key_3_state - key_2_state == 1)
		{
			 step_motor_dir_set(0, 2);
			 step_motor_set_angle(0.1, 2);
		}




		delay_ms(3);
		
		uint8_t key_state = get_key_state(KEY_KEY_10_PORT, KEY_KEY_10_PIN);
		if(key_state == 1)
		{
			key_10_flag ++;
		}
		else
		{
			key_10_flag --;
		}
		key_10_flag = key_10_flag < 0?0:key_10_flag;

		if(key_10_flag > 300)
		{
			DL_GPIO_togglePins(ji1guang1_PORT, ji1guang1_ji1guang1_PIN_PIN);
			key_10_flag = 0;
		}
		
		// DL_GPIO_clearPins(ji1guang1_PORT, ji1guang1_ji1guang1_PIN_PIN);
		// delay_ms(1000);
		// DL_GPIO_setPins(ji1guang1_PORT, ji1guang1_ji1guang1_PIN_PIN);
		// delay_ms(1000);

		
	// 	OLED_ShowPicture(0,0,128,64,BMP1,1);
	// 	OLED_Refresh();
	// 	delay_ms(500);
	// 	OLED_Clear();
	// 	OLED_ShowChinese(0,0,0,16,1);//中
	// 	OLED_ShowChinese(18,0,1,16,1);//景
	// 	OLED_ShowChinese(36,0,2,16,1);//园
	// 	OLED_ShowChinese(54,0,3,16,1);//电
	// 	OLED_ShowChinese(72,0,4,16,1);//子
	// 	OLED_ShowChinese(90,0,5,16,1);//技
	// 	OLED_ShowChinese(108,0,6,16,1);//术
	// 	OLED_ShowString(8,16, (u8 *)"ZHONGJINGYUAN",16,1);
	// 	OLED_ShowString(20,32,(u8 *)"2014/05/01",16,1);
	// 	OLED_ShowString(0,48,(u8 *)"ASCII:",16,1);  
	// 	OLED_ShowString(63,48,(u8 *)"CODE:",16,1);
	// 	OLED_ShowChar(48,48,t,16,1);//显示ASCII字符	   
	// 	t++;
	// 	if(t>'~')t=' ';
	// 	OLED_ShowNum(103,48,t,3,16,1);
	// 	OLED_Refresh();
	// 	delay_ms(500);
	// 	OLED_Clear();
	// 	OLED_ShowChinese(0,0,0,16,1);  //16*16 中
	//   OLED_ShowChinese(16,0,0,24,1); //24*24 中
	// 	OLED_ShowChinese(24,20,0,32,1);//32*32 中
	//   OLED_ShowChinese(64,0,0,64,1); //64*64 中
	// 	OLED_Refresh();
	//   delay_ms(500);
  	// OLED_Clear();
	// 	OLED_ShowString(0,0,(u8 *)"ABC",8,1);//6*8 “ABC”
	// 	OLED_ShowString(0,8,(u8 *)"ABC",12,1);//6*12 “ABC”
	//   OLED_ShowString(0,20,(u8 *)"ABC",16,1);//8*16 “ABC”
	// 	OLED_ShowString(0,36,(u8 *)"ABC",24,1);//12*24 “ABC”
	//   OLED_Refresh();
	// 	delay_ms(500);
	// 	OLED_ScrollDisplay(11,4,1);
	}
}
