#ifndef __V2026_NAV_LINK_H__
#define __V2026_NAV_LINK_H__

#include <stdint.h>

#include "v2026_nav_protocol.h"

typedef struct
{
    uint32_t bytes_received;
    uint32_t frames_ok;
    uint32_t frames_bad;
    uint32_t duplicate_frames;
    uint32_t missing_frames;
    uint16_t age_ms;
    uint8_t connected;
} v2026_nav_link_status_t;

void v2026_nav_link_init(void);
v2026_nav_parse_result_t v2026_nav_link_push_byte(uint8_t byte);
void v2026_nav_link_tick_10ms(void);
uint8_t v2026_nav_link_get_snapshot(v2026_nav_command_t *command,
                                    v2026_nav_link_status_t *status);

#endif
