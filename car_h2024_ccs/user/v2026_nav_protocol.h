#ifndef __V2026_NAV_PROTOCOL_H__
#define __V2026_NAV_PROTOCOL_H__

#include <stdint.h>

#define V2026_NAV_FRAME_SIZE       16U
#define V2026_NAV_SOF_0            0xA5U
#define V2026_NAV_SOF_1            0x5AU
#define V2026_NAV_PROTOCOL_VERSION 0x01U
#define V2026_NAV_MSG_DRIVE        0x01U

#define V2026_NAV_FLAG_ENABLE       0x01U
#define V2026_NAV_FLAG_ESTOP        0x02U
#define V2026_NAV_FLAG_HEADING_HOLD 0x04U
#define V2026_NAV_FLAG_FINAL_APPROACH 0x08U

typedef struct
{
    uint8_t sequence;
    uint8_t flags;
    int16_t linear_mm_s;
    int16_t angular_mdeg_s;
    int16_t target_heading_cdeg;
    uint16_t timeout_ms;
} v2026_nav_command_t;

typedef enum
{
    V2026_NAV_PARSE_NONE = 0,
    V2026_NAV_PARSE_FRAME_OK,
    V2026_NAV_PARSE_FRAME_BAD,
} v2026_nav_parse_result_t;

typedef struct
{
    uint8_t buffer[V2026_NAV_FRAME_SIZE];
    uint8_t index;
} v2026_nav_parser_t;

void v2026_nav_protocol_init(v2026_nav_parser_t *parser);
v2026_nav_parse_result_t v2026_nav_protocol_push(
    v2026_nav_parser_t *parser,
    uint8_t byte,
    v2026_nav_command_t *command);
uint16_t v2026_nav_crc16_ccitt(const uint8_t *data, uint8_t length);

#endif
