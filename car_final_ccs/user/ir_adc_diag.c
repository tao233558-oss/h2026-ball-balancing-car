#include "headfile.h"
#include "ir_adc_diag.h"

#define IR_CHANNEL_COUNT 8U
#define IR_ADC_DONE_MASK DL_ADC12_INTERRUPT_MEM7_RESULT_LOADED
#define IR_ADC_TIMEOUT_LOOPS 200000U
#define IR_FILTER_SAMPLES 16U
#define IR_CALIBRATION_SAMPLES 64U
#define IR_HYSTERESIS_COUNTS 20U
#define IR_MIN_CONTRAST_COUNTS 80U

#define IR_WHITE_KEY_PORT GPIOB
#define IR_WHITE_KEY_PIN DL_GPIO_PIN_2
#define IR_WHITE_KEY_IOMUX PB2
#define IR_BLACK_KEY_PORT GPIOB
#define IR_BLACK_KEY_PIN DL_GPIO_PIN_3
#define IR_BLACK_KEY_IOMUX PB3

/*
 * Sensor node wiring:
 * D1 PA27/A0.0, D2 PA26/A0.1, D3 PA25/A0.2, D4 PA24/A0.3,
 * D5 PB24/A0.5, D6 PB20/A0.6, D7 PA22/A0.7, D8 PA14/A0.12.
 *
 * The single stepper is deliberately kept asleep during this diagnostic:
 * PA7=SLP, PA12=STEP, PA13=DIR.
 */
static const uint32_t g_ir_adc_iomux[IR_CHANNEL_COUNT] = {
    PA27, PA26, PA25, PA24, PB24, PB20, PA22, PA14
};

static const uint32_t g_ir_adc_input[IR_CHANNEL_COUNT] = {
    DL_ADC12_INPUT_CHAN_0,
    DL_ADC12_INPUT_CHAN_1,
    DL_ADC12_INPUT_CHAN_2,
    DL_ADC12_INPUT_CHAN_3,
    DL_ADC12_INPUT_CHAN_5,
    DL_ADC12_INPUT_CHAN_6,
    DL_ADC12_INPUT_CHAN_7,
    DL_ADC12_INPUT_CHAN_12
};

static const DL_ADC12_ClockConfig g_ir_adc_clock = {
    .clockSel = DL_ADC12_CLOCK_ULPCLK,
    .divideRatio = DL_ADC12_CLOCK_DIVIDE_8,
    .freqRange = DL_ADC12_CLOCK_FREQ_RANGE_32_TO_40
};

static void ir_diag_stepper_sleep(void)
{
    gpio_init(GPIOA, DL_GPIO_PIN_7, PA7, OUT);
    gpio_init(GPIOA, DL_GPIO_PIN_12, PA12, OUT);
    gpio_init(GPIOA, DL_GPIO_PIN_13, PA13, OUT);
    gpio_set(GPIOA, DL_GPIO_PIN_7 | DL_GPIO_PIN_12 | DL_GPIO_PIN_13, 0U);
}

