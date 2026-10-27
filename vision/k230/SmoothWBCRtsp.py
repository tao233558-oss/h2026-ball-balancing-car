import time
import os
import sys
import multimedia as mm
from mpp import *
from media.vencoder import *
import _thread

from _media import Display


class SmoothRtspServer:
    """V1.6 WBC RTSP server with configurable encoder load."""

    def __init__(
        self,
        session_name="test",
        port=8554,
        video_type=mm.multi_media_type.media_h264,
        enable_audio=False,
        width=800,
        height=480,
        bit_rate=800,
        fps=12,
        gop_len=12
    ):
        self.session_name = session_name
        self.video_type = video_type
        self.enable_audio = enable_audio
        self.port = port
        self.bit_rate = bit_rate
        self.fps = fps
        self.gop_len = gop_len
        self.rtspserver = mm.rtsp_server()
        self.venc_chn = VENC_CHN_ID_0
        self.start_stream = False
        self.width = ALIGN_UP(width, 16)
        self.height = height
        self.encoder = Encoder()
        self.stream_data = StreamData()

        # Eight buffers is the value used by the official VENC examples.
        self.encoder.SetOutBufs(
            self.venc_chn, 8, self.width, self.height
        )

    def start(self):
        if self.start_stream:
            return

        # ChnAttrStr exposes bitrate, GOP and input/output frame-rate controls.
        chn_attr = ChnAttrStr(
            self.encoder.PAYLOAD_TYPE_H264,
            self.encoder.H264_PROFILE_MAIN,
            self.width,
            self.height,
            bit_rate=self.bit_rate,
            gopLen=self.gop_len,
            src_frame_rate=self.fps,
            dst_frame_rate=self.fps
        )
        self.encoder.Create(self.venc_chn, chn_attr)
        self.rtspserver.rtspserver_init(self.port)
        self.rtspserver.rtspserver_createsession(
            self.session_name, self.video_type, self.enable_audio
        )
        self.rtspserver.rtspserver_start()
        self.encoder.Start(self.venc_chn)
        self.start_stream = True

    def stop(self):
        if not self.start_stream:
            return

        self.start_stream = False

        # Drain already encoded data before destroying the encoder.
        while True:
            stream_data = StreamData()
            result = self.encoder.GetStream(
                self.venc_chn, stream_data, timeout=0
            )
            if result != 0:
                break
            self.encoder.ReleaseStream(self.venc_chn, stream_data)

        self.encoder.Stop(self.venc_chn)
        self.encoder.Destroy(self.venc_chn)
        self.rtspserver.rtspserver_stop()
        self.rtspserver.rtspserver_deinit()

    def get_rtsp_url(self):
        return self.rtspserver.rtspserver_getrtspurl(
            self.session_name
        )

    def send_video_frame(self, frame_info):
        if not self.start_stream:
            return -1

        # Never let the auxiliary video stream block the display/AI path.
        result = self.encoder.SendFrame(
            self.venc_chn, frame_info, timeout=20
        )
        if result != 0:
            return -1

        result = self.encoder.GetStream(
            self.venc_chn, self.stream_data, timeout=20
        )
        if result != 0:
            return -1

        try:
            for pack_index in range(self.stream_data.pack_cnt):
                self.rtspserver.rtspserver_sendvideodata_byphyaddr(
                    self.session_name,
                    self.stream_data.phy_addr[pack_index],
                    self.stream_data.data_size[pack_index],
                    1000
                )
        finally:
            self.encoder.ReleaseStream(
                self.venc_chn,
                self.stream_data,
            )
        return 0


class SmoothWBCRtsp:
    """Rate-limited LCD writeback RTSP helper for firmware v1.6."""

    _running = False
    _runthread_over = False
    rtspserver = None
    fps = 12
    last_error = None
    last_success_ms = None
    dropped_frames = 0

    @classmethod
    def _wbc_rtsp(cls):
        frame_interval_ms = max(1, 1000 // cls.fps)

        try:
            while cls._running:
                os.exitpoint()
                frame_start_ms = time.ticks_ms()

                frame_info = Display.writeback_dump(20)
                if frame_info:
                    if cls.rtspserver.send_video_frame(frame_info) == 0:
                        cls.last_success_ms = time.ticks_ms()
                    else:
                        cls.dropped_frames += 1
                else:
                    cls.dropped_frames += 1

                # Limit WBC capture and VENC submission to a stable target FPS.
                elapsed_ms = time.ticks_diff(
                    time.ticks_ms(), frame_start_ms
                )
                remaining_ms = frame_interval_ms - elapsed_ms
                if remaining_ms > 0:
                    time.sleep_ms(remaining_ms)
                else:
                    time.sleep_ms(1)
        except BaseException as error:
            cls.last_error = repr(error)
            print("Smooth WBC RTSP thread failed:", cls.last_error)
            sys.print_exception(error)
        finally:
            cls._runthread_over = True
            print("Smooth WBC RTSP thread over")

    @classmethod
    def configure(
        cls,
        wbc_width=480,
        wbc_height=800,
        bit_rate=800,
        fps=12,
        gop_len=12
    ):
        if not Display.inited():
            raise RuntimeError("start wbc before Display.init()")

        cls.fps = fps
        cls.last_error = None
        cls.last_success_ms = None
        cls.dropped_frames = 0
        cls._running = False
        cls._runthread_over = False

        cls.rtspserver = SmoothRtspServer(
            session_name="test",
            port=8554,
            video_type=mm.multi_media_type.media_h264,
            enable_audio=False,
            width=Display.width(),
            height=Display.height(),
            bit_rate=bit_rate,
            fps=fps,
            gop_len=gop_len
        )

    @classmethod
    def start(cls):
        if cls._running:
            return

        if not Display.writeback(True):
            raise RuntimeError("start wbc failed")

        cls.rtspserver.start()
        cls._running = True
        print(
            "Smooth RTSP server started:",
            cls.rtspserver.get_rtsp_url()
        )
        _thread.start_new_thread(cls._wbc_rtsp, ())

    @classmethod
    def is_healthy(cls, max_stall_ms=3000):
        if cls.last_error is not None or cls._runthread_over:
            return False
        if cls.last_success_ms is None:
            return cls._running
        return (
            time.ticks_diff(time.ticks_ms(), cls.last_success_ms)
            <= int(max_stall_ms)
        )

    @classmethod
    def stop(cls):
        if cls.rtspserver is None:
            return

        cls._running = False
        stop_start_ms = time.ticks_ms()
        while (
                not cls._runthread_over
                and time.ticks_diff(time.ticks_ms(), stop_start_ms) < 3000):
            time.sleep_ms(50)

        if not cls._runthread_over:
            print("Smooth WBC RTSP stop timeout")
            return

        if not Display.writeback(False):
            print("stop wbc failed")

        cls.rtspserver.stop()
        print("Smooth WBC RTSP stopped")
