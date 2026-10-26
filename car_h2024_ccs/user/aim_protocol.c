#include "aim_protocol.h"

#include <stddef.h>

static int16_t aim_read_i16_le(const uint8_t *data)
{
    return (int16_t) ((uint16_t) data[0] | ((uint16_t) data[1] << 8));
}

uint8_t aim_protocol_crc8(const uint8_t *data, uint8_t length)
{
    uint8_t crc = 0;

    for(uint8_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for(uint8_t bit = 0; bit < 8; bit++)
        {
            crc = (crc & 0x80U) ? (uint8_t) ((crc << 1) ^ 0x07U)
                                : (uint8_t) (crc << 1);
        }
    }
    return crc;
}

void aim_protocol_init(aim_protocol_parser_t *parser)
{
    if(parser != NULL)
    {
        parser->index = 0;
    }
}

aim_protocol_result_t aim_protocol_push(aim_protocol_parser_t *parser,
                                        uint8_t byte,
                                        aim_vision_sample_t *sample)
{
    uint8_t crc;

    if(parser == NULL || sample == NULL)
    {
        return AIM_PROTOCOL_FRAME_BAD;
    }

    if(parser->index == 0U)
    {
        if(byte == AIM_PROTOCOL_SYNC_0)
        {
            parser->data[0] = byte;
            parser->index = 1;
        }
        return AIM_PROTOCOL_NO_FRAME;
    }

    if(parser->index == 1U)
    {
        if(byte == AIM_PROTOCOL_SYNC_1)
        {
            parser->data[1] = byte;
            parser->index = 2;
        }
        else if(byte == AIM_PROTOCOL_SYNC_0)
        {
            parser->data[0] = byte;
            parser->index = 1;
        }
        else
        {
            parser->index = 0;
        }
        return AIM_PROTOCOL_NO_FRAME;
    }

    parser->data[parser->index++] = byte;
    if(parser->index < AIM_PROTOCOL_FRAME_SIZE)
    {
        return AIM_PROTOCOL_NO_FRAME;
    }

    parser->index = 0;
    crc = aim_protocol_crc8(&parser->data[2], 12U);
    if(parser->data[2] != AIM_PROTOCOL_VERSION ||
       crc != parser->data[AIM_PROTOCOL_FRAME_SIZE - 1U])
    {
        return AIM_PROTOCOL_FRAME_BAD;
    }

    sample->sequence = parser->data[3];
    sample->flags = parser->data[4];
    sample->target_x = aim_read_i16_le(&parser->data[5]);
    sample->target_y = aim_read_i16_le(&parser->data[7]);
    sample->spot_x = aim_read_i16_le(&parser->data[9]);
    sample->spot_y = aim_read_i16_le(&parser->data[11]);
    sample->circle_phase = parser->data[13];
    return AIM_PROTOCOL_FRAME_OK;
}
