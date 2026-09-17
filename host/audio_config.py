from __future__ import annotations

from dataclasses import dataclass

SUPPORTED_SAMPLE_RATES = (11025, 22050, 44100)
SUPPORTED_CHANNELS = (1, 2)
SUPPORTED_BITS = (16,)
AUDIO_BLOCK_SAMPLES = 256

@dataclass(frozen=True)
class AudioProfile:
    sample_rate: int = 22050
    channels: int = 1
    bits: int = 16

    def validate(self) -> None:
        if self.sample_rate not in SUPPORTED_SAMPLE_RATES:
            raise ValueError(f"Unsupported sample rate: {self.sample_rate}. Use one of {SUPPORTED_SAMPLE_RATES}.")
        if self.channels not in SUPPORTED_CHANNELS:
            raise ValueError(f"Unsupported channel count: {self.channels}. Use mono (1) or stereo (2).")
        if self.bits not in SUPPORTED_BITS:
            raise ValueError("Only 16-bit PCM is supported.")

DEFAULT_AUDIO_PROFILE = AudioProfile()

def format_audio_profile(profile: AudioProfile) -> str:
    channels = 'mono' if profile.channels == 1 else 'stereo'
    return f"{profile.sample_rate} Hz / {channels} / {profile.bits}-bit"
