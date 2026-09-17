"""Windows system-audio capture for the ESP32 A2DP audio bridge.

This version uses SoundCard's native Windows/WASAPI loopback backend.
The streaming path captures Windows system audio as float PCM, converts it to
mono signed 16-bit PCM, resamples to 22.05 kHz, and sends it uncompressed over
the fixed 921600-baud serial link.
"""

from __future__ import annotations

import threading
import time
import ctypes
import sys
import queue
import warnings
from typing import Callable, Optional

from audio_config import AudioProfile, DEFAULT_AUDIO_PROFILE, SUPPORTED_CHANNELS, AUDIO_BLOCK_SAMPLES

import numpy as np
import soundcard as sc


CAPTURE_RATE = 48000
CAPTURE_BLOCK_FRAMES = 1024
CAPTURE_RECORD_FRAMES = 1024
CAPTURE_RECORDER_BLOCKSIZE = 2048
AUDIO_QUEUE_MAX_BLOCKS = 256
PROBE_SECONDS = 0.35
LIVE_MONITOR_SECONDS = 3.0
SELF_TEST_SECONDS = 2.0
SELF_TEST_RATE = 48000
SELF_TEST_FREQ = 1000.0

# SoundCard can report recoverable WASAPI discontinuities on some Windows systems.
# Suppress the warning flood during continuous streaming; the stream remains alive.
warnings.filterwarnings(
    "ignore",
    message="data discontinuity in recording",
    category=RuntimeWarning,
    module=r"soundcard\.mediafoundation",
)

_preferred_loopback_id: Optional[str] = None


class LinearResampler:
    """Streaming linear PCM resampler for stereo int16 audio."""

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


def _speaker_name_and_id(speaker) -> tuple[str, str]:
    return str(getattr(speaker, "name", "")), str(getattr(speaker, "id", ""))


def _get_loopback_for_speaker(speaker):
    """Return the SoundCard loopback microphone for a speaker."""
    speaker_name, speaker_id = _speaker_name_and_id(speaker)

    # Prefer exact backend ID matching. This is more reliable than name-only
    # matching when virtual devices have similar names.
    try:
        mic = sc.get_microphone(speaker_id, include_loopback=True)
        if getattr(mic, "isloopback", False):
            return mic
    except Exception:
        pass

    # Fallback: select a loopback microphone whose name matches the speaker.
    for mic in sc.all_microphones(include_loopback=True):
        if not getattr(mic, "isloopback", False):
            continue
        mic_name = str(getattr(mic, "name", ""))
        if speaker_name and (speaker_name in mic_name or mic_name in speaker_name):
            return mic

    raise RuntimeError(f"No WASAPI loopback found for default speaker: {speaker_name}")


def get_loopback_devices() -> list[dict]:
    """Return available Windows loopback devices using SoundCard/WASAPI."""
    devices = []
    for idx, mic in enumerate(sc.all_microphones(include_loopback=True)):
        if not getattr(mic, "isloopback", False):
            continue
        devices.append({
            "index": idx,
            "name": str(getattr(mic, "name", "")),
            "id": str(getattr(mic, "id", "")),
            "channels": int(getattr(mic, "channels", 0) or 0),
            "object": mic,
        })
    return devices


def get_default_output_and_loopback() -> tuple[dict, dict]:
    """Return current default speaker and its native WASAPI loopback."""
    speaker = sc.default_speaker()
    loopback = _get_loopback_for_speaker(speaker)
    speaker_name, speaker_id = _speaker_name_and_id(speaker)
    loopback_name = str(getattr(loopback, "name", ""))
    loopback_id = str(getattr(loopback, "id", ""))
    output = {"index": -1, "name": speaker_name, "id": speaker_id}
    lb = {
        "index": -1,
        "name": loopback_name,
        "id": loopback_id,
        "channels": int(getattr(loopback, "channels", 0) or 0),
        "object": loopback,
    }
    return output, lb


def get_default_loopback() -> dict:
    return get_default_output_and_loopback()[1]


def describe_default_audio_path() -> tuple[dict, dict]:
    return get_default_output_and_loopback()


def _find_mic_by_id(device_id: str):
    for item in sc.all_microphones(include_loopback=True):
        if getattr(item, "isloopback", False) and str(getattr(item, "id", "")) == device_id:
            return item
    return None


def set_preferred_loopback_by_index(index: int) -> dict:
    """Select a SoundCard loopback device by its displayed list index."""
    global _preferred_loopback_id
    devices = get_loopback_devices()
    for device in devices:
        if int(device["index"]) == int(index):
            _preferred_loopback_id = str(device["id"])
            return dict(device)
    raise ValueError(f"Invalid WASAPI loopback index: {index}")


