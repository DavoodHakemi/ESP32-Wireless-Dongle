from pathlib import Path

import numpy as np

from host.audio_config import AUDIO_BLOCK_SAMPLES
from host.audio_pipeline import LinearResampler

ROOT = Path(__file__).resolve().parents[2]


def test_default_capture_ratio_produces_exact_half_rate_blocks() -> None:
    resampler = LinearResampler(44100, 22050)
    counts = []

    for _ in range(12):
        output = resampler.process(np.zeros((256, 1), dtype=np.int16))
        counts.append(output.shape[0])

    assert counts == [128] * 12
    assert sum(counts) == 6 * AUDIO_BLOCK_SAMPLES


def test_resampler_preserves_stereo_shape() -> None:
    resampler = LinearResampler(44100, 22050)
    samples = np.zeros((256, 2), dtype=np.int16)

    output = resampler.process(samples)

    assert output.shape == (128, 2)
    assert output.dtype == np.int16


def test_audio_transport_uses_low_jitter_capture_settings() -> None:
    source = (ROOT / "host/audio.py").read_text(encoding="utf-8")

    assert "CAPTURE_RATE = 44100" in source
    assert "CAPTURE_RECORD_FRAMES = 256" in source
    assert "CAPTURE_RECORDER_BLOCKSIZE = 1024" in source
    assert "AUDIO_QUEUE_MAX_BLOCKS = 8" in source
    assert "block_interval = AUDIO_BLOCK_SAMPLES / float(self.profile.sample_rate)" in source
    assert "self.block_queue.put_nowait(packet)" in source
