#include "tof200f.h"
#include "ml_i2c.h"
#include "ml_delay.h"
#include "vl53l0x_api.h"

static VL53L0X_Dev_t g_tof_device;
static uint16_t g_model_id;
static VL53L0X_Error g_last_api_error = VL53L0X_ERROR_NONE;

static tof200f_status_t remember(VL53L0X_Error status,
                                 tof200f_status_t stage_error)
{
    g_last_api_error = status;
    return (status == VL53L0X_ERROR_NONE) ? TOF200F_OK : stage_error;
}

tof200f_status_t tof200f_init(void)
{
    uint8_t vhv_settings = 0U;
    uint8_t phase_cal = 0U;
    uint32_t ref_spad_count = 0U;
    uint8_t is_aperture_spads = 0U;
    VL53L0X_Error status;

    g_model_id = 0U;
    g_last_api_error = VL53L0X_ERROR_NONE;
    g_tof_device.I2cDevAddr = TOF200F_I2C_ADDRESS_8BIT;
    g_tof_device.comms_type = 1U;
    g_tof_device.comms_speed_khz = 100U;

    I2C_Init();
    delay_ms(50U);

    status = VL53L0X_RdWord(&g_tof_device,
                             VL53L0X_REG_IDENTIFICATION_MODEL_ID,
                             &g_model_id);
    if(status != VL53L0X_ERROR_NONE)
    {
        g_last_api_error = status;
        return TOF200F_ERROR_NO_ACK;
    }
    if(g_model_id != TOF200F_EXPECTED_MODEL_ID)
    {
        return TOF200F_ERROR_BAD_ID;
    }

    status = VL53L0X_DataInit(&g_tof_device);
    if(status != VL53L0X_ERROR_NONE)
    {
        return remember(status, TOF200F_ERROR_DATA_INIT);
    }
    status = VL53L0X_StaticInit(&g_tof_device);
    if(status != VL53L0X_ERROR_NONE)
    {
        return remember(status, TOF200F_ERROR_STATIC_INIT);
    }
    status = VL53L0X_PerformRefCalibration(&g_tof_device,
                                           &vhv_settings, &phase_cal);
    if(status != VL53L0X_ERROR_NONE)
    {
        return remember(status, TOF200F_ERROR_REF_CAL);
    }
    status = VL53L0X_PerformRefSpadManagement(&g_tof_device,
                                              &ref_spad_count,
                                              &is_aperture_spads);
    if(status != VL53L0X_ERROR_NONE)
    {
        return remember(status, TOF200F_ERROR_SPAD);
    }
    status = VL53L0X_SetDeviceMode(&g_tof_device,
                                   VL53L0X_DEVICEMODE_SINGLE_RANGING);
    if(status != VL53L0X_ERROR_NONE)
    {
        return remember(status, TOF200F_ERROR_MODE);
    }

    status = VL53L0X_SetLimitCheckEnable(
        &g_tof_device, VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE, 1U);
    if(status == VL53L0X_ERROR_NONE)
    {
        status = VL53L0X_SetLimitCheckEnable(
            &g_tof_device, VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, 1U);
    }
    if(status == VL53L0X_ERROR_NONE)
    {
        status = VL53L0X_SetLimitCheckValue(
            &g_tof_device, VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE,
            (FixPoint1616_t) 1179648U); /* 18.0 */
    }
    if(status == VL53L0X_ERROR_NONE)
    {
        status = VL53L0X_SetLimitCheckValue(
            &g_tof_device, VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
            (FixPoint1616_t) 16384U); /* 0.25 */
    }
    if(status == VL53L0X_ERROR_NONE)
    {
        status = VL53L0X_SetMeasurementTimingBudgetMicroSeconds(
            &g_tof_device, 33000U);
    }
    if(status != VL53L0X_ERROR_NONE)
    {
        return remember(status, TOF200F_ERROR_CONFIG);
    }

    g_last_api_error = VL53L0X_ERROR_NONE;
    return TOF200F_OK;
}

tof200f_status_t tof200f_read_mm(uint16_t *distance_mm,
                                 uint8_t *range_status)
{
    VL53L0X_RangingMeasurementData_t measurement;
    VL53L0X_Error status;

    if((distance_mm == NULL) || (range_status == NULL))
    {
        return TOF200F_ERROR_MEASURE;
    }

    status = VL53L0X_PerformSingleRangingMeasurement(&g_tof_device,
                                                      &measurement);
    g_last_api_error = status;
    if(status != VL53L0X_ERROR_NONE)
    {
        return TOF200F_ERROR_MEASURE;
    }

    *distance_mm = measurement.RangeMilliMeter;
    *range_status = measurement.RangeStatus;
    return TOF200F_OK;
}

tof200f_status_t tof200f_start_continuous(void)
{
    VL53L0X_Error status;

    status = VL53L0X_SetDeviceMode(&g_tof_device,
                                   VL53L0X_DEVICEMODE_CONTINUOUS_RANGING);
    if(status == VL53L0X_ERROR_NONE)
    {
        status = VL53L0X_StartMeasurement(&g_tof_device);
    }
    g_last_api_error = status;
    return (status == VL53L0X_ERROR_NONE) ? TOF200F_OK
                                          : TOF200F_ERROR_MEASURE;
}

tof200f_status_t tof200f_poll_mm(uint16_t *distance_mm,
                                 uint8_t *range_status)
{
    uint8_t ready = 0U;
    VL53L0X_RangingMeasurementData_t measurement;
    VL53L0X_Error status;

    if((distance_mm == NULL) || (range_status == NULL))
    {
        return TOF200F_ERROR_MEASURE;
    }

    status = VL53L0X_GetMeasurementDataReady(&g_tof_device, &ready);
    if(status != VL53L0X_ERROR_NONE)
    {
        g_last_api_error = status;
        return TOF200F_ERROR_MEASURE;
    }
    if(ready == 0U)
    {
        g_last_api_error = VL53L0X_ERROR_NONE;
        return TOF200F_NOT_READY;
    }

    status = VL53L0X_GetRangingMeasurementData(&g_tof_device, &measurement);
    if(status == VL53L0X_ERROR_NONE)
    {
        status = VL53L0X_ClearInterruptMask(&g_tof_device, 0U);
    }
    g_last_api_error = status;
    if(status != VL53L0X_ERROR_NONE)
    {
        return TOF200F_ERROR_MEASURE;
    }

    *distance_mm = measurement.RangeMilliMeter;
    *range_status = measurement.RangeStatus;
    return TOF200F_OK;
}

tof200f_status_t tof200f_stop_continuous(void)
{
    VL53L0X_Error status = VL53L0X_StopMeasurement(&g_tof_device);

    g_last_api_error = status;
    return (status == VL53L0X_ERROR_NONE) ? TOF200F_OK
                                          : TOF200F_ERROR_MEASURE;
}

uint16_t tof200f_model_id(void)
{
    return g_model_id;
}

int8_t tof200f_last_api_error(void)
{
    return (int8_t) g_last_api_error;
}