def get_preferred_loopback() -> Optional[dict]:
    """Return the explicitly selected loopback device, if any."""
    if not _preferred_loopback_id:
        return None
    mic = _find_mic_by_id(_preferred_loopback_id)
    if mic is None:
        return None
    return {
        "index": -1,
        "name": str(getattr(mic, "name", "")),
        "id": str(getattr(mic, "id", "")),
        "channels": int(getattr(mic, "channels", 0) or 0),
        "object": mic,
    }


def _probe_soundcard_loopback(device, seconds: float) -> tuple[int, int, int]:
    """Capture a short block and return byte count, peak and RMS."""
    # SoundCard returns normalized float32 PCM in the range [-1, 1].
    frames = max(128, int(min(CAPTURE_RATE * seconds, CAPTURE_RATE * 0.5)))
    with device.recorder(
        samplerate=CAPTURE_RATE,
        channels=2,
        blocksize=CAPTURE_BLOCK_FRAMES,
        exclusive_mode=False,
    ) as recorder:
        data = recorder.record(numframes=frames)

    samples = np.asarray(data, dtype=np.float32)
    if samples.ndim == 1:
        samples = samples.reshape(-1, 1)
    if samples.size == 0:
        return 0, 0, 0
    if samples.shape[1] == 1:
        samples = np.repeat(samples, 2, axis=1)
    else:
        samples = samples[:, :2]
    samples = np.nan_to_num(samples, nan=0.0, posinf=0.0, neginf=0.0)
    clipped = np.clip(samples, -1.0, 1.0)
    peak = int(np.max(np.abs(clipped)) * 32767.0)
    rms = int(round(float(np.sqrt(np.mean(clipped * clipped))) * 32767.0))
    return int(clipped.shape[0] * 2 * 2), peak, rms


def _speaker_for_loopback(loopback):
    """Find the speaker object corresponding to a loopback microphone."""
    lb_id = str(getattr(loopback, "id", ""))
    lb_name = str(getattr(loopback, "name", ""))
    speakers = sc.all_speakers()
    for speaker in speakers:
        if lb_id and str(getattr(speaker, "id", "")) == lb_id:
            return speaker
    for speaker in speakers:
        name = str(getattr(speaker, "name", ""))
        if name and (name in lb_name or lb_name in name):
            return speaker
    return None

def _make_test_tone(frames: int, rate: int = SELF_TEST_RATE, freq: float = SELF_TEST_FREQ) -> np.ndarray:
    t = (np.arange(frames, dtype=np.float32) / float(rate))
    tone = 0.20 * np.sin(2.0 * np.pi * freq * t)
    return np.column_stack((tone, tone)).astype(np.float32)

def self_test_loopback() -> list[dict]:
    """Play a locally generated tone through each speaker and capture its loopback."""
    results = []
    speakers = sc.all_speakers()
    for speaker in speakers:
        speaker_name = str(getattr(speaker, "name", ""))
        speaker_id = str(getattr(speaker, "id", ""))
        try:
            loopback = _get_loopback_for_speaker(speaker)
            # Capture and playback must overlap. The generated tone removes dependency
            # on browser/player routing and proves the Windows endpoint itself.
            captured = []
            stop_play = threading.Event()

            def _player() -> None:
                try:
                    tone = _make_test_tone(1024)
                    with speaker.player(samplerate=SELF_TEST_RATE, channels=2, blocksize=1024, exclusive_mode=False) as player:
                        end = time.monotonic() + SELF_TEST_SECONDS
                        while time.monotonic() < end and not stop_play.is_set():
                            player.play(tone)
                finally:
                    stop_play.set()

            thread = threading.Thread(target=_player, daemon=True)
            thread.start()
            with loopback.recorder(samplerate=SELF_TEST_RATE, channels=2, blocksize=1024, exclusive_mode=False) as recorder:
                end = time.monotonic() + SELF_TEST_SECONDS
                while time.monotonic() < end:
                    data = recorder.record(numframes=1024)
                    if data is not None:
                        arr = np.asarray(data, dtype=np.float32)
                        if arr.size:
                            captured.append(arr)
            stop_play.set()
            thread.join(timeout=1.0)
            if not captured:
                results.append({"speaker": speaker_name, "speaker_id": speaker_id, "loopback": str(getattr(loopback, "name", "")), "captured_bytes": 0, "peak": 0, "rms": 0, "error": "no frames captured"})
                continue
            samples = np.concatenate(captured, axis=0)
            samples = np.nan_to_num(samples, nan=0.0, posinf=0.0, neginf=0.0)
            samples = np.clip(samples, -1.0, 1.0)
            peak = int(np.max(np.abs(samples)) * 32767.0)
            rms = int(round(float(np.sqrt(np.mean(samples * samples))) * 32767.0))
            results.append({"speaker": speaker_name, "speaker_id": speaker_id, "loopback": str(getattr(loopback, "name", "")), "captured_bytes": int(samples.shape[0] * 2 * 2), "peak": peak, "rms": rms, "error": None})
        except Exception as exc:
            results.append({"speaker": speaker_name, "speaker_id": speaker_id, "loopback": "", "captured_bytes": 0, "peak": 0, "rms": 0, "error": str(exc)})
    return results