static void ir_adc_init(void)
{
    uint8_t i;

    for(i = 0U; i < IR_CHANNEL_COUNT; i++)
    {
        DL_GPIO_initPeripheralAnalogFunction(g_ir_adc_iomux[i]);
    }

    DL_ADC12_reset(ADC0);
    DL_ADC12_enablePower(ADC0);
    delay_ms(1U);

    DL_ADC12_setClockConfig(ADC0,
                            (DL_ADC12_ClockConfig *) &g_ir_adc_clock);
    DL_ADC12_initSeqSample(
        ADC0,
        DL_ADC12_REPEAT_MODE_DISABLED,
        DL_ADC12_SAMPLING_SOURCE_AUTO,
        DL_ADC12_TRIG_SRC_SOFTWARE,
        DL_ADC12_SEQ_START_ADDR_00,
        DL_ADC12_SEQ_END_ADDR_07,
        DL_ADC12_SAMP_CONV_RES_12_BIT,
        DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);

    for(i = 0U; i < IR_CHANNEL_COUNT; i++)
    {
        DL_ADC12_configConversionMem(
            ADC0,
            (DL_ADC12_MEM_IDX) i,
            g_ir_adc_input[i],
            DL_ADC12_REFERENCE_VOLTAGE_VDDA,
            DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
            DL_ADC12_AVERAGING_MODE_DISABLED,
            DL_ADC12_BURN_OUT_SOURCE_DISABLED,
            DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
            DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    }

    DL_ADC12_setPowerDownMode(ADC0, DL_ADC12_POWER_DOWN_MODE_MANUAL);
    DL_ADC12_setSampleTime0(ADC0, 160U);
    DL_ADC12_clearInterruptStatus(ADC0, IR_ADC_DONE_MASK);
    DL_ADC12_enableConversions(ADC0);
}

static uint8_t ir_adc_read(uint16_t result[IR_CHANNEL_COUNT])
{
    uint32_t timeout = IR_ADC_TIMEOUT_LOOPS;
    uint8_t i;

    DL_ADC12_clearInterruptStatus(ADC0, IR_ADC_DONE_MASK);
    DL_ADC12_startConversion(ADC0);

    while((DL_ADC12_getRawInterruptStatus(ADC0, IR_ADC_DONE_MASK) == 0U) &&
          (timeout > 0U))
    {
        timeout--;
    }

    if(timeout == 0U)
    {
        DL_ADC12_stopConversion(ADC0);
        DL_ADC12_enableConversions(ADC0);
        return 0U;
    }

    for(i = 0U; i < IR_CHANNEL_COUNT; i++)
    {
        result[i] = DL_ADC12_getMemResult(ADC0, (DL_ADC12_MEM_IDX) i);
    }

    DL_ADC12_clearInterruptStatus(ADC0, IR_ADC_DONE_MASK);
    DL_ADC12_enableConversions(ADC0);
    return 1U;
}

static uint8_t ir_adc_average(uint16_t result[IR_CHANNEL_COUNT],
                              uint16_t sample_count)
{
    uint32_t sum[IR_CHANNEL_COUNT] = {0U};
    uint16_t sample[IR_CHANNEL_COUNT];
    uint16_t n;
    uint8_t i;

    for(n = 0U; n < sample_count; n++)
    {
        if(ir_adc_read(sample) == 0U)
        {
            return 0U;
        }
        for(i = 0U; i < IR_CHANNEL_COUNT; i++)
        {
            sum[i] += sample[i];
        }
    }

    for(i = 0U; i < IR_CHANNEL_COUNT; i++)
    {
        result[i] = (uint16_t) ((sum[i] + sample_count / 2U) /
                                sample_count);
    }
    return 1U;
}

static void ir_cal_keys_init(void)
{
    gpio_init(IR_WHITE_KEY_PORT, IR_WHITE_KEY_PIN, IR_WHITE_KEY_IOMUX, IN_UP);
    gpio_init(IR_BLACK_KEY_PORT, IR_BLACK_KEY_PIN, IR_BLACK_KEY_IOMUX, IN_UP);
}

static uint8_t ir_cal_key_read(void)
{
    uint8_t key = 0U;

    if(gpio_get(IR_WHITE_KEY_PORT, IR_WHITE_KEY_PIN) == 0U)
    {
        delay_ms(20U);
        if(gpio_get(IR_WHITE_KEY_PORT, IR_WHITE_KEY_PIN) == 0U)
        {
            key = 1U;
            while(gpio_get(IR_WHITE_KEY_PORT, IR_WHITE_KEY_PIN) == 0U)
            {
                ir_diag_stepper_sleep();
                delay_ms(1U);
            }
        }
    }
    else if(gpio_get(IR_BLACK_KEY_PORT, IR_BLACK_KEY_PIN) == 0U)
    {
        delay_ms(20U);
        if(gpio_get(IR_BLACK_KEY_PORT, IR_BLACK_KEY_PIN) == 0U)
        {
            key = 2U;
            while(gpio_get(IR_BLACK_KEY_PORT, IR_BLACK_KEY_PIN) == 0U)
            {
                ir_diag_stepper_sleep();
                delay_ms(1U);
            }
        }
    }

    if(key != 0U)
    {
        delay_ms(20U);
    }
    return key;
}

static uint16_t ir_abs_difference(uint16_t a, uint16_t b)
{
    return (a >= b) ? (uint16_t) (a - b) : (uint16_t) (b - a);
}

static uint16_t ir_calculate_thresholds(
    const uint16_t white[IR_CHANNEL_COUNT],
    const uint16_t black[IR_CHANNEL_COUNT],
    uint16_t threshold[IR_CHANNEL_COUNT],
    uint8_t black_is_high[IR_CHANNEL_COUNT],
    uint8_t line_state[IR_CHANNEL_COUNT])
{
    uint16_t minimum_contrast = 0xFFFFU;
    uint8_t i;

    for(i = 0U; i < IR_CHANNEL_COUNT; i++)
    {
        uint16_t contrast = ir_abs_difference(white[i], black[i]);

        threshold[i] = (uint16_t) (((uint32_t) white[i] + black[i]) / 2U);
        black_is_high[i] = (black[i] > white[i]) ? 1U : 0U;
        line_state[i] = 0U;
        if(contrast < minimum_contrast)
        {
            minimum_contrast = contrast;
        }
    }
    return minimum_contrast;
}

static uint8_t ir_update_pattern(
    const uint16_t value[IR_CHANNEL_COUNT],
    const uint16_t threshold[IR_CHANNEL_COUNT],
    const uint8_t black_is_high[IR_CHANNEL_COUNT],
    uint8_t line_state[IR_CHANNEL_COUNT])
{
    uint8_t pattern = 0U;
    uint8_t i;

    for(i = 0U; i < IR_CHANNEL_COUNT; i++)
    {
        uint16_t upper = (uint16_t) (threshold[i] + IR_HYSTERESIS_COUNTS);
        uint16_t lower = (threshold[i] > IR_HYSTERESIS_COUNTS) ?
                         (uint16_t) (threshold[i] - IR_HYSTERESIS_COUNTS) :
                         0U;

        if(black_is_high[i] != 0U)
        {
            if(line_state[i] == 0U && value[i] >= upper)
            {
                line_state[i] = 1U;
            }
            else if(line_state[i] != 0U && value[i] <= lower)
            {
                line_state[i] = 0U;
            }
        }
        else
        {
            if(line_state[i] == 0U && value[i] <= lower)
            {
                line_state[i] = 1U;
            }
            else if(line_state[i] != 0U && value[i] >= upper)
            {
                line_state[i] = 0U;
            }
        }

        if(line_state[i] != 0U)
        {
            pattern |= (uint8_t) (1U << i);
        }
    }
    return pattern;
}

static int16_t ir_pattern_error(uint8_t pattern, uint8_t *active_count)
{
    /* D1 was previously verified as positive and D8 as negative. */
    static const int8_t weight[IR_CHANNEL_COUNT] = {
        35, 25, 15, 5, -5, -15, -25, -35
    };
    int16_t sum = 0;
    uint8_t count = 0U;
    uint8_t i;

    for(i = 0U; i < IR_CHANNEL_COUNT; i++)
    {
        if((pattern & (uint8_t) (1U << i)) != 0U)
        {
            sum += weight[i];
            count++;
        }
    }

    *active_count = count;
    return (count == 0U) ? 0 : (int16_t) (sum / count);
}

static void ir_adc_show_pair(uint8_t line,
                             uint8_t left_index,
                             uint8_t right_index,
                             const uint16_t result[IR_CHANNEL_COUNT])
{
    OLED_ShowChar(line, 1U, (char) ('1' + left_index));
    OLED_ShowString(line, 2U, ":");
    OLED_ShowNum(line, 3U, result[left_index], 4U);
    OLED_ShowChar(line, 8U, (char) ('1' + right_index));
    OLED_ShowString(line, 9U, ":");
    OLED_ShowNum(line, 10U, result[right_index], 4U);
    OLED_ShowString(line, 14U, "   ");
}

static void ir_adc_show_calibrated(
    uint8_t pattern,
    int16_t error,
    uint8_t active_count,
    uint16_t minimum_contrast,
    const uint16_t result[IR_CHANNEL_COUNT],
    uint8_t calibration_valid)
{
    uint8_t i;
    uint16_t shown_contrast =
        (minimum_contrast > 999U) ? 999U : minimum_contrast;

    OLED_ShowString(1U, 1U, "B:");
    for(i = 0U; i < IR_CHANNEL_COUNT; i++)
    {
        OLED_ShowChar(1U, (uint8_t) (3U + i),
                      (pattern & (uint8_t) (1U << i)) ? '1' : '0');
    }
    OLED_ShowString(1U, 11U, " C:");
    OLED_ShowString(1U, 14U, calibration_valid ? "OK " : "NO ");

    OLED_ShowString(2U, 1U, "E:");
    OLED_ShowSignedNum(2U, 3U, error, 2U);
    OLED_ShowString(2U, 6U, " N:");
    OLED_ShowNum(2U, 9U, active_count, 1U);
    OLED_ShowString(2U, 10U, " M:");
    OLED_ShowNum(2U, 13U, shown_contrast, 3U);

    ir_adc_show_pair(3U, 0U, 1U, result);
    ir_adc_show_pair(4U, 6U, 7U, result);
}

static void ir_adc_show_message(const char *message)
{
    OLED_Clear();
    OLED_ShowString(1U, 1U, "IR CALIBRATION  ");
    OLED_ShowString(2U, 1U, (char *) message);
    OLED_ShowString(3U, 1U, "HOLD SURFACE    ");
    OLED_ShowString(4U, 1U, "STEP:SLEEP      ");
}

void ir_adc_diag_run(void)
{
    uint16_t result[IR_CHANNEL_COUNT] = {0U};
    uint16_t white[IR_CHANNEL_COUNT] = {
        3850U, 2350U, 3850U, 3850U, 3850U, 3850U, 3850U, 3850U
    };
    uint16_t black[IR_CHANNEL_COUNT] = {
        4000U, 2500U, 4000U, 4000U, 4000U, 4000U, 4000U, 4000U
    };
    uint16_t threshold[IR_CHANNEL_COUNT] = {0U};
    uint8_t black_is_high[IR_CHANNEL_COUNT] = {0U};
    uint8_t line_state[IR_CHANNEL_COUNT] = {0U};
    uint8_t white_ready = 1U;
    uint8_t black_ready = 1U;
    uint8_t calibration_valid = 0U;
    uint16_t minimum_contrast = 0U;

    ir_diag_stepper_sleep();
    ir_cal_keys_init();
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(1U, 1U, "IR FIXED TEST   ");
    OLED_ShowString(2U, 1U, "NO CAL KEYS     ");
    OLED_ShowString(3U, 1U, "TH:3925/2425    ");
    OLED_ShowString(4U, 1U, "STEP:SLEEP      ");
    ir_adc_init();
    delay_ms(1000U);

    while(1)
    {
        uint8_t key;

        ir_diag_stepper_sleep();
        key = ir_cal_key_read();
        if(key == 1U)
        {
            ir_adc_show_message("CAPTURE WHITE   ");
            if(ir_adc_average(white, IR_CALIBRATION_SAMPLES) != 0U)
            {
                white_ready = 1U;
                ir_adc_show_message("WHITE SAVED     ");
            }
            else
            {
                ir_adc_show_message("ADC TIMEOUT     ");
            }
            delay_ms(600U);
        }
        else if(key == 2U)
        {
            ir_adc_show_message("CAPTURE BLACK   ");
            if(ir_adc_average(black, IR_CALIBRATION_SAMPLES) != 0U)
            {
                black_ready = 1U;
                ir_adc_show_message("BLACK SAVED     ");
            }
            else
            {
                ir_adc_show_message("ADC TIMEOUT     ");
            }
            delay_ms(600U);
        }

        if(white_ready != 0U && black_ready != 0U &&
           (key != 0U || minimum_contrast == 0U))
        {
            minimum_contrast = ir_calculate_thresholds(
                white, black, threshold, black_is_high, line_state);
            calibration_valid =
                (minimum_contrast >= IR_MIN_CONTRAST_COUNTS) ? 1U : 0U;
        }

        if(ir_adc_average(result, IR_FILTER_SAMPLES) != 0U)
        {
            if(white_ready != 0U && black_ready != 0U)
            {
                uint8_t pattern = 0U;
                uint8_t active_count = 0U;
                int16_t error = 0;

                if(calibration_valid != 0U)
                {
                    pattern = ir_update_pattern(
                        result, threshold, black_is_high, line_state);
                    error = ir_pattern_error(pattern, &active_count);
                }
                ir_adc_show_calibrated(
                    pattern, error, active_count, minimum_contrast,
                    result, calibration_valid);
            }
            else
            {
                ir_adc_show_pair(1U, 0U, 1U, result);
                ir_adc_show_pair(2U, 2U, 3U, result);
                ir_adc_show_pair(3U, 4U, 5U, result);
                ir_adc_show_pair(4U, 6U, 7U, result);
            }
        }
        else
        {
            OLED_ShowString(1U, 1U, "ADC TIMEOUT     ");
            OLED_ShowString(2U, 1U, "CHECK ADC POWER ");
            OLED_ShowString(3U, 1U, "STEP:SLEEP      ");
            OLED_ShowString(4U, 1U, "RESET BOARD     ");
        }
        delay_ms(100U);
    }
}
