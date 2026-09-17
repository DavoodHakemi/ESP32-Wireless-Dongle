from __future__ import annotations

from abc import ABC, abstractmethod

class AudioTransport(ABC):
    """Transport-neutral sink for complete audio protocol payloads."""
    @abstractmethod
    def send_audio_payload(self, payload: bytes) -> None:
        raise NotImplementedError

class SerialAudioTransport(AudioTransport):
    """UART implementation of the transport-neutral audio sink."""
    def __init__(self, device) -> None:
        self.device = device

    def send_audio_payload(self, payload: bytes) -> None:
        self.device.audio_send_batch(payload)

# Future SPI transport should implement AudioTransport without changing PcAudioStreamer.
