#include "h2026_uart_protocol_v2.h"

#include <string.h>


static void h2026_write_u16_le(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t) (value & 0xFFU);
    output[1] = (uint8_t) ((value >> 8U) & 0xFFU);
}


static void h2026_write_i16_le(uint8_t *output, int16_t value)
{
    h2026_write_u16_le(output, (uint16_t) value);
}


static void h2026_write_i32_le(uint8_t *output, int32_t value)
{
    uint32_t unsigned_value = (uint32_t) value;
    output[0] = (uint8_t) (unsigned_value & 0xFFU);
    output[1] = (uint8_t) ((unsigned_value >> 8U) & 0xFFU);
    output[2] = (uint8_t) ((unsigned_value >> 16U) & 0xFFU);
    output[3] = (uint8_t) ((unsigned_value >> 24U) & 0xFFU);
}


static uint16_t h2026_read_u16_le(const uint8_t *input)
{
    return (uint16_t) input[0] |
           (uint16_t) ((uint16_t) input[1] << 8U);
}


static int16_t h2026_read_i16_le(const uint8_t *input)
{
    return (int16_t) h2026_read_u16_le(input);
}


static int32_t h2026_read_i32_le(const uint8_t *input)
{
    uint32_t value =
        (uint32_t) input[0] |
        ((uint32_t) input[1] << 8U) |
        ((uint32_t) input[2] << 16U) |
        ((uint32_t) input[3] << 24U);
    return (int32_t) value;
}


static void h2026_parser_drop_front(h2026_parser_t *parser, uint8_t count)
{
    if(count >= parser->length)
    {
        parser->length = 0U;
        return;
    }

    (void) memmove(
        &parser->buffer[0],
        &parser->buffer[count],
        (size_t) (parser->length - count));
    parser->length = (uint8_t) (parser->length - count);
}


static void h2026_parser_align_header(h2026_parser_t *parser)
{
    while(parser->length >= 2U)
    {
        if(parser->buffer[0] == H2026_UART_SOF_0 &&
           parser->buffer[1] == H2026_UART_SOF_1)
        {
            return;
        }
        h2026_parser_drop_front(parser, 1U);
    }

    if(parser->length == 1U &&
       parser->buffer[0] != H2026_UART_SOF_0)
    {
        parser->length = 0U;
    }
}


uint16_t h2026_crc16_ccitt_false(const uint8_t *data, uint8_t length)
{
    uint16_t crc = 0xFFFFU;
    uint8_t index;
    uint8_t bit;

    if(data == 0)
    {
        return crc;
    }

    for(index = 0U; index < length; index++)
    {
        crc ^= (uint16_t) ((uint16_t) data[index] << 8U);
        for(bit = 0U; bit < 8U; bit++)
        {
            if((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t) ((crc << 1U) ^ 0x1021U);
            }
            else
            {
                crc <<= 1U;
            }
        }
    }
    return crc;
}


void h2026_parser_init(h2026_parser_t *parser)
{
    if(parser == 0)
    {
        return;
    }
    (void) memset(parser, 0, sizeof(*parser));
}


void h2026_parser_reset(h2026_parser_t *parser)
{
    if(parser == 0)
    {
        return;
    }
    parser->length = 0U;
    parser->last_byte_valid = false;
}


