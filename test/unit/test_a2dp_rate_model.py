from __future__ import annotations

def consumed_source_samples(output_frames: int, source_rate: int, output_rate: int) -> int:
    phase = 0
    consumed = 0
    for _ in range(output_frames):
        phase += source_rate
        while phase >= output_rate:
            phase -= output_rate
            consumed += 1
    return consumed

def test_phase_resampler_matches_source_rate_at_all_supported_a2dp_rates() -> None:
    source = 22050
    for output in (16000, 32000, 44100, 48000):
        consumed = consumed_source_samples(output, source, output)
        assert consumed == source

def test_legacy_fixed_two_x_repeat_drifts_at_32khz() -> None:
    source = 22050
    output = 32000
    legacy_consumed = output // 2
    assert legacy_consumed == 16000
    assert legacy_consumed != source

def test_32khz_queue_model_matches_the_observed_saturation_pattern() -> None:
    source_blocks = 22050 / 256
    legacy_32k_blocks = 32000 / 2 / 256
    duration = 5.72
    net_blocks = (source_blocks - legacy_32k_blocks) * duration
    assert 120 < net_blocks < 140
    assert 50 <= min(55, int(net_blocks))
