import importlib.util
import math
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "vision/k230"))
from vision_ascii_uart import VisionAsciiPublisher, create_uart, encode_position
from target_selection import select_best_target, target_center


class FakeUart:
    def __init__(self):
        self.packets = []
        self.return_count = None
        self.raise_error = False
        self.closed = False

    def write(self, packet):
        if self.raise_error:
            raise OSError("UART write failed")
        self.packets.append(packet)
        return len(packet) if self.return_count is None else self.return_count

    def deinit(self):
        self.closed = True


class EncodingTests(unittest.TestCase):
    def test_exact_example_and_boundaries(self):
        self.assertEqual(encode_position(400, 240), bytes.fromhex("5b3430302c3234305d0d0a"))
        self.assertEqual(encode_position(0, 0), b"[0,0]\r\n")
        self.assertEqual(encode_position(799, 479), b"[799,479]\r\n")

    def test_reject_invalid_coordinates(self):
        for pair in [(-1, 0), (800, 0), (0, -1), (0, 480), (1.5, 2),
                     (True, 1), ("1", 2), (math.inf, 2), (math.nan, 2)]:
            with self.subTest(pair=pair), self.assertRaises((ValueError, OverflowError)):
                encode_position(*pair)

    def test_board_uart_mapping(self):
        functions = []

        class Fpioa:
            UART2_TXD, UART2_RXD = 50, 60

            def set_function(self, gpio, function):
                functions.append((gpio, function))

        class Uart:
            UART2, EIGHTBITS, PARITY_NONE, STOPBITS_ONE = 2, 8, None, 1

            def __init__(self, port, **kwargs):
                self.port, self.kwargs = port, kwargs

        from types import SimpleNamespace
        previous = sys.modules.get("machine")
        sys.modules["machine"] = SimpleNamespace(FPIOA=Fpioa, UART=Uart)
        try:
            _, uart = create_uart()
        finally:
            if previous is None:
                del sys.modules["machine"]
            else:
                sys.modules["machine"] = previous
        self.assertEqual(functions, [(5, 50), (6, 60)])
        self.assertEqual(uart.port, 2)
        self.assertEqual(uart.kwargs, {"baudrate": 9600, "bits": 8, "parity": None, "stop": 1, "timeout": 0})


class PublisherTests(unittest.TestCase):
    def setUp(self):
        self.uart = FakeUart()
        self.sender = VisionAsciiPublisher(uart=self.uart)

    def test_changed_target_limited_to_50_ms(self):
        self.assertTrue(self.sender.publish(400, 240, now_ms=0))
        self.assertFalse(self.sender.publish(401, 240, now_ms=49))
        self.assertTrue(self.sender.publish(402, 240, now_ms=50))
        self.assertEqual(self.uart.packets, [b"[400,240]\r\n", b"[402,240]\r\n"])

    def test_stationary_target_requires_fresh_detection_at_100_ms(self):
        self.assertTrue(self.sender.publish(400, 240, now_ms=0))
        self.assertFalse(self.sender.publish(400, 240, now_ms=50))
        self.assertFalse(self.sender.publish(400, 240, now_ms=99))
        self.assertTrue(self.sender.publish(400, 240, now_ms=100))

    def test_no_coordinates_when_target_is_missing(self):
        self.sender.publish(400, 240, now_ms=0)
        for now in (100, 200, 1000):
            self.assertFalse(self.sender.publish(None, None, valid=False, now_ms=now))
        self.assertEqual(len(self.uart.packets), 1)
        self.assertTrue(self.sender.publish(400, 240, now_ms=1100))

    def test_invalid_samples_never_send_old_position(self):
        self.sender.publish(1, 2, now_ms=0)
        self.assertFalse(self.sender.publish(800, 0, now_ms=500))
        self.assertFalse(self.sender.publish(None, 0, now_ms=600))
        self.assertEqual(self.uart.packets, [b"[1,2]\r\n"])

    def test_partial_write_does_not_mark_success(self):
        self.uart.return_count = 3
        self.assertFalse(self.sender.publish(1, 2, now_ms=0))
        self.assertEqual(self.sender.sent_count, 0)
        self.assertEqual(self.sender.write_error_count, 1)
        self.uart.return_count = None
        self.assertTrue(self.sender.publish(1, 2, now_ms=1))

    def test_oserror_is_counted(self):
        self.uart.raise_error = True
        self.assertFalse(self.sender.publish(1, 2, now_ms=0))
        self.assertEqual(self.sender.write_error_count, 1)

    def test_ticks_wrap(self):
        sender = VisionAsciiPublisher(uart=self.uart, ticks_diff=lambda a, b: ((a-b+512) % 1024)-512)
        self.assertTrue(sender.publish(1, 2, now_ms=1020))
        self.assertFalse(sender.publish(2, 2, now_ms=4))
        self.assertTrue(sender.publish(3, 2, now_ms=55))

    def test_closed_sender_does_not_write(self):
        self.sender.deinit()
        self.assertTrue(self.uart.closed)
        self.assertFalse(self.sender.publish(1, 2, now_ms=0))