h2026_parse_result_t h2026_parser_feed(
    h2026_parser_t *parser,
    uint8_t byte,
    uint32_t now_ms,
    h2026_frame_t *frame)
{
    bool bad_seen = false;
    uint8_t payload_length;
    uint8_t expected_size;
    uint16_t received_crc;
    uint16_t calculated_crc;

    if(parser == 0 || frame == 0)
    {
        return H2026_PARSE_FRAME_BAD;
    }

    if(parser->last_byte_valid &&
       parser->length > 0U &&
       (uint32_t) (now_ms - parser->last_byte_ms) >
           H2026_UART_INTER_BYTE_TIMEOUT_MS)
    {
        parser->length = 0U;
    }

    parser->last_byte_ms = now_ms;
    parser->last_byte_valid = true;

    if(parser->length >= H2026_UART_MAX_FRAME_SIZE)
    {
        h2026_parser_drop_front(parser, 1U);
        parser->bad_frame_count++;
        bad_seen = true;
    }
    parser->buffer[parser->length++] = byte;

    for(;;)
    {
        h2026_parser_align_header(parser);
        if(parser->length < 6U)
        {
            return bad_seen ? H2026_PARSE_FRAME_BAD : H2026_PARSE_NONE;
        }

        payload_length = parser->buffer[5];
        if(payload_length > H2026_UART_MAX_PAYLOAD_SIZE)
        {
            h2026_parser_drop_front(parser, 1U);
            parser->bad_frame_count++;
            bad_seen = true;
            continue;
        }

        expected_size =
            (uint8_t) (payload_length + H2026_UART_FRAME_OVERHEAD);
        if(parser->length < expected_size)
        {
            return bad_seen ? H2026_PARSE_FRAME_BAD : H2026_PARSE_NONE;
        }

        received_crc = h2026_read_u16_le(
            &parser->buffer[(uint8_t) (expected_size - 2U)]);
        calculated_crc = h2026_crc16_ccitt_false(
            &parser->buffer[2],
            (uint8_t) (payload_length + 4U));

        if(parser->buffer[2] != H2026_UART_PROTOCOL_VERSION ||
           received_crc != calculated_crc)
        {
            /*
             * Do not discard the whole candidate. Sliding one byte lets the
             * parser find a valid AA 55 frame embedded after a false header.
             */
            h2026_parser_drop_front(parser, 1U);
            parser->bad_frame_count++;
            bad_seen = true;
            continue;
        }

        frame->version = parser->buffer[2];
        frame->message_type = parser->buffer[3];
        frame->sequence = parser->buffer[4];
        frame->payload_length = payload_length;
        if(payload_length > 0U)
        {
            (void) memcpy(
                frame->payload,
                &parser->buffer[6],
                payload_length);
        }
        h2026_parser_drop_front(parser, expected_size);
        return H2026_PARSE_FRAME_OK;
    }
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
    uint16_t crc;

    if(output == 0 ||
       payload_length > H2026_UART_MAX_PAYLOAD_SIZE ||
       (payload_length > 0U && payload == 0))
    {
        return 0U;
    }

    frame_size =
        (uint8_t) (payload_length + H2026_UART_FRAME_OVERHEAD);
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
    if(payload_length > 0U)
    {
        (void) memcpy(&output[6], payload, payload_length);
    }

    crc = h2026_crc16_ccitt_false(
        &output[2],
        (uint8_t) (payload_length + 4U));
    h2026_write_u16_le(&output[(uint8_t) (frame_size - 2U)], crc);
    return frame_size;
}


uint8_t h2026_pack_vision_sample(
    uint8_t payload[H2026_VISION_PAYLOAD_SIZE],
    const h2026_vision_sample_t *message)
{
    if(payload == 0 || message == 0)
    {
        return 0U;
    }
    h2026_write_i16_le(&payload[0], message->x_mm);
    payload[2] = message->confidence_percent;
    payload[3] = message->flags;
    h2026_write_u16_le(&payload[4], message->frame_id);
    h2026_write_u16_le(&payload[6], message->sample_age_ms);
    h2026_write_u16_le(&payload[8], message->inference_ms);
    return H2026_VISION_PAYLOAD_SIZE;
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
    h2026_write_i16_le(&payload[4], message->vehicle_speed_ref_mm_s);
    h2026_write_i16_le(&payload[6], message->vehicle_accel_ref_mm_s2);
    h2026_write_u16_le(&payload[8], message->command_id);
    return H2026_A_COMMAND_PAYLOAD_SIZE;
}


