from dataclasses import dataclass

from crc import crc16


SOF = b"\xAA\x55"
VERSION = 0x01

TYPE_REQUEST = 0x01
TYPE_RESPONSE = 0x02
TYPE_EVENT = 0x03
TYPE_ERROR = 0x04

HEADER_SIZE = 6
CRC_SIZE = 2
MAX_PAYLOAD = 4096


@dataclass
class Frame:
    frame_type: int
    cmd: int
    seq: int
    payload: bytes


@dataclass
class ParserStats:
    frames_ok: int = 0
    version_errors: int = 0
    length_errors: int = 0
    crc_errors: int = 0


def build_frame(
    frame_type: int,
    cmd: int,
    seq: int,
    payload: bytes = b"",
) -> bytes:
    if frame_type not in (TYPE_REQUEST, TYPE_RESPONSE, TYPE_EVENT, TYPE_ERROR):
        raise ValueError("Invalid frame type")

    if not 0 <= cmd <= 255:
        raise ValueError("Command/event ID must be 0..255")

    if not 0 <= seq <= 255:
        raise ValueError("Sequence must be 0..255")

    if len(payload) > MAX_PAYLOAD:
        raise ValueError("Payload too large")

    header = bytes(
        [
            VERSION,
            frame_type,
            cmd,
            seq,
            len(payload) & 0xFF,
            (len(payload) >> 8) & 0xFF,
        ]
    )

    crc = crc16(header + payload)

    return (
        SOF
        + header
        + payload
        + bytes([crc & 0xFF, (crc >> 8) & 0xFF])
    )


class FrameParser:
    WAIT_SOF1 = 0
    WAIT_SOF2 = 1
    READ_HEADER = 2
    READ_PAYLOAD = 3
    READ_CRC = 4

    def __init__(self) -> None:
        self.stats = ParserStats()
        self.reset()

    def reset(self) -> None:
        self.state = self.WAIT_SOF1
        self.header = bytearray()
        self.payload = bytearray()
        self.crc_bytes = bytearray()
        self.expected_length = 0

    def feed(self, data: bytes) -> list[Frame]:
        frames: list[Frame] = []

        for byte_value in data:
            frame = self._feed_byte(byte_value)
            if frame is not None:
                frames.append(frame)

        return frames

    def _feed_byte(self, byte_value: int) -> Frame | None:
        if self.state == self.WAIT_SOF1:
            if byte_value == SOF[0]:
                self.state = self.WAIT_SOF2
            return None

        if self.state == self.WAIT_SOF2:
            if byte_value == SOF[1]:
                self.header.clear()
                self.state = self.READ_HEADER
            elif byte_value == SOF[0]:
                self.state = self.WAIT_SOF2
            else:
                self.state = self.WAIT_SOF1
            return None

        if self.state == self.READ_HEADER:
            self.header.append(byte_value)

            if len(self.header) == HEADER_SIZE:
                if self.header[0] != VERSION:
                    self.stats.version_errors += 1
                    self.reset()
                    return None

                self.expected_length = (
                    self.header[4] | (self.header[5] << 8)
                )

                if self.expected_length > MAX_PAYLOAD:
                    self.stats.length_errors += 1
                    self.reset()
                    return None

                self.payload.clear()
                self.crc_bytes.clear()
                self.state = (
                    self.READ_CRC
                    if self.expected_length == 0
                    else self.READ_PAYLOAD
                )

            return None

        if self.state == self.READ_PAYLOAD:
            self.payload.append(byte_value)

            if len(self.payload) == self.expected_length:
                self.state = self.READ_CRC

            return None

        if self.state == self.READ_CRC:
            self.crc_bytes.append(byte_value)

            if len(self.crc_bytes) == CRC_SIZE:
                header = bytes(self.header)
                payload = bytes(self.payload)
                received_crc = self.crc_bytes[0] | (self.crc_bytes[1] << 8)

                if crc16(header + payload) != received_crc:
                    self.stats.crc_errors += 1
                    self.reset()
                    return None

                frame_type = self.header[1]
                if frame_type not in (
                    TYPE_REQUEST,
                    TYPE_RESPONSE,
                    TYPE_EVENT,
                    TYPE_ERROR,
                ):
                    self.reset()
                    return None

                frame = Frame(
                    frame_type=frame_type,
                    cmd=self.header[2],
                    seq=self.header[3],
                    payload=payload,
                )
                self.stats.frames_ok += 1
                self.reset()
                return frame

        return None
