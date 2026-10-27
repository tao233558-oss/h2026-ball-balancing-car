"""K230 UART2 sender matching the archived STM32 V17 ASCII parser.

Call publish once per NEW inference result. No queued/cached coordinate is
retransmitted unless a fresh valid detection is supplied by the caller.
"""
import time


def _ticks_ms():
    if hasattr(time, "ticks_ms"):
        return time.ticks_ms()
    return int(time.monotonic() * 1000)


def _ticks_diff(new, old):
    if hasattr(time, "ticks_diff"):
        return time.ticks_diff(new, old)
    return new - old


def encode_position(x, y):
    if isinstance(x, bool) or isinstance(y, bool):
        raise ValueError("coordinates must be integer pixels")
    ix, iy = int(x), int(y)
    if ix != x or iy != y or not (0 <= ix <= 799 and 0 <= iy <= 479):
        raise ValueError("expected integer x=0..799, y=0..479")
    return ("[%d,%d]\r\n" % (ix, iy)).encode()


def create_uart(tx_gpio=5, rx_gpio=6):
    from machine import FPIOA, UART
    fpioa = FPIOA()
    fpioa.set_function(tx_gpio, FPIOA.UART2_TXD)
    fpioa.set_function(rx_gpio, FPIOA.UART2_RXD)
    uart = UART(
        UART.UART2, baudrate=9600, bits=UART.EIGHTBITS,
        parity=UART.PARITY_NONE, stop=UART.STOPBITS_ONE, timeout=0,
    )
    return fpioa, uart


class VisionAsciiPublisher:
    def __init__(self, tx_gpio=5, rx_gpio=6, target_period_ms=50,
                 heartbeat_period_ms=100, uart=None, clock_ms=None,
                 ticks_diff=None):
        self._fpioa = None
        if uart is None:
            self._fpioa, uart = create_uart(tx_gpio, rx_gpio)
        self._uart = uart
        self._clock = clock_ms or _ticks_ms
        self._diff = ticks_diff or _ticks_diff
        self._period_ms = max(1, int(target_period_ms))
        self._heartbeat_ms = max(self._period_ms, int(heartbeat_period_ms))
        self._last_send_ms = None
        self._last_position = None
        self.sent_count = 0
        self.write_error_count = 0

    def publish(self, x, y, valid=True, now_ms=None):
        """Send a fresh absolute-pixel sample; invalid samples send nothing."""
        if not valid or self._uart is None:
            self._last_position = None
            return False
        try:
            packet = encode_position(x, y)
        except (ValueError, TypeError, OverflowError):
            self._last_position = None
            return False
        now = self._clock() if now_ms is None else int(now_ms)
        position = (int(x), int(y))
        if self._last_send_ms is not None:
            elapsed = self._diff(now, self._last_send_ms)
            if elapsed < self._period_ms:
                return False
            if position == self._last_position and elapsed < self._heartbeat_ms:
                return False
        try:
            written = self._uart.write(packet)
        except OSError:
            self.write_error_count += 1
            return False
        if written != len(packet):
            self.write_error_count += 1
            return False
        self._last_send_ms = now
        self._last_position = position
        self.sent_count += 1
        return True

    def deinit(self):
        if self._uart is not None:
            self._uart.deinit()
            self._uart = None
