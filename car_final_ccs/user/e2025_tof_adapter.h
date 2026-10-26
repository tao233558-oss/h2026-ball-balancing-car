#ifndef E2025_TOF_ADAPTER_H_
#define E2025_TOF_ADAPTER_H_

#include <stdint.h>

#include "e2025_app.h"

#define E2025_TOF_STALE_TICKS       50U
#define E2025_TOF_STATUS_IO_ERROR  0xFEU
#define E2025_TOF_STATUS_UNAVAILABLE 0xFFU

/* One 32-bit aligned store publishes a complete sample from main to the ISR. */
typedef struct
{
    volatile uint32_t packed_sample;
} e2025_tof_mailbox_t;

typedef struct
{
    uint8_t last_sequence;
    uint8_t present;
    uint8_t valid;
    uint8_t range_status;
    uint16_t distance_mm;
    uint16_t age_ticks;
} e2025_tof_reader_t;

void e2025_tof_mailbox_init(e2025_tof_mailbox_t *mailbox);
void e2025_tof_reader_init(e2025_tof_reader_t *reader);
void e2025_tof_publish_sample(e2025_tof_mailbox_t *mailbox,
                              uint16_t distance_mm,
                              uint8_t range_status);
void e2025_tof_publish_error(e2025_tof_mailbox_t *mailbox,
                             uint8_t status);
void e2025_tof_snapshot_tick(const e2025_tof_mailbox_t *mailbox,
                             e2025_tof_reader_t *reader,
                             e2025_input_t *input);

#endif
