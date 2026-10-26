#ifndef VISION_ASCII_PARSER_H
#define VISION_ASCII_PARSER_H

#include <stdint.h>

typedef enum
{
    VISION_ASCII_WAIT_HEADER = 0,
    VISION_ASCII_READ_X,
    VISION_ASCII_READ_Y
} vision_ascii_state_t;

typedef enum
{
    VISION_ASCII_NONE = 0,
    VISION_ASCII_FRAME_OK,
    VISION_ASCII_FRAME_BAD
} vision_ascii_result_t;

typedef struct
{
    vision_ascii_state_t state;
    uint16_t work_x;
    uint16_t work_y;
    uint8_t have_x_digit;
    uint8_t have_y_digit;
    uint16_t x;
    uint16_t y;
    uint32_t frame_count;
    uint32_t error_count;
} vision_ascii_parser_t;

void vision_ascii_parser_init(vision_ascii_parser_t *parser);
vision_ascii_result_t vision_ascii_parser_push(
    vision_ascii_parser_t *parser,
    uint8_t byte);

#endif
