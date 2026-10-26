#include "v2026_nav_protocol.h"

static int16_t v2026_nav_read_i16_le(const uint8_t *data)
{
    return (int16_t) ((uint16_t) data[0] |
                      ((uint16_t) data[1] << 8));
}

static uint16_t v2026_nav_read_u16_le(const uint8_t *data)
{
    return (uint16_t) data[0] | ((uint16_t) data[1] << 8);
}

uint16_t v2026_nav_crc16_ccitt(const uint8_t *data, uint8_t length)
{
    uint16_t crc = 0xFFFFU;
    uint8_t index;

    for(index = 0U; index < length; index++)
    {
        uint8_t bit;
        crc ^= (uint16_t) data[index] << 8;
        for(bit = 0U; bit < 8U; bit++)
        {
            if((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t) ((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void v2026_nav_protocol_init(v2026_nav_parser_t *parser)
{
    parser->index = 0U;
}

v2026_nav_parse_result_t v2026_nav_protocol_push(
    v2026_nav_parser_t *parser,
    uint8_t byte,
    v2026_nav_command_t *command)
{
    uint16_t received_crc;
    uint16_t calculated_crc;

    if(parser->index == 0U)
    {
        if(byte == V2026_NAV_SOF_0)
        {
            parser->buffer[parser->index++] = byte;
        }
        return V2026_NAV_PARSE_NONE;
    }

    if(parser->index == 1U && byte != V2026_NAV_SOF_1)
    {
        parser->index = (byte == V2026_NAV_SOF_0) ? 1U : 0U;
        parser->buffer[0] = V2026_NAV_SOF_0;
        return V2026_NAV_PARSE_NONE;
    }

    parser->buffer[parser->index++] = byte;
    if(parser->index < V2026_NAV_FRAME_SIZE)
    {
        return V2026_NAV_PARSE_NONE;
    }
    parser->index = 0U;

    received_crc = v2026_nav_read_u16_le(&parser->buffer[14]);
    calculated_crc = v2026_nav_crc16_ccitt(&parser->buffer[2], 12U);
    if(parser->buffer[2] != V2026_NAV_PROTOCOL_VERSION ||
       parser->buffer[3] != V2026_NAV_MSG_DRIVE ||
       received_crc != calculated_crc)
    {
        return V2026_NAV_PARSE_FRAME_BAD;
    }

    command->sequence = parser->buffer[4];
    command->flags = parser->buffer[5];
    command->linear_mm_s = v2026_nav_read_i16_le(&parser->buffer[6]);
    command->angular_mdeg_s = v2026_nav_read_i16_le(&parser->buffer[8]);
    command->target_heading_cdeg =
        v2026_nav_read_i16_le(&parser->buffer[10]);
    command->timeout_ms = v2026_nav_read_u16_le(&parser->buffer[12]);

    if(command->timeout_ms < 50U || command->timeout_ms > 1000U)
    {
        return V2026_NAV_PARSE_FRAME_BAD;
    }
    return V2026_NAV_PARSE_FRAME_OK;
}
