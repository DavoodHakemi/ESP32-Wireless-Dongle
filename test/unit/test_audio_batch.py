import sys
import types
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "host"))

serial_stub = types.ModuleType("serial")
serial_stub.Serial = object
serial_stub.EIGHTBITS = 8
serial_stub.PARITY_NONE = "N"
serial_stub.STOPBITS_ONE = 1
sys.modules.setdefault("serial", serial_stub)

from host.esp32 import Esp32Device
from host.frame import FrameParser


class CaptureTransport:
    def __init__(self) -> None:
        self.data = b""

    def write_stream(self, data: bytes) -> None:
        self.data += data


def test_audio_send_batch_coalesces_multiple_wire_frames() -> None:
    transport = CaptureTransport()
    device = Esp32Device.__new__(Esp32Device)
    device.transport = transport
    device.sequence = 0
    device._audio_channels = 1

    block = bytes([0x50, 0x00, 0x01]) + bytes(256 * 2)
    device.audio_send_batch(block * 4)

    frames = FrameParser().feed(transport.data)
    assert len(frames) == 4
    assert all(frame.cmd == 0x6D for frame in frames)
    assert all(frame.payload == block for frame in frames)
    assert [frame.seq for frame in frames] == [0, 1, 2, 3]
