#ifndef TOF200F_H_
#define TOF200F_H_

#include <stdint.h>

#define TOF200F_I2C_ADDRESS_8BIT 0x52U
#define TOF200F_EXPECTED_MODEL_ID 0xEEAAU

typedef enum
{
    TOF200F_OK = 0,
    TOF200F_NOT_READY = 1,
    TOF200F_ERROR_NO_ACK = -1,
    TOF200F_ERROR_BAD_ID = -2,
    TOF200F_ERROR_DATA_INIT = -3,
    TOF200F_ERROR_STATIC_INIT = -4,
    TOF200F_ERROR_REF_CAL = -5,
    TOF200F_ERROR_SPAD = -6,
    TOF200F_ERROR_MODE = -7,
    TOF200F_ERROR_CONFIG = -8,
    TOF200F_ERROR_MEASURE = -9
} tof200f_status_t;

tof200f_status_t tof200f_init(void);
tof200f_status_t tof200f_read_mm(uint16_t *distance_mm,
                                 uint8_t *range_status);
tof200f_status_t tof200f_start_continuous(void);
tof200f_status_t tof200f_poll_mm(uint16_t *distance_mm,
                                 uint8_t *range_status);
tof200f_status_t tof200f_stop_continuous(void);
uint16_t tof200f_model_id(void);
int8_t tof200f_last_api_error(void);

#endif