class TargetTests(unittest.TestCase):
    def test_best_center_is_absolute_pixel_not_offset(self):
        result = ([[390, 230, 21, 21], [10, 20, 4, 4]], [0, 0], [0.9, 0.6])
        target = select_best_target(result)
        self.assertEqual(target_center(target), (400, 240))
        self.assertEqual(encode_position(*target_center(target)), b"[400,240]\r\n")

    def test_empty_result(self):
        for result in (None, [], ([], [], [])):
            self.assertIsNone(select_best_target(result))

    def test_invalid_or_low_score_boxes(self):
        for box, score in [([0, 0, 0, 5], .9), ([0, 0, -5, 5], .9),
                           ([0, 0, 4, 4], .1), ([0, 0, 4, 4], math.nan),
                           ([0, 0, 4, 4], 1.5), ([math.inf, 0, 4, 4], .9),
                           ([900, 0, 4, 4], .9), ([0, 0, 4], .9), (None, .9)]:
            with self.subTest(box=box, score=score):
                self.assertIsNone(select_best_target(([box], [0], [score])))

    def test_out_of_frame_candidate_does_not_hide_valid_candidate(self):
        result = ([[900, 0, 10, 10], [100, 100, 20, 20]], [0, 0], [.99, .8])
        self.assertEqual(target_center(select_best_target(result)), (110, 110))


class ArtifactTests(unittest.TestCase):
    def test_board_scripts_parse_without_importing_board_runtime(self):
        for path in (ROOT / "vision/k230").glob("*.py"):
            compile(path.read_text(encoding="utf-8"), str(path), "exec")

    def test_preserved_stream_and_labels_match_archive(self):
        for new, old in [
            ("vision/k230/SmoothWBCRtsp.py", "output/k230_deploy_gpio5_v2/libs/SmoothWBCRtsp.py"),
            ("vision/k230/yolo11_gq_classes.txt", "output/k230_sdcard_backup_20260730_1330/yolo11_gq_classes.txt"),
        ]:
            self.assertEqual((ROOT / new).read_bytes(), (ROOT / old).read_bytes())

    def test_reference_models_and_explicit_missing_yolo11(self):
        spec = importlib.util.spec_from_file_location("vision_assets", ROOT / "tools/verify_vision_assets.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        result = module.audit()
        self.assertTrue(all(item["matches_reference"] for item in result["reference_yolov8"]))
        self.assertFalse(result["yolo11"]["board_verified"])


HOST_CC = shutil.which("gcc") or shutil.which("clang")


@unittest.skipUnless(HOST_CC, "host gcc/clang required; CI runs the actual C parser")
class ActualStm32ParserTests(unittest.TestCase):
    def test_sender_frames_and_resynchronization_with_actual_c_parser(self):
        source = ROOT / "H2026_STM32F407/STM32F407_ProjectTemplate/protocol/vision_ascii"
        with tempfile.TemporaryDirectory() as temp:
            executable = Path(temp) / ("parser.exe" if sys.platform == "win32" else "parser")
            subprocess.run([HOST_CC, "-std=c99", "-Wall", "-Wextra", "-Werror", "-I", str(source),
                            str(source / "vision_ascii_parser.c"), str(ROOT / "tests/vision_ascii_harness.c"),
                            "-o", str(executable)], check=True)
            stream = b"noise[broken][800,1]" + encode_position(0, 0) + b"[1,[" + encode_position(400, 240) + encode_position(799, 479)
            result = subprocess.run([str(executable)], input=stream, capture_output=True, check=True)
            self.assertEqual(result.stdout.decode().splitlines(), ["0,0", "400,240", "799,479"])


if __name__ == "__main__":
    unittest.main()
