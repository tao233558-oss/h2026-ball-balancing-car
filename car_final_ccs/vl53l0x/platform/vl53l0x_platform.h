#ifndef VL53L0X_PLATFORM_H_
#define VL53L0X_PLATFORM_H_

#include "vl53l0x_def.h"
#include <string.h>

/* TOF200F is powered from 3.3 V, so enable the VL53L0X 2.8/3.3 V I/O pad. */
#define USE_I2C_2V8 1

/* The firmware keeps the ST API trace layer disabled. */
#define TRACE_MODULE_API 1
#define VL53L0X_ErrLog(...) ((void) 0)
#define _LOG_FUNCTION_START(module, fmt, ...) ((void) 0)
#define _LOG_FUNCTION_END(module, status, ...) ((void) 0)
#define _LOG_FUNCTION_END_FMT(module, status, fmt, ...) ((void) 0)
#define VL53L0X_COPYSTRING(destination, source) strcpy((destination), (source))

typedef struct
{
    VL53L0X_DevData_t Data;
    uint8_t I2cDevAddr;
    uint8_t comms_type;
    uint16_t comms_speed_khz;
} VL53L0X_Dev_t;

typedef VL53L0X_Dev_t *VL53L0X_DEV;

#define VL53L0X_MAX_I2C_XFER_SIZE 64U
#define PALDevDataGet(Dev, field) ((Dev)->Data.field)
#define PALDevDataSet(Dev, field, value) ((Dev)->Data.field = (value))

VL53L0X_Error VL53L0X_WriteMulti(VL53L0X_DEV Dev, uint8_t index,
                                 uint8_t *pdata, uint32_t count);
VL53L0X_Error VL53L0X_ReadMulti(VL53L0X_DEV Dev, uint8_t index,
                                uint8_t *pdata, uint32_t count);
VL53L0X_Error VL53L0X_WrByte(VL53L0X_DEV Dev, uint8_t index, uint8_t data);
VL53L0X_Error VL53L0X_WrWord(VL53L0X_DEV Dev, uint8_t index, uint16_t data);
VL53L0X_Error VL53L0X_WrDWord(VL53L0X_DEV Dev, uint8_t index, uint32_t data);
VL53L0X_Error VL53L0X_UpdateByte(VL53L0X_DEV Dev, uint8_t index,
                                 uint8_t and_data, uint8_t or_data);
VL53L0X_Error VL53L0X_RdByte(VL53L0X_DEV Dev, uint8_t index, uint8_t *data);
VL53L0X_Error VL53L0X_RdWord(VL53L0X_DEV Dev, uint8_t index, uint16_t *data);
VL53L0X_Error VL53L0X_RdDWord(VL53L0X_DEV Dev, uint8_t index, uint32_t *data);
VL53L0X_Error VL53L0X_PollingDelay(VL53L0X_DEV Dev);

#endif