uint8_t h2026_pack_b_status(
    uint8_t payload[H2026_B_STATUS_PAYLOAD_SIZE],
    const h2026_b_status_t *message)
{
    if(payload == 0 || message == 0)
    {
        return 0U;
    }
    payload[0] = message->state;
    payload[1] = message->flags;
    h2026_write_i16_le(&payload[2], message->ball_x_mm);
    h2026_write_i16_le(&payload[4], message->ball_error_mm);
    h2026_write_i32_le(&payload[6], message->actuator_position_steps);
    h2026_write_i16_le(&payload[10], message->control_output_permille);
    h2026_write_i16_le(&payload[12], message->line_error);
    payload[14] = message->line_bits;
    payload[15] = 0U;
    h2026_write_u16_le(&payload[16], message->fault_bits);
    h2026_write_u16_le(&payload[18], message->vision_rx_age_ms);
    h2026_write_u16_le(&payload[20], message->ball_valid_age_ms);
    h2026_write_u16_le(&payload[22], message->command_rx_age_ms);
    h2026_write_u16_le(&payload[24], message->command_id_echo);
    return H2026_B_STATUS_PAYLOAD_SIZE;
}


bool h2026_unpack_vision_sample(
    const h2026_frame_t *frame,
    h2026_vision_sample_t *message)
{
    if(frame == 0 || message == 0 ||
       frame->message_type != H2026_MSG_VISION_SAMPLE ||
       frame->payload_length != H2026_VISION_PAYLOAD_SIZE)
    {
        return false;
    }
    message->x_mm = h2026_read_i16_le(&frame->payload[0]);
    message->confidence_percent = frame->payload[2];
    message->flags = frame->payload[3];
    message->frame_id = h2026_read_u16_le(&frame->payload[4]);
    message->sample_age_ms = h2026_read_u16_le(&frame->payload[6]);
    message->inference_ms = h2026_read_u16_le(&frame->payload[8]);
    return true;
}


bool h2026_unpack_vision_compact(
    const h2026_frame_t *frame,
    h2026_vision_sample_t *message)
{
    if(frame == 0 || message == 0 ||
       frame->message_type != H2026_MSG_VISION_COMPACT ||
       frame->payload_length != H2026_VISION_COMPACT_PAYLOAD_SIZE)
    {
        return false;
    }
    message->x_mm = h2026_read_i16_le(&frame->payload[0]);
    message->confidence_percent = frame->payload[2];
    message->flags = frame->payload[3];
    message->frame_id = frame->sequence;
    message->sample_age_ms = 0U;
    message->inference_ms = 0U;
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
    message->vehicle_speed_ref_mm_s =
        h2026_read_i16_le(&frame->payload[4]);
    message->vehicle_accel_ref_mm_s2 =
        h2026_read_i16_le(&frame->payload[6]);
    message->command_id = h2026_read_u16_le(&frame->payload[8]);
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
    message->state = frame->payload[0];
    message->flags = frame->payload[1];
    message->ball_x_mm = h2026_read_i16_le(&frame->payload[2]);
    message->ball_error_mm = h2026_read_i16_le(&frame->payload[4]);
    message->actuator_position_steps =
        h2026_read_i32_le(&frame->payload[6]);
    message->control_output_permille =
        h2026_read_i16_le(&frame->payload[10]);
    message->line_error = h2026_read_i16_le(&frame->payload[12]);
    message->line_bits = frame->payload[14];
    message->fault_bits = h2026_read_u16_le(&frame->payload[16]);
    message->vision_rx_age_ms =
        h2026_read_u16_le(&frame->payload[18]);
    message->ball_valid_age_ms =
        h2026_read_u16_le(&frame->payload[20]);
    message->command_rx_age_ms =
        h2026_read_u16_le(&frame->payload[22]);
    message->command_id_echo =
        h2026_read_u16_le(&frame->payload[24]);
    return true;
}
