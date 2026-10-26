#include "h2026_uart_protocol.h"

static uint16_t h2026_read_u16_le(const uint8_t *data)
{
    return (uint16_t) data[0] | ((uint16_t) data[1] << 8);
}

static int16_t h2026_read_i16_le(const uint8_t *data)
{
    return (int16_t) h2026_read_u16_le(data);
}

static uint32_t h2026_read_u32_le(const uint8_t *data)
{
    return (uint32_t) data[0] |
           ((uint32_t) data[1] << 8) |
           ((uint32_t) data[2] << 16) |
           ((uint32_t) data[3] << 24);
}

static void h2026_write_u16_le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t) value;
    data[1] = (uint8_t) (value >> 8);
}

static void h2026_write_i16_le(uint8_t *data, int16_t value)
{
    h2026_write_u16_le(data, (uint16_t) value);
}

static void h2026_write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t) value;
    data[1] = (uint8_t) (value >> 8);
    data[2] = (uint8_t) (value >> 16);
    data[3] = (uint8_t) (value >> 24);
}

static void h2026_parser_reset(h2026_parser_t *parser)
{
    parser->index = 0U;
    parser->expected_size = 0U;
}

uint16_t h2026_crc16_ccitt_false(const uint8_t *data, uint8_t length)
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

void h2026_protocol_init(h2026_parser_t *parser)
{
    if(parser != 0)
    {
        h2026_parser_reset(parser);
    }
}

h2026_parse_result_t h2026_protocol_push(
    h2026_parser_t *parser,
    uint8_t byte,
    h2026_frame_t *frame)
{
    uint8_t payload_length;
    uint8_t expected_size;
    uint8_t index;
    uint16_t received_crc;
    uint16_t calculated_crc;

    if(parser == 0 || frame == 0)
    {
        return H2026_PARSE_FRAME_BAD;
    }

    if(parser->index == 0U)
    {
        if(byte == H2026_UART_SOF_0)
        {
            parser->buffer[0] = byte;
            parser->index = 1U;
        }
        return H2026_PARSE_NONE;
    }

    if(parser->index == 1U)
    {
        if(byte == H2026_UART_SOF_1)
        {
            parser->buffer[1] = byte;
            parser->index = 2U;
        }
        else if(byte != H2026_UART_SOF_0)
        {
            h2026_parser_reset(parser);
        }
        return H2026_PARSE_NONE;
    }

    if(parser->index >= H2026_UART_MAX_FRAME_SIZE)
    {
        h2026_parser_reset(parser);
        return H2026_PARSE_FRAME_BAD;
    }

    parser->buffer[parser->index++] = byte;

    if(parser->index == 6U)
    {
        payload_length = parser->buffer[5];
        if(payload_length > H2026_UART_MAX_PAYLOAD_SIZE)
        {
            h2026_parser_reset(parser);
            return H2026_PARSE_FRAME_BAD;
        }
        parser->expected_size =
            (uint8_t) (payload_length + H2026_UART_FRAME_OVERHEAD);
    }

    if(parser->expected_size == 0U ||
       parser->index < parser->expected_size)
    {
        return H2026_PARSE_NONE;
    }

    expected_size = parser->expected_size;
    payload_length = parser->buffer[5];
    received_crc = h2026_read_u16_le(
        &parser->buffer[(uint8_t) (expected_size - 2U)]);
    calculated_crc = h2026_crc16_ccitt_false(
        &parser->buffer[2],
        (uint8_t) (payload_length + 4U));

    if(parser->buffer[2] != H2026_UART_PROTOCOL_VERSION ||
       received_crc != calculated_crc)
    {
        h2026_parser_reset(parser);
        return H2026_PARSE_FRAME_BAD;
    }

    frame->version = parser->buffer[2];
    frame->message_type = parser->buffer[3];
    frame->sequence = parser->buffer[4];
    frame->payload_length = payload_length;
    for(index = 0U; index < payload_length; index++)
    {
        frame->payload[index] = parser->buffer[(uint8_t) (6U + index)];
    }

    h2026_parser_reset(parser);
    return H2026_PARSE_FRAME_OK;
}

uint8_t h2026_protocol_encode(
    uint8_t message_type,
    uint8_t sequence,
    const uint8_t *payload,
    uint8_t payload_length,
    uint8_t *output,
    uint8_t output_capacity)
{
    uint8_t frame_size;
    uint8_t index;
    uint16_t crc;

    if(output == 0 ||
       payload_length > H2026_UART_MAX_PAYLOAD_SIZE ||
       (payload_length > 0U && payload == 0))
    {
        return 0U;
    }

    frame_size = (uint8_t) (payload_length + H2026_UART_FRAME_OVERHEAD);
    if(output_capacity < frame_size)
    {
        return 0U;
    }

    output[0] = H2026_UART_SOF_0;
    output[1] = H2026_UART_SOF_1;
    output[2] = H2026_UART_PROTOCOL_VERSION;
    output[3] = message_type;
    output[4] = sequence;
    output[5] = payload_length;
    for(index = 0U; index < payload_length; index++)
    {
        output[(uint8_t) (6U + index)] = payload[index];
    }

    crc = h2026_crc16_ccitt_false(
        &output[2],
        (uint8_t) (payload_length + 4U));
    h2026_write_u16_le(&output[(uint8_t) (frame_size - 2U)], crc);
    return frame_size;
}