def probe_loopback_devices(seconds_per_device: float = PROBE_SECONDS) -> list[dict]:
    """Probe all SoundCard WASAPI loopbacks without process/thread isolation."""
    results = []
    for device in get_loopback_devices():
        entry = dict(device)
        entry.pop("object", None)
        device_obj = _find_mic_by_id(str(device["id"]))
        if device_obj is None:
            entry.update(captured_bytes=0, peak=0, rms=0, error="Loopback device object not found")
            results.append(entry)
            continue
        try:
            captured, peak, rms = _probe_soundcard_loopback(device_obj, seconds_per_device)
            entry.update(captured_bytes=captured, peak=peak, rms=rms, error=None)
        except Exception as exc:
            entry.update(captured_bytes=0, peak=0, rms=0, error=str(exc))
        results.append(entry)
    return results


def choose_active_loopback(probe_seconds: float = PROBE_SECONDS) -> tuple[dict, list[dict]]:
    """Probe loopbacks for diagnostics, but never infer the real route from a generated tone."""
    global _preferred_loopback_id

    _, default_loopback = get_default_output_and_loopback()
    results = probe_loopback_devices(probe_seconds)

    # A generic probe is intentionally diagnostic only. It must not silently
    # switch the user's Windows route because an endpoint happens to be nonzero.
    selected = dict(default_loopback)
    _preferred_loopback_id = str(selected.get("id", ""))
    return dict(selected), results


def monitor_real_audio(seconds_per_device: float = LIVE_MONITOR_SECONDS) -> list[dict]:
    """Monitor actual Windows playback on each loopback while user audio is playing."""
    results = []
    for device in get_loopback_devices():
        entry = {k: v for k, v in device.items() if k != "object"}
        obj = _find_mic_by_id(str(device["id"]))
        if obj is None:
            entry.update(captured_bytes=0, peak=0, rms=0, error="Loopback device object not found")
            results.append(entry)
            continue

        peak_max = 0
        rms_values = []
        captured_bytes = 0
        try:
            with obj.recorder(
                samplerate=CAPTURE_RATE,
                channels=2,
                blocksize=CAPTURE_BLOCK_FRAMES,
                exclusive_mode=False,
            ) as recorder:
                deadline = time.monotonic() + seconds_per_device
                while time.monotonic() < deadline:
                    data = recorder.record(numframes=None)
                    if data is None:
                        time.sleep(0.01)
                        continue
                    arr = np.asarray(data, dtype=np.float32)
                    if arr.size == 0:
                        time.sleep(0.005)
                        continue
                    arr = np.nan_to_num(arr, nan=0.0, posinf=0.0, neginf=0.0)
                    arr = np.clip(arr, -1.0, 1.0)
                    peak = int(np.max(np.abs(arr)) * 32767.0)
                    rms = int(round(float(np.sqrt(np.mean(arr * arr))) * 32767.0))
                    peak_max = max(peak_max, peak)
                    rms_values.append(rms)
                    captured_bytes += int(arr.shape[0] * max(1, arr.shape[1] if arr.ndim > 1 else 1) * 4)
            rms = int(round(sum(rms_values) / len(rms_values))) if rms_values else 0
            entry.update(captured_bytes=captured_bytes, peak=peak_max, rms=rms, error=None)
        except Exception as exc:
            entry.update(captured_bytes=captured_bytes, peak=peak_max, rms=0, error=str(exc))
        results.append(entry)
    return results


def get_preferred_loopback_index() -> Optional[int]:
    """Compatibility helper retained for older callers."""
    return None


def get_preferred_loopback_id() -> Optional[str]:
    return _preferred_loopback_id


