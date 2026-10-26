#ifndef H2026_UART_PROTOCOL_V2_H
#define H2026_UART_PROTOCOL_V2_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define H2026_UART_SOF_0                     (0xAAU)
#define H2026_UART_SOF_1                     (0x55U)
#define H2026_UART_PROTOCOL_VERSION          (0x02U)
#define H2026_UART_MAX_PAYLOAD_SIZE          (32U)
#define H2026_UART_FRAME_OVERHEAD            (8U)
#define H2026_UART_MAX_FRAME_SIZE            (40U)
#define H2026_UART_INTER_BYTE_TIMEOUT_MS     (5U)

#define H2026_MSG_VISION_SAMPLE              (0x10U)
#define H2026_MSG_VISION_COMPACT             (0x11U)
#define H2026_MSG_B_STATUS                   (0x20U)
#define H2026_MSG_A_COMMAND                  (0x30U)

#define H2026_VISION_PAYLOAD_SIZE            (10U)
#define H2026_VISION_COMPACT_PAYLOAD_SIZE    (4U)
#define H2026_B_STATUS_PAYLOAD_SIZE          (26U)
#define H2026_A_COMMAND_PAYLOAD_SIZE         (10U)

/* VISION_SAMPLE flags. */
#define H2026_VISION_VALID                   (1U << 0)
#define H2026_VISION_CALIBRATED              (1U << 1)
#define H2026_VISION_IN_ROI                  (1U << 2)
#define H2026_VISION_SINGLE_TARGET           (1U << 3)
#define H2026_VISION_MODEL_READY             (1U << 4)
#define H2026_VISION_CAMERA_READY            (1U << 5)

/* B_STATUS flags. */
#define H2026_B_BALL_VALID                   (1U << 0)
#define H2026_B_LINE_VALID                   (1U << 1)
#define H2026_B_ACTUATOR_REFERENCE_VALID     (1U << 2)
#define H2026_B_ACTUATOR_HOMED               H2026_B_ACTUATOR_REFERENCE_VALID
#define H2026_B_CONTROL_ACTIVE               (1U << 3)
#define H2026_B_VISION_LINK_OK               (1U << 4)
#define H2026_B_A_LINK_OK                    (1U << 5)
#define H2026_B_HOME_SWITCH_ACTIVE           (1U << 6)

/* A_COMMAND modes. */
#define H2026_A_MODE_SAFE_IDLE               (0U)
#define H2026_A_MODE_HOME                    (1U)
#define H2026_A_MODE_BALANCE                 (2U)
#define H2026_A_MODE_LEVEL_HOLD              (3U)
#define H2026_A_MODE_DIAGNOSTIC              (4U)

/* A_COMMAND flags. */
#define H2026_A_ENABLE                       (1U << 0)
#define H2026_A_ESTOP                        (1U << 1)
#define H2026_A_CLEAR_FAULT                  (1U << 2)
#define H2026_A_ACCEL_VALID                  (1U << 3)

/* B state. */
#define H2026_B_STATE_BOOT                   (0U)
#define H2026_B_STATE_HOMING                 (1U)
#define H2026_B_STATE_READY                  (2U)
#define H2026_B_STATE_ACTIVE                 (3U)
#define H2026_B_STATE_DEGRADED               (4U)
#define H2026_B_STATE_FAULT                  (5U)
#define H2026_B_STATE_ESTOP                  (6U)

