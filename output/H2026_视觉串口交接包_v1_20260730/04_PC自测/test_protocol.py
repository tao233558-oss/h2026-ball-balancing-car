import pathlib
import sys


MODULE_DIR = pathlib.Path(__file__).resolve().parent / "k230"
sys.path.insert(0, str(MODULE_DIR))

import h2026_uart_protocol as protocol


def main():
    assert protocol.crc16_ccitt_false(b"123456789") == 0x29B1

    ball_frame = protocol.pack_ball_position(
        sequence=7,
        x_mm=-35,
        confidence_percent=92,
        valid=True,
        pipe_visible=True,
        frame_id=0x1234,
        processing_ms=17,
    )
    assert ball_frame.hex() == "aa5501100708ddff5c0334121100b84c"

    parser = protocol.FrameParser()
    frames = parser.feed(b"\x00\xff\xaa" + ball_frame)
    assert len(frames) == 1
    assert frames[0]["message_type"] == protocol.MSG_BALL_POSITION
    assert frames[0]["sequence"] == 7
    assert frames[0]["payload"].hex() == "ddff5c0334121100"

    corrupt = bytearray(ball_frame)
    corrupt[8] ^= 0x01
    assert protocol.FrameParser().feed(corrupt) == []

    status_payload = bytes.fromhex("fbff8102e2ff7b00a5002a00")
    status_frame = protocol.pack_frame(
        protocol.MSG_B_STATUS, 8, status_payload)
    combined = protocol.FrameParser().feed(ball_frame + status_frame)
    assert len(combined) == 2
    status = protocol.unpack_b_status(combined[1]["payload"])
    assert status == {
        "line_error": -5,
        "line_bits": 0x81,
        "flags": 0x02,
        "ball_x_mm": -30,
        "rod_angle_cdeg": 123,
        "fault_bits": 0x00A5,
        "vision_age_ms": 42,
    }

    print("H2026 UART protocol tests: PASS")
    print("BALL_POSITION example:", ball_frame.hex())


if __name__ == "__main__":
    main()