class PcAudioStreamer:
    """Capture Windows system output and send raw 22.05 kHz mono PCM16 blocks to the ESP32."""

    def __init__(self, send_batch: Callable[[bytes], None], profile: AudioProfile = DEFAULT_AUDIO_PROFILE) -> None:
        profile.validate()
        self.send_batch = send_batch
        self.profile = profile
        self.capture_stop_event = threading.Event()
        self.capture_done_event = threading.Event()
        self.worker: Optional[threading.Thread] = None
        self.sender: Optional[threading.Thread] = None
        self.block_queue: queue.Queue[bytes] = queue.Queue(maxsize=AUDIO_QUEUE_MAX_BLOCKS)
        self.error: Optional[Exception] = None
        self.input_rate = 0
        self.input_channels = 0
        self.sent_bytes = 0
        self.sent_pcm_bytes = 0
        self.sent_blocks = 0
        self.dropped_chunks = 0
        self.captured_frames = 0
        self.last_peak = 0
        self.last_rms = 0
        self._recorder = None
        self._loopback = None
        self._selected_route_name = ""
        self._loopback_id = ""

    def set_profile(self, profile: AudioProfile) -> None:
        if self.is_running():
            raise RuntimeError("Stop PC audio before changing the audio profile")
        profile.validate()
        self.profile = profile

    def is_running(self) -> bool:
        return (
            (self.worker is not None and self.worker.is_alive())
            or (self.sender is not None and self.sender.is_alive())
        )

    def start(self, device_index: Optional[int] = None) -> None:
        del device_index
        if self.is_running():
            raise RuntimeError("PC audio capture is already running")

        self.capture_stop_event.clear()
        self.capture_done_event.clear()
        self.error = None
        self.sent_bytes = 0
        self.sent_pcm_bytes = 0
        self.sent_blocks = 0
        self.dropped_chunks = 0
        self.captured_frames = 0
        self.last_peak = 0
        self.last_rms = 0
        self.input_rate = CAPTURE_RATE
        self.input_channels = 0
        self._recorder = None
        self._loopback = None
        self.block_queue = queue.Queue(maxsize=AUDIO_QUEUE_MAX_BLOCKS)

        try:
            preferred = get_preferred_loopback()
            if preferred is not None:
                self._loopback = preferred["object"]
                self._selected_route_name = str(getattr(self._loopback, "name", ""))
            else:
                speaker = sc.default_speaker()
                self._loopback = _get_loopback_for_speaker(speaker)
                self._selected_route_name = str(getattr(self._loopback, "name", ""))
            self._loopback_id = str(getattr(self._loopback, "id", ""))
        except Exception as exc:
            raise RuntimeError(f"WASAPI system-audio route unavailable: {exc}") from exc

        self.worker = threading.Thread(
            target=self._run_capture,
            name="PC-Audio-Capture",
            daemon=True,
        )
        self.sender = threading.Thread(
            target=self._run_sender,
            name="PC-Audio-Sender",
            daemon=True,
        )
        self.sender.start()
        self.worker.start()

    def stop(self, timeout: float = 3.0) -> None:
        self.capture_stop_event.set()
        if self.worker is not None:
            self.worker.join(timeout=timeout)
        # Capture is done; the sender is allowed to drain queued audio.
        if self.sender is not None:
            self.sender.join(timeout=timeout)
        self.worker = None
        self.sender = None
        self._recorder = None

    @staticmethod
    def _float_to_int16(data: np.ndarray, output_channels: int) -> np.ndarray:
        samples = np.asarray(data, dtype=np.float32)
        if samples.ndim == 1:
            samples = samples.reshape(-1, 1)
        samples = np.nan_to_num(samples, nan=0.0, posinf=0.0, neginf=0.0)
        samples = np.clip(samples, -1.0, 1.0)
        if output_channels == 1:
            samples = np.mean(samples, axis=1, keepdims=True) if samples.shape[1] > 1 else samples[:, :1]
        else:
            if samples.shape[1] == 1:
                samples = np.repeat(samples, 2, axis=1)
            else:
                samples = samples[:, :2]
        return np.rint(samples * 32767.0).astype(np.int16)

    def _run_sender(self) -> None:
        try:
            while True:
                try:
                    block = self.block_queue.get(timeout=0.05)
                except queue.Empty:
                    if self.capture_done_event.is_set():
                        break
                    continue

                if not block:
                    continue
                self.send_batch(block)
                self.sent_bytes += len(block)
                self.sent_pcm_bytes += len(block) - 3
                self.sent_blocks += 1
        except Exception as exc:
            self.error = exc
            self.capture_stop_event.set()
        finally:
            self.capture_done_event.set()

    def _run_capture(self) -> None:
        com_initialized = False
        try:
            if sys.platform.startswith("win"):
                ole32 = ctypes.windll.ole32
                hr = ole32.CoInitializeEx(None, 0x0)
                if hr not in (0, 1):
                    raise RuntimeError(f"CoInitializeEx failed: 0x{hr & 0xFFFFFFFF:08X}")
                com_initialized = True

            loopback = None
            if self._loopback_id:
                loopback = sc.get_microphone(self._loopback_id, include_loopback=True)
                if not getattr(loopback, "isloopback", False):
                    loopback = None
            if loopback is None:
                loopback = self._loopback
            if loopback is None:
                raise RuntimeError("No WASAPI loopback device selected")

            self._selected_route_name = str(getattr(loopback, "name", self._selected_route_name))

            # A larger WASAPI blocksize with smaller record reads reduces callback-like
            # churn while keeping end-to-end latency reasonable.
            with warnings.catch_warnings():
                warnings.filterwarnings(
                    "ignore",
                    message="data discontinuity in recording",
                    category=RuntimeWarning,
                    module=r"soundcard\.mediafoundation",
                )
                with loopback.recorder(
                    samplerate=CAPTURE_RATE,
                    channels=2,
                    blocksize=CAPTURE_RECORDER_BLOCKSIZE,
                    exclusive_mode=False,
                ) as recorder:
                    self._recorder = recorder
                    resampler = LinearResampler(CAPTURE_RATE, self.profile.sample_rate)
                    block_samples = AUDIO_BLOCK_SAMPLES
                    self.input_channels = 2
                    pending = bytearray()

                    while not self.capture_stop_event.is_set():
                        data = recorder.record(numframes=CAPTURE_RECORD_FRAMES)
                        if data is None:
                            time.sleep(0.001)
                            continue
                        data = np.asarray(data, dtype=np.float32)
                        if data.size == 0:
                            continue

                        safe = np.nan_to_num(data, nan=0.0, posinf=0.0, neginf=0.0)
                        safe = np.asarray(safe, dtype=np.float32)
                        self.captured_frames += int(safe.shape[0])
                        peak = int(np.max(np.abs(safe)) * 32767.0)
                        rms = int(round(float(np.sqrt(np.mean(safe * safe))) * 32767.0))
                        self.last_peak = max(self.last_peak, peak)
                        if rms > 0:
                            self.last_rms = rms

                        pcm = self._float_to_int16(safe, self.profile.channels)
                        output = resampler.process(pcm)
                        if output.size == 0:
                            continue
                        pending.extend(output[:, :1].astype("<i2", copy=False).tobytes())

                        block_bytes = block_samples * self.profile.channels * 2
                        while len(pending) >= block_bytes and not self.capture_stop_event.is_set():
                            pcm_bytes = bytes(pending[:block_bytes])
                            del pending[:block_bytes]
                            payload = bytearray()
                            payload.append(0x50)
                            payload.extend(int(block_samples).to_bytes(2, "little"))
                            payload.extend(pcm_bytes)
                            try:
                                self.block_queue.put(bytes(payload), timeout=0.25)
                            except queue.Full:
                                self.dropped_chunks += 1
        except Exception as exc:
            if not self.capture_stop_event.is_set():
                self.error = exc
        finally:
            self._recorder = None
            self.capture_done_event.set()
            if com_initialized:
                try:
                    ctypes.windll.ole32.CoUninitialize()
                except Exception:
                    pass

    def status_text(self) -> str:
        route = ""
        if self._loopback is not None:
            route = f", route={self._selected_route_name or getattr(self._loopback, 'name', '')}"
        queued = self.block_queue.qsize()
        if self.error is not None:
            return f"ERROR: {self.error}"
        return (
            f"running={self.is_running()}, input={self.input_rate} Hz/{self.input_channels} ch, "
            f"transport={self.profile.sample_rate} Hz/{self.profile.channels} ch/{self.profile.bits}-bit, "
            f"sent={self.sent_bytes} raw PCM bytes ({self.sent_pcm_bytes} PCM bytes), "
            f"blocks={self.sent_blocks}, dropped_capture_chunks={self.dropped_chunks}, "
            f"capture_queue={queued}/{AUDIO_QUEUE_MAX_BLOCKS}, captured_frames={self.captured_frames}, "
            f"peak={self.last_peak}, rms={self.last_rms}{route}"
        )