/* B_STATUS fault_bits. */
#define H2026_B_FAULT_VISION_LINK_TIMEOUT    (1U << 0)
#define H2026_B_FAULT_BALL_LOST              (1U << 1)
#define H2026_B_FAULT_A_COMMAND_TIMEOUT      (1U << 2)
#define H2026_B_FAULT_REFERENCE_INVALID      (1U << 3)
#define H2026_B_FAULT_REFERENCE_INIT_TIMEOUT (1U << 4)
#define H2026_B_FAULT_NOT_HOMED              H2026_B_FAULT_REFERENCE_INVALID
#define H2026_B_FAULT_HOME_TIMEOUT           H2026_B_FAULT_REFERENCE_INIT_TIMEOUT
#define H2026_B_FAULT_ACTUATOR_SOFT_LIMIT    (1U << 5)
#define H2026_B_FAULT_ACTUATOR_ENDSTOP       (1U << 6)
#define H2026_B_FAULT_ENDSTOP_CONFLICT       (1U << 7)
#define H2026_B_FAULT_LINE_INVALID           (1U << 8)
#define H2026_B_FAULT_UART_RX_OVERFLOW       (1U << 9)
#define H2026_B_FAULT_PROTOCOL_ERRORS        (1U << 10)
#define H2026_B_FAULT_ESTOP_ACTIVE           (1U << 11)
#define H2026_B_FAULT_STEPPER_POSITION       (1U << 12)

typedef enum
{
    H2026_PARSE_NONE = 0,
    H2026_PARSE_FRAME_OK,
    H2026_PARSE_FRAME_BAD
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
    uint8_t length;
    uint32_t last_byte_ms;
    bool last_byte_valid;
    uint32_t bad_frame_count;
} h2026_parser_t;

typedef struct
{
    int16_t x_mm;
    uint8_t confidence_percent;
    uint8_t flags;
    uint16_t frame_id;
    uint16_t sample_age_ms;
    uint16_t inference_ms;
} h2026_vision_sample_t;

typedef struct
{
    uint8_t mode;
    uint8_t flags;
    int16_t target_x_mm;
    int16_t vehicle_speed_ref_mm_s;
    int16_t vehicle_accel_ref_mm_s2;
    uint16_t command_id;
} h2026_a_command_t;

typedef struct
{
    uint8_t state;
    uint8_t flags;
    int16_t ball_x_mm;
    int16_t ball_error_mm;
    int32_t actuator_position_steps;
    int16_t control_output_permille;
    int16_t line_error;
    uint8_t line_bits;
    uint16_t fault_bits;
    uint16_t vision_rx_age_ms;
    uint16_t ball_valid_age_ms;
    uint16_t command_rx_age_ms;
    uint16_t command_id_echo;
} h2026_b_status_t;

uint16_t h2026_crc16_ccitt_false(const uint8_t *data, uint8_t length);

void h2026_parser_init(h2026_parser_t *parser);
void h2026_parser_reset(h2026_parser_t *parser);
h2026_parse_result_t h2026_parser_feed(
    h2026_parser_t *parser,
    uint8_t byte,
    uint32_t now_ms,
    h2026_frame_t *frame);

uint8_t h2026_protocol_encode(
    uint8_t message_type,
    uint8_t sequence,
    const uint8_t *payload,
    uint8_t payload_length,
    uint8_t *output,
    uint8_t output_capacity);

uint8_t h2026_pack_vision_sample(
    uint8_t payload[H2026_VISION_PAYLOAD_SIZE],
    const h2026_vision_sample_t *message);
uint8_t h2026_pack_a_command(
    uint8_t payload[H2026_A_COMMAND_PAYLOAD_SIZE],
    const h2026_a_command_t *message);
uint8_t h2026_pack_b_status(
    uint8_t payload[H2026_B_STATUS_PAYLOAD_SIZE],
    const h2026_b_status_t *message);

bool h2026_unpack_vision_sample(
    const h2026_frame_t *frame,
    h2026_vision_sample_t *message);
bool h2026_unpack_vision_compact(
    const h2026_frame_t *frame,
    h2026_vision_sample_t *message);
bool h2026_unpack_a_command(
    const h2026_frame_t *frame,
    h2026_a_command_t *message);
bool h2026_unpack_b_status(
    const h2026_frame_t *frame,
    h2026_b_status_t *message);

#ifdef __cplusplus
}
#endif

#endif
