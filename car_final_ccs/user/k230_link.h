#ifndef __K230_LINK_H__
#define __K230_LINK_H__

#include <stdint.h>

#include "aim_protocol.h"

#define K230_LINK_TIMEOUT_TICKS 20U

typedef struct
{
    uint32_t bytes_received;
    uint32_t frames_ok;
    uint32_t frames_bad;
    uint32_t duplicate_frames;
    uint32_t missing_frames;
    uint32_t sequence_resets;
    uint16_t age_ticks;
    uint8_t last_sequence;
    uint8_t have_sequence;
    uint8_t connected;
} k230_link_status_t;

void k230_link_init(void);
aim_protocol_result_t k230_link_push_byte(uint8_t byte);
void k230_link_tick_10ms(void);
uint8_t k230_link_get_snapshot(aim_vision_sample_t *sample,
                               k230_link_status_t *status);

#endif
