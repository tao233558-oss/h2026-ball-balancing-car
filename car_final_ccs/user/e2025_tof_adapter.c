#include "e2025_tof_adapter.h"

#include <stddef.h>

static uint32_t e2025_tof_pack(uint8_t sequence, uint8_t status,
                               uint16_t distance_mm)
{
    return ((uint32_t) sequence << 24U) |
           ((uint32_t) status << 16U) |
           (uint32_t) distance_mm;
}

static void e2025_tof_publish(e2025_tof_mailbox_t *mailbox,
                              uint16_t distance_mm, uint8_t status)
{
    uint8_t sequence;

    if(mailbox == NULL)
    {
        return;
    }

    sequence = (uint8_t) (mailbox->packed_sample >> 24U);
    sequence++;
    if(sequence == 0U)
    {
        sequence = 1U;
    }
    mailbox->packed_sample = e2025_tof_pack(sequence, status, distance_mm);
}

void e2025_tof_mailbox_init(e2025_tof_mailbox_t *mailbox)
{
    if(mailbox != NULL)
    {
        mailbox->packed_sample =
            e2025_tof_pack(1U, E2025_TOF_STATUS_UNAVAILABLE, 0U);
    }
}

void e2025_tof_reader_init(e2025_tof_reader_t *reader)
{
    if(reader == NULL)
    {
        return;
    }
    reader->last_sequence = 0U;
    reader->present = 0U;
    reader->valid = 0U;
    reader->range_status = E2025_TOF_STATUS_UNAVAILABLE;
    reader->distance_mm = 0U;
    reader->age_ticks = 0xFFFFU;
}

void e2025_tof_publish_sample(e2025_tof_mailbox_t *mailbox,
                              uint16_t distance_mm,
                              uint8_t range_status)
{
    e2025_tof_publish(mailbox, distance_mm, range_status);
}

void e2025_tof_publish_error(e2025_tof_mailbox_t *mailbox,
                             uint8_t status)
{
    if(status < E2025_TOF_STATUS_IO_ERROR)
    {
        status = E2025_TOF_STATUS_IO_ERROR;
    }
    e2025_tof_publish(mailbox, 0U, status);
}

void e2025_tof_snapshot_tick(const e2025_tof_mailbox_t *mailbox,
                             e2025_tof_reader_t *reader,
                             e2025_input_t *input)
{
    uint32_t packed;
    uint8_t sequence;
    uint8_t fresh = 0U;

    if((mailbox == NULL) || (reader == NULL) || (input == NULL))
    {
        return;
    }

    packed = mailbox->packed_sample;
    sequence = (uint8_t) (packed >> 24U);
    if(sequence != reader->last_sequence)
    {
        reader->last_sequence = sequence;
        reader->range_status = (uint8_t) (packed >> 16U);
        reader->distance_mm = (uint16_t) packed;
        reader->age_ticks = 0U;
        reader->present =
            (reader->range_status == E2025_TOF_STATUS_UNAVAILABLE) ? 0U : 1U;
        reader->valid = (reader->range_status == 0U) ? 1U : 0U;
        fresh = (reader->range_status < E2025_TOF_STATUS_IO_ERROR) ? 1U : 0U;
    }
    else if(reader->age_ticks < 0xFFFFU)
    {
        reader->age_ticks++;
    }

    if(reader->age_ticks > E2025_TOF_STALE_TICKS)
    {
        reader->valid = 0U;
    }

    input->tof_present = reader->present;
    input->distance_fresh = fresh;
    input->distance_valid = reader->valid;
    input->tof_range_status = reader->range_status;
    input->distance_mm = reader->distance_mm;
    input->distance_age_ticks = reader->age_ticks;
}
