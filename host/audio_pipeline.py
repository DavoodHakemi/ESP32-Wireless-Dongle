from __future__ import annotations

from typing import Optional

import numpy as np


class LinearResampler:
    """Streaming linear PCM resampler used by the PC audio transport."""

    def __init__(self, input_rate: int, output_rate: int) -> None:
        if input_rate <= 0 or output_rate <= 0:
            raise ValueError("Sample rates must be positive")
        self.input_rate = input_rate
        self.output_rate = output_rate
        self.step = input_rate / output_rate
        self.position = 0.0
        self.previous: Optional[np.ndarray] = None

    def process(self, samples: np.ndarray) -> np.ndarray:
        if samples.ndim != 2:
            raise ValueError("Expected samples with shape (frames, channels)")
        if samples.size == 0:
            return np.empty((0, samples.shape[1]), dtype=np.int16)

        if self.previous is not None:
            work = np.vstack((self.previous[None, :], samples))
        else:
            work = samples

        if work.shape[0] < 2:
            self.previous = work[-1].copy()
            return np.empty((0, work.shape[1]), dtype=np.int16)

        positions = []
        pos = self.position
        limit = work.shape[0] - 1
        while pos < limit:
            positions.append(pos)
            pos += self.step

        self.position = pos - limit
        self.previous = work[-1].copy()

        if not positions:
            return np.empty((0, work.shape[1]), dtype=np.int16)

        p = np.asarray(positions, dtype=np.float64)
        i0 = np.floor(p).astype(np.int64)
        frac = (p - i0).reshape(-1, 1)
        out = work[i0] * (1.0 - frac) + work[i0 + 1] * frac
        return np.clip(np.rint(out), -32768, 32767).astype(np.int16)
