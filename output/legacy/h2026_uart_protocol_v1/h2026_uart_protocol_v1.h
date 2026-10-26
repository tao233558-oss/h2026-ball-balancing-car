#ifndef H2026_UART_PROTOCOL_H
#define H2026_UART_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define H2026_UART_BAUD_RATE          115200U
#define H2026_UART_SOF_0              0xAAU
#define H2026_UART_SOF_1              0x55U
#define H2026_UART_PROTOCOL_VERSION   0x01U
#define H2026_UART_MAX_PAYLOAD_SIZE   32U
#define H2026_UART_FRAME_OVERHEAD     8U
#define H2026_UART_MAX_FRAME_SIZE     \
    (H2026_UART_MAX_PAYLOAD_SIZE + H2026_UART_FRAME_OVERHEAD)

#define H2026_MSG_HEARTBEAT           0x01U
#define H2026_MSG_BALL_POSITION       0x10U
#define H2026_MSG_B_STATUS            0x20U
#define H2026_MSG_A_COMMAND           0x30U

#define H2026_HEARTBEAT_PAYLOAD_SIZE  6U
#define H2026_BALL_PAYLOAD_SIZE       8U
#define H2026_B_STATUS_PAYLOAD_SIZE   12U
#define H2026_A_COMMAND_PAYLOAD_SIZE  10U

#define H2026_BALL_FLAG_VALID         0x01U
#define H2026_BALL_FLAG_PIPE_VISIBLE  0x02U

#define H2026_STATUS_FLAG_BALL_VALID  0x01U
#define H2026_STATUS_FLAG_LINE_VALID  0x02U
#define H2026_STATUS_FLAG_ROD_HOMED   0x04U
#define H2026_STATUS_FLAG_FAULT       0x08U

#define H2026_COMMAND_FLAG_RUN_ENABLE 0x01U
#define H2026_COMMAND_FLAG_ESTOP      0x02U
#define H2026_COMMAND_FLAG_REHOME     0x04U

typedef enum
{
    H2026_ROLE_K230 = 1,
    H2026_ROLE_MAIN_A = 2,
    H2026_ROLE_CONTROL_B = 3,
} h2026_role_t;

typedef enum
{
    H2026_MODE_SAFE_IDLE = 0,
    H2026_MODE_HOLD_CENTER = 1,
    H2026_MODE_HOLD_TARGET = 2,
    H2026_MODE_HOME = 3,
    H2026_MODE_DIAG = 4,
} h2026_mode_t;

typedef enum
{
    H2026_PARSE_NONE = 0,
    H2026_PARSE_FRAME_OK,
    H2026_PARSE_FRAME_BAD,
} h2026_parse_result_t;

typedef struct
{
    uint8_t version;
    uint8_t message_type;
    uint8_t sequence;
    uint8_t payload_length;
    uint8_t payload[H2026_UART_MAX_PAYLOAD_SIZE];
} h2026_frame_t;

typedef struct
{
    uint8_t buffer[H2026_UART_MAX_FRAME_SIZE];
    uint8_t index;
    uint8_t expected_size;
} h2026_parser_t;

typedef struct
{
    uint32_t uptime_ms;
    uint8_t role;
    uint8_t state;
} h2026_heartbeat_t;

typedef struct
{
    int16_t x_mm;
    uint8_t confidence_percent;
    uint8_t flags;
    uint16_t frame_id;
    uint16_t processing_ms;
} h2026_ball_position_t;

typedef struct
{
    int16_t line_error;
    uint8_t line_bits;
    uint8_t flags;
    int16_t ball_x_mm;
    int16_t rod_angle_cdeg;
    uint16_t fault_bits;
    uint16_t vision_age_ms;
} h2026_b_status_t;

typedef struct
{
    uint8_t mode;
    uint8_t flags;
    int16_t target_x_mm;
    int16_t vehicle_speed_mm_s;
    int16_t vehicle_accel_mm_s2;
    uint16_t command_timeout_ms;
} h2026_a_command_t;

void h2026_protocol_init(h2026_parser_t *parser);

h2026_parse_result_t h2026_protocol_push(
    h2026_parser_t *parser,
    uint8_t byte,
    h2026_frame_t *frame);

uint8_t h2026_protocol_encode(
    uint8_t message_type,
    uint8_t sequence,
    const uint8_t *payload,
    uint8_t payload_length,
    uint8_t *output,
    uint8_t output_capacity);

uint16_t h2026_crc16_ccitt_false(const uint8_t *data, uint8_t length);

uint8_t h2026_pack_heartbeat(
    uint8_t payload[H2026_HEARTBEAT_PAYLOAD_SIZE],
    const h2026_heartbeat_t *message);
uint8_t h2026_pack_ball_position(
    uint8_t payload[H2026_BALL_PAYLOAD_SIZE],
    const h2026_ball_position_t *message);
uint8_t h2026_pack_b_status(
    uint8_t payload[H2026_B_STATUS_PAYLOAD_SIZE],
    const h2026_b_status_t *message);
uint8_t h2026_pack_a_command(
    uint8_t payload[H2026_A_COMMAND_PAYLOAD_SIZE],
    const h2026_a_command_t *message);

bool h2026_unpack_heartbeat(
    const h2026_frame_t *frame,
    h2026_heartbeat_t *message);
bool h2026_unpack_ball_position(
    const h2026_frame_t *frame,
    h2026_ball_position_t *message);
bool h2026_unpack_b_status(
    const h2026_frame_t *frame,
    h2026_b_status_t *message);
bool h2026_unpack_a_command(
    const h2026_frame_t *frame,
    h2026_a_command_t *message);

#endif