uint8_t h2026_pack_heartbeat(
    uint8_t payload[H2026_HEARTBEAT_PAYLOAD_SIZE],
    const h2026_heartbeat_t *message)
{
    if(payload == 0 || message == 0)
    {
        return 0U;
    }
    h2026_write_u32_le(&payload[0], message->uptime_ms);
    payload[4] = message->role;
    payload[5] = message->state;
    return H2026_HEARTBEAT_PAYLOAD_SIZE;
}

uint8_t h2026_pack_ball_position(
    uint8_t payload[H2026_BALL_PAYLOAD_SIZE],
    const h2026_ball_position_t *message)
{
    if(payload == 0 || message == 0)
    {
        return 0U;
    }
    h2026_write_i16_le(&payload[0], message->x_mm);
    payload[2] = message->confidence_percent;
    payload[3] = message->flags;
    h2026_write_u16_le(&payload[4], message->frame_id);
    h2026_write_u16_le(&payload[6], message->processing_ms);
    return H2026_BALL_PAYLOAD_SIZE;
}

uint8_t h2026_pack_b_status(
    uint8_t payload[H2026_B_STATUS_PAYLOAD_SIZE],
    const h2026_b_status_t *message)
{
    if(payload == 0 || message == 0)
    {
        return 0U;
    }
    h2026_write_i16_le(&payload[0], message->line_error);
    payload[2] = message->line_bits;
    payload[3] = message->flags;
    h2026_write_i16_le(&payload[4], message->ball_x_mm);
    h2026_write_i16_le(&payload[6], message->rod_angle_cdeg);
    h2026_write_u16_le(&payload[8], message->fault_bits);
    h2026_write_u16_le(&payload[10], message->vision_age_ms);
    return H2026_B_STATUS_PAYLOAD_SIZE;
}

uint8_t h2026_pack_a_command(
    uint8_t payload[H2026_A_COMMAND_PAYLOAD_SIZE],
    const h2026_a_command_t *message)
{
    if(payload == 0 || message == 0)
    {
        return 0U;
    }
    payload[0] = message->mode;
    payload[1] = message->flags;
    h2026_write_i16_le(&payload[2], message->target_x_mm);
    h2026_write_i16_le(&payload[4], message->vehicle_speed_mm_s);
    h2026_write_i16_le(&payload[6], message->vehicle_accel_mm_s2);
    h2026_write_u16_le(&payload[8], message->command_timeout_ms);
    return H2026_A_COMMAND_PAYLOAD_SIZE;
}

bool h2026_unpack_heartbeat(
    const h2026_frame_t *frame,
    h2026_heartbeat_t *message)
{
    if(frame == 0 || message == 0 ||
       frame->message_type != H2026_MSG_HEARTBEAT ||
       frame->payload_length != H2026_HEARTBEAT_PAYLOAD_SIZE)
    {
        return false;
    }
    message->uptime_ms = h2026_read_u32_le(&frame->payload[0]);
    message->role = frame->payload[4];
    message->state = frame->payload[5];
    return true;
}

bool h2026_unpack_ball_position(
    const h2026_frame_t *frame,
    h2026_ball_position_t *message)
{
    if(frame == 0 || message == 0 ||
       frame->message_type != H2026_MSG_BALL_POSITION ||
       frame->payload_length != H2026_BALL_PAYLOAD_SIZE)
    {
        return false;
    }
    message->x_mm = h2026_read_i16_le(&frame->payload[0]);
    message->confidence_percent = frame->payload[2];
    message->flags = frame->payload[3];
    message->frame_id = h2026_read_u16_le(&frame->payload[4]);
    message->processing_ms = h2026_read_u16_le(&frame->payload[6]);
    return true;
}

bool h2026_unpack_b_status(
    const h2026_frame_t *frame,
    h2026_b_status_t *message)
{
    if(frame == 0 || message == 0 ||
       frame->message_type != H2026_MSG_B_STATUS ||
       frame->payload_length != H2026_B_STATUS_PAYLOAD_SIZE)
    {
        return false;
    }
    message->line_error = h2026_read_i16_le(&frame->payload[0]);
    message->line_bits = frame->payload[2];
    message->flags = frame->payload[3];
    message->ball_x_mm = h2026_read_i16_le(&frame->payload[4]);
    message->rod_angle_cdeg = h2026_read_i16_le(&frame->payload[6]);
    message->fault_bits = h2026_read_u16_le(&frame->payload[8]);
    message->vision_age_ms = h2026_read_u16_le(&frame->payload[10]);
    return true;
}

bool h2026_unpack_a_command(
    const h2026_frame_t *frame,
    h2026_a_command_t *message)
{
    if(frame == 0 || message == 0 ||
       frame->message_type != H2026_MSG_A_COMMAND ||
       frame->payload_length != H2026_A_COMMAND_PAYLOAD_SIZE)
    {
        return false;
    }
    message->mode = frame->payload[0];
    message->flags = frame->payload[1];
    message->target_x_mm = h2026_read_i16_le(&frame->payload[2]);
    message->vehicle_speed_mm_s =
        h2026_read_i16_le(&frame->payload[4]);
    message->vehicle_accel_mm_s2 =
        h2026_read_i16_le(&frame->payload[6]);
    message->command_timeout_ms =
        h2026_read_u16_le(&frame->payload[8]);
    return true;
}
