#ifndef __AIM_PROTOCOL_H__
#define __AIM_PROTOCOL_H__

#include <stdint.h>

#define AIM_PROTOCOL_SYNC_0          0xAAU
#define AIM_PROTOCOL_SYNC_1          0x55U
#define AIM_PROTOCOL_VERSION         0x01U
#define AIM_PROTOCOL_FRAME_SIZE      15U

#define AIM_FLAG_TARGET_VALID        0x01U
#define AIM_FLAG_SPOT_VALID          0x02U
#define AIM_FLAG_CIRCLE_COMPLETE     0x04U

typedef struct
{
    uint8_t sequence;
    uint8_t flags;
    int16_t target_x;
    int16_t target_y;
    int16_t spot_x;
    int16_t spot_y;
    uint8_t circle_phase;
} aim_vision_sample_t;

typedef struct
{
    uint8_t data[AIM_PROTOCOL_FRAME_SIZE];
    uint8_t index;
} aim_protocol_parser_t;

typedef enum
{
    AIM_PROTOCOL_NO_FRAME = 0,
    AIM_PROTOCOL_FRAME_OK = 1,
    AIM_PROTOCOL_FRAME_BAD = 2,
} aim_protocol_result_t;

void aim_protocol_init(aim_protocol_parser_t *parser);
aim_protocol_result_t aim_protocol_push(aim_protocol_parser_t *parser,
                                        uint8_t byte,
                                        aim_vision_sample_t *sample);
uint8_t aim_protocol_crc8(const uint8_t *data, uint8_t length);

#endif
