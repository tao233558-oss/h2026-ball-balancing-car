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

#include "bsp_IRtracking.h"
#include "board.h"
#include "stdio.h"



/******************************************************************
 * 函 数 名 称：IRtracking_GPIO_Init
 * 函 数 说 明：红外循迹模块引脚初始化
 * 函 数 形 参：无
 * 函 数 返 回：无
 * 作       者：LC
 * 备       注：AO引脚必须有ADC功能
******************************************************************/
void IRtracking_GPIO_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	ADC_CommonInitTypeDef ADC_CommonInitStruct;
	ADC_InitTypeDef ADC_InitStruct;

	RCC_AHB1PeriphClockCmd (RCC_IR_AO, ENABLE);	 // 开启GPIO时钟
	RCC_AHB1PeriphClockCmd (RCC_IR_DO, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_IR_ADC, ENABLE);  // 开启ADC1时钟

	// AO初始化
	GPIO_InitStructure.GPIO_Pin = GPIO_IR_AO;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;//模拟输入
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(PORT_IR_AO, &GPIO_InitStructure);
	
	// DO初始化
	GPIO_InitStructure.GPIO_Pin = GPIO_IR_DO;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;//输入
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(PORT_IR_DO, &GPIO_InitStructure);

	ADC_DeInit();//ADC复位

	ADC_CommonInitStruct.ADC_DMAAccessMode=ADC_DMAAccessMode_Disabled;
	ADC_CommonInitStruct.ADC_Mode=ADC_Mode_Independent;
	ADC_CommonInitStruct.ADC_Prescaler=ADC_Prescaler_Div4;
	ADC_CommonInitStruct.ADC_TwoSamplingDelay=ADC_TwoSamplingDelay_5Cycles;

	ADC_CommonInit(&ADC_CommonInitStruct);


	ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStruct.ADC_DataAlign=ADC_DataAlign_Right;
	ADC_InitStruct.ADC_ExternalTrigConvEdge=ADC_ExternalTrigConvEdge_None;
	ADC_InitStruct.ADC_NbrOfConversion=1;
	ADC_InitStruct.ADC_Resolution=ADC_Resolution_12b;
	ADC_InitStruct.ADC_ScanConvMode = DISABLE;

	ADC_Init(PORT_IR_ADC, &ADC_InitStruct);

	ADC_Cmd(PORT_IR_ADC, ENABLE); 
}                           

/******************************************************************
 * 函 数 名 称：Get_ADC_Value
 * 函 数 说 明：对ADC值进行平均值计算后输出
 * 函 数 形 参：
 * 函 数 返 回：对应扫描的ADC值
 * 作       者：LC
 * 备       注：无
******************************************************************/
unsigned int Get_ADC_Value(void)
{
	unsigned char i = 0;
	unsigned int AdcValue = 0;
	
	ADC_RegularChannelConfig(PORT_IR_ADC, CHANNEL_IR_ADC, 1, ADC_SampleTime_480Cycles );                            

	ADC_SoftwareStartConv(PORT_IR_ADC);

	/* 因为采集 SAMPLES 次，故循环 SAMPLES 次 */
	for(i=0; i< SAMPLES; i++)
	{
			/*    累加    */
			AdcValue += ADC_GetConversionValue(PORT_IR_ADC);
	}
	/* 求平均值 */
	AdcValue = AdcValue / SAMPLES;

	return AdcValue;
}
/******************************************************************
 * 函 数 名 称：Get_DO_Num
 * 函 数 说 明：读取传感器识别状态
 * 函 数 形 参：无
 * 函 数 返 回：1=识别为黑色   0=识别的不是黑色
 * 作       者：LC
 * 备       注：可以通过模块上的可调电阻调整识别黑色的阈值
******************************************************************/
unsigned char Get_DO_Num(void)
{
    if( IR_DO == 0 )//识别为黑色
    {
        return 0;
    }
    else//识别的不是黑色
    {
        return 1;
    }
}

