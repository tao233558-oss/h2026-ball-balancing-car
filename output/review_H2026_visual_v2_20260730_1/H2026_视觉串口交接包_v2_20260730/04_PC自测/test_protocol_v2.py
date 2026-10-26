import pathlib
import sys


MODULE_DIR = pathlib.Path(__file__).resolve().parents[1] / "01_协议"
sys.path.insert(0, str(MODULE_DIR))

import h2026_uart_protocol_v2 as protocol


def assert_single_frame(raw, message_type, unpacker):
    parser = protocol.FrameParser()
    frames = parser.feed(raw)
    assert len(frames) == 1
    assert frames[0]["version"] == protocol.VERSION
    assert frames[0]["message_type"] == message_type
    return unpacker(frames[0]["payload"])


def main():
    assert protocol.crc16_ccitt_false(b"123456789") == 0x29B1

    vision_flags = (
        protocol.VISION_FLAG_VALID
        | protocol.VISION_FLAG_CALIBRATED
        | protocol.VISION_FLAG_IN_ROI
        | protocol.VISION_FLAG_SINGLE_TARGET
        | protocol.VISION_FLAG_MODEL_READY
        | protocol.VISION_FLAG_CAMERA_READY
    )
    vision_frame = protocol.pack_vision_sample(
        sequence=7,
        x_mm=-35,
        confidence_percent=92,
        flags=vision_flags,
        frame_id=0x1234,
        sample_age_ms=12,
        inference_ms=37,
    )
    vision = assert_single_frame(
        vision_frame,
        protocol.MSG_VISION_SAMPLE,
        protocol.unpack_vision_sample,
    )
    assert vision == {
        "x_mm": -35,
        "confidence_percent": 92,
        "flags": 0x3F,
        "frame_id": 0x1234,
        "sample_age_ms": 12,
        "inference_ms": 37,
    }

    command_frame = protocol.pack_a_command(
        sequence=8,
        mode=protocol.A_MODE_BALANCE,
        flags=protocol.A_FLAG_ENABLE | protocol.A_FLAG_ACCEL_VALID,
        target_x_mm=50,
        vehicle_speed_ref_mm_s=600,
        vehicle_accel_ref_mm_s2=120,
        command_id=0x0201,
    )
    command = assert_single_frame(
        command_frame,
        protocol.MSG_A_COMMAND,
        protocol.unpack_a_command,
    )
    assert command["target_x_mm"] == 50
    assert command["command_id"] == 0x0201

    b_flags = (
        protocol.B_FLAG_BALL_VALID
        | protocol.B_FLAG_LINE_VALID
        | protocol.B_FLAG_ACTUATOR_REFERENCE_VALID
        | protocol.B_FLAG_CONTROL_ACTIVE
        | protocol.B_FLAG_VISION_LINK_OK
        | protocol.B_FLAG_A_LINK_OK
    )
    status_frame = protocol.pack_b_status(
        sequence=9,
        state=protocol.B_STATE_ACTIVE,
        flags=b_flags,
        ball_x_mm=46,
        ball_error_mm=4,
        actuator_position_steps=12345,
        control_output_permille=-120,
        line_error=250,
        line_bits=0x18,
        fault_bits=0,
        vision_rx_age_ms=8,
        ball_valid_age_ms=18,
        command_rx_age_ms=6,
        command_id_echo=0x0201,
    )
    status = assert_single_frame(
        status_frame,
        protocol.MSG_B_STATUS,
        protocol.unpack_b_status,
    )
    assert status["state"] == protocol.B_STATE_ACTIVE
    assert status["actuator_position_steps"] == 12345
    assert status["command_id_echo"] == 0x0201

    # Arbitrary fragmentation and leading noise.
    parser = protocol.FrameParser()
    combined = b"\x00\xff\xaa" + vision_frame + command_frame + status_frame
    frames = []
    for index in range(0, len(combined), 3):
        frames.extend(parser.feed(combined[index:index + 3]))
    assert [frame["message_type"] for frame in frames] == [
        protocol.MSG_VISION_SAMPLE,
        protocol.MSG_A_COMMAND,
        protocol.MSG_B_STATUS,
    ]

    # A plausible false 32-byte header must not swallow following valid frames.
    false_long_header = bytes((
        protocol.SOF0,
        protocol.SOF1,
        protocol.VERSION,
        protocol.MSG_VISION_SAMPLE,
        0,
        protocol.MAX_PAYLOAD,
    ))
    parser = protocol.FrameParser()
    recovered = parser.feed(false_long_header + vision_frame * 4)
    assert len(recovered) == 4
    assert parser.bad_frame_count >= 1

    # Corruption and wrong protocol version are rejected.
    corrupt = bytearray(vision_frame)
    corrupt[8] ^= 0x01
    assert protocol.FrameParser().feed(corrupt) == []

    wrong_version = bytearray(vision_frame)
    wrong_version[2] = 1
    assert protocol.FrameParser().feed(wrong_version) == []

    # An inter-byte gap discards a stale half-frame.
    parser = protocol.FrameParser(inter_byte_timeout_ms=5)
    assert parser.feed(vision_frame[:6], now_ms=100) == []
    assert parser.feed(vision_frame[6:], now_ms=110) == []
    assert len(parser.feed(vision_frame, now_ms=111)) == 1

    print("H2026 UART protocol v2 tests: PASS")
    print("VISION_SAMPLE:", vision_frame.hex())
    print("A_COMMAND:", command_frame.hex())
    print("B_STATUS:", status_frame.hex())


if __name__ == "__main__":
    main()
