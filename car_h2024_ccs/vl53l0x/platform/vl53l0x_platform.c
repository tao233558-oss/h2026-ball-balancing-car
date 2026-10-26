#include "vl53l0x_platform.h"
#include "ml_i2c.h"
#include "ml_delay.h"

static VL53L0X_Error bus_status(uint8_t status)
{
    return (status == 0U) ? VL53L0X_ERROR_NONE
                          : VL53L0X_ERROR_CONTROL_INTERFACE;
}

VL53L0X_Error VL53L0X_WriteMulti(VL53L0X_DEV Dev, uint8_t index,
                                 uint8_t *pdata, uint32_t count)
{
    if((Dev == NULL) || (count > VL53L0X_MAX_I2C_XFER_SIZE))
    {
        return VL53L0X_ERROR_INVALID_PARAMS;
    }
    return bus_status(I2C_WriteBuffer(Dev->I2cDevAddr, index, pdata,
                                      (uint16_t) count));
}

VL53L0X_Error VL53L0X_ReadMulti(VL53L0X_DEV Dev, uint8_t index,
                                uint8_t *pdata, uint32_t count)
{
    if((Dev == NULL) || (pdata == NULL) || (count == 0U) ||
       (count > VL53L0X_MAX_I2C_XFER_SIZE))
    {
        return VL53L0X_ERROR_INVALID_PARAMS;
    }
    return bus_status(I2C_ReadBuffer(Dev->I2cDevAddr, index, pdata,
                                     (uint16_t) count));
}

VL53L0X_Error VL53L0X_WrByte(VL53L0X_DEV Dev, uint8_t index, uint8_t data)
{
    return VL53L0X_WriteMulti(Dev, index, &data, 1U);
}

VL53L0X_Error VL53L0X_WrWord(VL53L0X_DEV Dev, uint8_t index, uint16_t data)
{
    uint8_t bytes[2] = {(uint8_t) (data >> 8U), (uint8_t) data};
    return VL53L0X_WriteMulti(Dev, index, bytes, 2U);
}

VL53L0X_Error VL53L0X_WrDWord(VL53L0X_DEV Dev, uint8_t index, uint32_t data)
{
    uint8_t bytes[4] = {
        (uint8_t) (data >> 24U), (uint8_t) (data >> 16U),
        (uint8_t) (data >> 8U), (uint8_t) data
    };
    return VL53L0X_WriteMulti(Dev, index, bytes, 4U);
}

VL53L0X_Error VL53L0X_UpdateByte(VL53L0X_DEV Dev, uint8_t index,
                                 uint8_t and_data, uint8_t or_data)
{
    uint8_t value = 0U;
    VL53L0X_Error status = VL53L0X_RdByte(Dev, index, &value);

    if(status == VL53L0X_ERROR_NONE)
    {
        value = (uint8_t) ((value & and_data) | or_data);
        status = VL53L0X_WrByte(Dev, index, value);
    }
    return status;
}

VL53L0X_Error VL53L0X_RdByte(VL53L0X_DEV Dev, uint8_t index, uint8_t *data)
{
    return VL53L0X_ReadMulti(Dev, index, data, 1U);
}

VL53L0X_Error VL53L0X_RdWord(VL53L0X_DEV Dev, uint8_t index, uint16_t *data)
{
    uint8_t bytes[2];
    VL53L0X_Error status;

    if(data == NULL)
    {
        return VL53L0X_ERROR_INVALID_PARAMS;
    }
    status = VL53L0X_ReadMulti(Dev, index, bytes, 2U);
    if(status == VL53L0X_ERROR_NONE)
    {
        *data = (uint16_t) (((uint16_t) bytes[0] << 8U) | bytes[1]);
    }
    return status;
}

VL53L0X_Error VL53L0X_RdDWord(VL53L0X_DEV Dev, uint8_t index, uint32_t *data)
{
    uint8_t bytes[4];
    VL53L0X_Error status;

    if(data == NULL)
    {
        return VL53L0X_ERROR_INVALID_PARAMS;
    }
    status = VL53L0X_ReadMulti(Dev, index, bytes, 4U);
    if(status == VL53L0X_ERROR_NONE)
    {
        *data = ((uint32_t) bytes[0] << 24U) |
                ((uint32_t) bytes[1] << 16U) |
                ((uint32_t) bytes[2] << 8U) |
                (uint32_t) bytes[3];
    }
    return status;
}

VL53L0X_Error VL53L0X_PollingDelay(VL53L0X_DEV Dev)
{
    (void) Dev;
    delay_ms(1U);
    return VL53L0X_ERROR_NONE;
}
