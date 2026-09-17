from dataclasses import dataclass

from crc import crc16


SOF = b"\xAA\x55"
VERSION = 0x01

TYPE_REQUEST = 0x01
TYPE_RESPONSE = 0x02
TYPE_EVENT = 0x03
TYPE_ERROR = 0x04

MAX_PAYLOAD = 4096  # Audio data uses the full 4096-byte payload budget.


@dataclass
class Frame:
    frame_type: int
    cmd: int
    seq: int
    payload: bytes


def build_frame(
    frame_type: int,
    cmd: int,
    seq: int,
    payload: bytes = b"",
) -> bytes:
    if not 0 <= seq <= 255:
        raise ValueError("Sequence must be 0..255")

    if len(payload) > MAX_PAYLOAD:
        raise ValueError("Payload too large")

    header = bytes([
        VERSION,
        frame_type,
        cmd,
        seq,
        len(payload) & 0xFF,
        (len(payload) >> 8) & 0xFF,
    ])

    crc = crc16(header + payload)

    return (
        SOF
        + header
        + payload
        + bytes([
            crc & 0xFF,
            (crc >> 8) & 0xFF,
        ])
    )


class FrameParser:
    WAIT_SOF1 = 0
    WAIT_SOF2 = 1
    READ_HEADER = 2
    READ_PAYLOAD = 3
    READ_CRC = 4

    def __init__(self) -> None:
        self.reset()

    def reset(self) -> None:
        self.state = self.WAIT_SOF1
        self.header = bytearray()
        self.payload = bytearray()
        self.crc_bytes = bytearray()
        self.expected_length = 0

    def feed(self, data: bytes) -> list[Frame]:
        frames = []

        for byte_value in data:
            frame = self._feed_byte(byte_value)

            if frame is not None:
                frames.append(frame)

        return frames

    def _feed_byte(self, byte_value: int) -> Frame | None:

        if self.state == self.WAIT_SOF1:

            if byte_value == 0xAA:
                self.state = self.WAIT_SOF2

            return None

        if self.state == self.WAIT_SOF2:

            if byte_value == 0x55:
                self.header.clear()
                self.state = self.READ_HEADER

            elif byte_value == 0xAA:
                self.state = self.WAIT_SOF2

            else:
                self.state = self.WAIT_SOF1

            return None

        if self.state == self.READ_HEADER:

            self.header.append(byte_value)

            if len(self.header) == 6:

                version = self.header[0]

                self.expected_length = (
                    self.header[4]
                    | (self.header[5] << 8)
                )

                if version != VERSION:
                    self.reset()
                    return None

                if self.expected_length > MAX_PAYLOAD:
                    self.reset()
                    return None

                self.payload.clear()
                self.crc_bytes.clear()

                if self.expected_length == 0:
                    self.state = self.READ_CRC
                else:
                    self.state = self.READ_PAYLOAD

            return None

        if self.state == self.READ_PAYLOAD:

            self.payload.append(byte_value)

            if len(self.payload) == self.expected_length:
                self.state = self.READ_CRC

            return None

        if self.state == self.READ_CRC:

            self.crc_bytes.append(byte_value)

            if len(self.crc_bytes) == 2:

                header = bytes(self.header)
                payload = bytes(self.payload)

                received_crc = (
                    self.crc_bytes[0]
                    | (self.crc_bytes[1] << 8)
                )

                if crc16(header + payload) != received_crc:
                    self.reset()
                    return None

                frame = Frame(
                    frame_type=header[1],
                    cmd=header[2],
                    seq=header[3],
                    payload=payload,
                )

                self.reset()
                return frame

        return None
