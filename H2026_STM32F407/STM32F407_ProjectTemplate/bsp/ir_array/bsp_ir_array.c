#include "bsp_ir_array.h"

#define IR_ENABLE_PIN       GPIO_Pin_2
#define IR_AVERAGE_SAMPLES  8U

static const uint8_t ir_adc_channels[IR_ARRAY_CHANNEL_COUNT] =
{
    ADC_Channel_10,
    ADC_Channel_11,
    ADC_Channel_12,
    ADC_Channel_13,
    ADC_Channel_14,
    ADC_Channel_15,
    ADC_Channel_8,
    ADC_Channel_9
};

static uint16_t ir_adc_read_average(uint8_t channel)
{
    uint8_t sample;
    uint32_t sum = 0U;

    ADC_RegularChannelConfig(ADC1, channel, 1U, ADC_SampleTime_480Cycles);

    for(sample = 0U; sample < IR_AVERAGE_SAMPLES; sample++)
    {
        ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
        ADC_SoftwareStartConv(ADC1);
        while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET)
        {
            /* Conversion completes in hardware. */
        }
        sum += ADC_GetConversionValue(ADC1);
    }

    return (uint16_t)(sum / IR_AVERAGE_SAMPLES);
}

void ir_array_set_enabled(uint8_t enabled)
{
    if(enabled != 0U)
    {
        GPIO_SetBits(GPIOE, IR_ENABLE_PIN);
    }
    else
    {
        GPIO_ResetBits(GPIOE, IR_ENABLE_PIN);
    }
}

void ir_array_init(void)
{
    GPIO_InitTypeDef gpio;
    ADC_CommonInitTypeDef adc_common;
    ADC_InitTypeDef adc;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB |
                           RCC_AHB1Periph_GPIOC |
                           RCC_AHB1Periph_GPIOE, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    gpio.GPIO_Mode = GPIO_Mode_AN;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 |
                    GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_Init(GPIOC, &gpio);

    gpio.GPIO_Pin = IR_ENABLE_PIN;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOE, &gpio);
    ir_array_set_enabled(1U);

    ADC_CommonStructInit(&adc_common);
    adc_common.ADC_Mode = ADC_Mode_Independent;
    adc_common.ADC_Prescaler = ADC_Prescaler_Div4;
    adc_common.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
    adc_common.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInit(&adc_common);

    ADC_StructInit(&adc);
    adc.ADC_Resolution = ADC_Resolution_12b;
    adc.ADC_ScanConvMode = DISABLE;
    adc.ADC_ContinuousConvMode = DISABLE;
    adc.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    adc.ADC_DataAlign = ADC_DataAlign_Right;
    adc.ADC_NbrOfConversion = 1U;
    ADC_Init(ADC1, &adc);
    ADC_Cmd(ADC1, ENABLE);
}

void ir_array_read_all(uint16_t values[IR_ARRAY_CHANNEL_COUNT])
{
    uint8_t index;

    for(index = 0U; index < IR_ARRAY_CHANNEL_COUNT; index++)
    {
        values[index] = ir_adc_read_average(ir_adc_channels[index]);
    }
}
