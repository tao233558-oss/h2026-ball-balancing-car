#include "k230_link.h"

static aim_protocol_parser_t g_k230_parser;
static volatile aim_vision_sample_t g_k230_sample;
static volatile uint8_t g_k230_publish_guard;
static volatile uint8_t g_k230_sample_valid;
static volatile uint8_t g_k230_have_sequence;
static volatile uint8_t g_k230_last_sequence;
static volatile uint16_t g_k230_age_ticks;
static volatile uint32_t g_k230_bytes_received;
static volatile uint32_t g_k230_frames_ok;
static volatile uint32_t g_k230_frames_bad;
static volatile uint32_t g_k230_duplicate_frames;
static volatile uint32_t g_k230_missing_frames;
static volatile uint32_t g_k230_sequence_resets;

static void k230_link_publish_begin(void)
{
    g_k230_publish_guard++;
}

static void k230_link_publish_end(void)
{
    g_k230_publish_guard++;
}

void k230_link_init(void)
{
    aim_protocol_init(&g_k230_parser);
    g_k230_publish_guard = 0U;
    g_k230_sample_valid = 0U;
    g_k230_have_sequence = 0U;
    g_k230_last_sequence = 0U;
    g_k230_age_ticks = 0xFFFFU;
    g_k230_bytes_received = 0U;
    g_k230_frames_ok = 0U;
    g_k230_frames_bad = 0U;
    g_k230_duplicate_frames = 0U;
    g_k230_missing_frames = 0U;
    g_k230_sequence_resets = 0U;
}

aim_protocol_result_t k230_link_push_byte(uint8_t byte)
{
    aim_vision_sample_t sample;
    aim_protocol_result_t result =
        aim_protocol_push(&g_k230_parser, byte, &sample);

    k230_link_publish_begin();
    g_k230_bytes_received++;
    if(result == AIM_PROTOCOL_FRAME_OK)
    {
        if(g_k230_have_sequence)
        {
            uint8_t delta = (uint8_t) (sample.sequence - g_k230_last_sequence);
            if(delta == 0U)
            {
                g_k230_duplicate_frames++;
            }
            else if(delta > 128U)
            {
                /* Treat a large backward jump as a K230 restart, not packet loss. */
                g_k230_sequence_resets++;
                g_k230_last_sequence = sample.sequence;
            }
            else
            {
                if(delta > 1U)
                {
                    g_k230_missing_frames += (uint32_t) delta - 1U;
                }
                g_k230_last_sequence = sample.sequence;
            }
        }
        else
        {
            g_k230_have_sequence = 1U;
            g_k230_last_sequence = sample.sequence;
        }

        g_k230_sample.sequence = sample.sequence;
        g_k230_sample.flags = sample.flags;
        g_k230_sample.target_x = sample.target_x;
        g_k230_sample.target_y = sample.target_y;
        g_k230_sample.spot_x = sample.spot_x;
        g_k230_sample.spot_y = sample.spot_y;
        g_k230_sample.circle_phase = sample.circle_phase;
        g_k230_sample_valid = 1U;
        g_k230_age_ticks = 0U;
        g_k230_frames_ok++;
    }
    else if(result == AIM_PROTOCOL_FRAME_BAD)
    {
        g_k230_frames_bad++;
    }
    k230_link_publish_end();
    return result;
}

void k230_link_tick_10ms(void)
{
    k230_link_publish_begin();
    if(g_k230_sample_valid && g_k230_age_ticks < 0xFFFFU)
    {
        g_k230_age_ticks++;
    }
    k230_link_publish_end();
}

uint8_t k230_link_get_snapshot(aim_vision_sample_t *sample,
                               k230_link_status_t *status)
{
    uint8_t before;
    uint8_t after;
    uint8_t sample_valid;
    uint16_t age_ticks;

    do
    {
        before = g_k230_publish_guard;
        if(before & 1U)
        {
            continue;
        }

        sample_valid = g_k230_sample_valid;
        age_ticks = g_k230_age_ticks;
        if(sample != 0)
        {
            sample->sequence = g_k230_sample.sequence;
            sample->flags = g_k230_sample.flags;
            sample->target_x = g_k230_sample.target_x;
            sample->target_y = g_k230_sample.target_y;
            sample->spot_x = g_k230_sample.spot_x;
            sample->spot_y = g_k230_sample.spot_y;
            sample->circle_phase = g_k230_sample.circle_phase;
        }
        if(status != 0)
        {
            status->bytes_received = g_k230_bytes_received;
            status->frames_ok = g_k230_frames_ok;
            status->frames_bad = g_k230_frames_bad;
            status->duplicate_frames = g_k230_duplicate_frames;
            status->missing_frames = g_k230_missing_frames;
            status->sequence_resets = g_k230_sequence_resets;
            status->age_ticks = age_ticks;
            status->last_sequence = g_k230_last_sequence;
            status->have_sequence = g_k230_have_sequence;
        }
        after = g_k230_publish_guard;
    }
    while(before != after || (after & 1U));

    sample_valid =
        (sample_valid && age_ticks <= K230_LINK_TIMEOUT_TICKS) ? 1U : 0U;
    if(status != 0)
    {
        status->connected = sample_valid;
    }
    return sample_valid;
}
