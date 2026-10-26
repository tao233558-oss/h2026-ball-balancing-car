#include "vision_ascii_parser.h"

static void reset_working_frame(vision_ascii_parser_t *parser)
{
    parser->work_x = 0U;
    parser->work_y = 0U;
    parser->have_x_digit = 0U;
    parser->have_y_digit = 0U;
}

static vision_ascii_result_t reject_frame(
    vision_ascii_parser_t *parser)
{
    parser->state = VISION_ASCII_WAIT_HEADER;
    parser->error_count++;
    return VISION_ASCII_FRAME_BAD;
}

void vision_ascii_parser_init(vision_ascii_parser_t *parser)
{
    parser->state = VISION_ASCII_WAIT_HEADER;
    reset_working_frame(parser);
    parser->x = 0U;
    parser->y = 0U;
    parser->frame_count = 0U;
    parser->error_count = 0U;
}

vision_ascii_result_t vision_ascii_parser_push(
    vision_ascii_parser_t *parser,
    uint8_t byte)
{
    if(byte == (uint8_t)'[')
    {
        reset_working_frame(parser);
        parser->state = VISION_ASCII_READ_X;
        return VISION_ASCII_NONE;
    }

    switch(parser->state)
    {
        case VISION_ASCII_WAIT_HEADER:
            return VISION_ASCII_NONE;

        case VISION_ASCII_READ_X:
            if((byte >= (uint8_t)'0') &&
               (byte <= (uint8_t)'9'))
            {
                parser->work_x = (uint16_t)(
                    parser->work_x * 10U +
                    (uint16_t)(byte - (uint8_t)'0'));
                parser->have_x_digit = 1U;
                if(parser->work_x > 799U)
                {
                    return reject_frame(parser);
                }
                return VISION_ASCII_NONE;
            }
            if((byte == (uint8_t)',') &&
               (parser->have_x_digit != 0U))
            {
                parser->state = VISION_ASCII_READ_Y;
                return VISION_ASCII_NONE;
            }
            return reject_frame(parser);

        case VISION_ASCII_READ_Y:
            if((byte >= (uint8_t)'0') &&
               (byte <= (uint8_t)'9'))
            {
                parser->work_y = (uint16_t)(
                    parser->work_y * 10U +
                    (uint16_t)(byte - (uint8_t)'0'));
                parser->have_y_digit = 1U;
                if(parser->work_y > 479U)
                {
                    return reject_frame(parser);
                }
                return VISION_ASCII_NONE;
            }
            if((byte == (uint8_t)']') &&
               (parser->have_y_digit != 0U))
            {
                parser->x = parser->work_x;
                parser->y = parser->work_y;
                parser->frame_count++;
                parser->state = VISION_ASCII_WAIT_HEADER;
                return VISION_ASCII_FRAME_OK;
            }
            return reject_frame(parser);

        default:
            return reject_frame(parser);
    }
}
