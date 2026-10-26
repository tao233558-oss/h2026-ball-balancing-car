/*
 * 立创开发板软硬件资料与相关扩展板软硬件资料官网全部开源
 * 开发板官网：www.lckfb.com
 * 技术支持常驻论坛，任何技术问题欢迎随时交流学习
 * 立创论坛：https://oshwhub.com/forum
 * 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
 * 不靠卖板赚钱，以培养中国工程师为己任
 * 
 Change Logs:
 * Date           Author       Notes
 * 2024-03-18     LCKFB-LP    first version
 */
#ifndef _BSP_IRTRACKING_H_
#define _BSP_IRTRACKING_H_
 
#include "stm32f4xx.h"

#define RCC_IR_ADC          RCC_APB2Periph_ADC1
#define RCC_IR_DO           RCC_AHB1Periph_GPIOA
#define RCC_IR_AO           RCC_AHB1Periph_GPIOC

#define PORT_IR_AO          GPIOC
#define GPIO_IR_AO          GPIO_Pin_1

#define PORT_IR_DO          GPIOA
#define GPIO_IR_DO          GPIO_Pin_1


#define PORT_IR_ADC         ADC1
#define CHANNEL_IR_ADC      ADC_Channel_11

#define IR_DO   GPIO_ReadInputDataBit( PORT_IR_DO, GPIO_IR_DO )



 //采样次数
#define SAMPLES         30


void IRtracking_GPIO_Init(void);//初始化
unsigned int Get_ADC_Value(void);//读取AO值
unsigned char Get_DO_Num(void);//读取DO值
#endif

