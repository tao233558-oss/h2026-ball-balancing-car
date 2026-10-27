#include <stdio.h>
#include "vision_ascii_parser.h"

int main(void)
{
    vision_ascii_parser_t parser;
    int byte;
    vision_ascii_parser_init(&parser);
    while((byte = getchar()) != EOF)
    {
        if(vision_ascii_parser_push(&parser, (uint8_t)byte) == VISION_ASCII_FRAME_OK)
        {
            printf("%u,%u\n", (unsigned)parser.x, (unsigned)parser.y);
        }
    }
    return 0;
}
