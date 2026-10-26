#include "v2026_nav_link.h"

static v2026_nav_parser_t g_parser;
static volatile v2026_nav_command_t g_command;
static volatile uint8_t g_publish_guard;
static volatile uint8_t g_command_valid;
static volatile uint8_t g_have_sequence;
static volatile uint8_t g_last_sequence;
static volatile uint16_t g_age_ms;
static volatile uint32_t g_bytes_received;
static volatile uint32_t g_frames_ok;
static volatile uint32_t g_frames_bad;
static volatile uint32_t g_duplicate_frames;
static volatile uint32_t g_missing_frames;

static void v2026_nav_link_publish_begin(void)
{
    g_publish_guard++;
}

static void v2026_nav_link_publish_end(void)
{
    g_publish_guard++;
}

void v2026_nav_link_init(void)
{
    v2026_nav_protocol_init(&g_parser);
    g_publish_guard = 0U;
    g_command_valid = 0U;
    g_have_sequence = 0U;
    g_last_sequence = 0U;
    g_age_ms = 0xFFFFU;
    g_bytes_received = 0U;
    g_frames_ok = 0U;
    g_frames_bad = 0U;
    g_duplicate_frames = 0U;
    g_missing_frames = 0U;
}

v2026_nav_parse_result_t v2026_nav_link_push_byte(uint8_t byte)
{
    v2026_nav_command_t command;
    v2026_nav_parse_result_t result =
        v2026_nav_protocol_push(&g_parser, byte, &command);

    v2026_nav_link_publish_begin();
    g_bytes_received++;
    if(result == V2026_NAV_PARSE_FRAME_OK)
    {
        if(g_have_sequence != 0U)
        {
            uint8_t delta = (uint8_t) (command.sequence - g_last_sequence);
            if(delta == 0U)
            {
                g_duplicate_frames++;
            }
            else if(delta <= 128U && delta > 1U)
            {
                g_missing_frames += (uint32_t) delta - 1U;
            }
        }
        g_have_sequence = 1U;
        g_last_sequence = command.sequence;
        g_command.sequence = command.sequence;
        g_command.flags = command.flags;
        g_command.linear_mm_s = command.linear_mm_s;
        g_command.angular_mdeg_s = command.angular_mdeg_s;
        g_command.target_heading_cdeg = command.target_heading_cdeg;
        g_command.timeout_ms = command.timeout_ms;
        g_command_valid = 1U;
        g_age_ms = 0U;
        g_frames_ok++;
    }
    else if(result == V2026_NAV_PARSE_FRAME_BAD)
    {
        g_frames_bad++;
    }
    v2026_nav_link_publish_end();
    return result;
}

void v2026_nav_link_tick_10ms(void)
{
    v2026_nav_link_publish_begin();
    if(g_command_valid != 0U && g_age_ms <= 0xFFF5U)
    {
        g_age_ms += 10U;
    }
    v2026_nav_link_publish_end();
}

uint8_t v2026_nav_link_get_snapshot(v2026_nav_command_t *command,
                                    v2026_nav_link_status_t *status)
{
    uint8_t before;
    uint8_t after;
    uint8_t valid;
    uint16_t age_ms;
    uint16_t timeout_ms;

    do
    {
        before = g_publish_guard;
        if((before & 1U) != 0U)
        {
            continue;
        }
        valid = g_command_valid;
        age_ms = g_age_ms;
        timeout_ms = g_command.timeout_ms;
        if(command != 0)
        {
            command->sequence = g_command.sequence;
            command->flags = g_command.flags;
            command->linear_mm_s = g_command.linear_mm_s;
            command->angular_mdeg_s = g_command.angular_mdeg_s;
            command->target_heading_cdeg = g_command.target_heading_cdeg;
            command->timeout_ms = timeout_ms;
        }
        if(status != 0)
        {
            status->bytes_received = g_bytes_received;
            status->frames_ok = g_frames_ok;
            status->frames_bad = g_frames_bad;
            status->duplicate_frames = g_duplicate_frames;
            status->missing_frames = g_missing_frames;
            status->age_ms = age_ms;
        }
        after = g_publish_guard;
    }
    while(before != after || (after & 1U) != 0U);

    valid = (valid != 0U && age_ms <= timeout_ms) ? 1U : 0U;
    if(status != 0)
    {
        status->connected = valid;
    }
    return valid;
}
