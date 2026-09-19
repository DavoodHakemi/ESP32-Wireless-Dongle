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


def queue_model(
    producer_blocks_per_s: float,
    consumer_blocks_per_s: float,
    duration_s: float,
    capacity: int = 55,
    prebuffer: int = 8,
) -> tuple[int, int, int, int]:
    dt = 0.0001
    queue = 0
    received = dropped = consumed = underruns = 0
    primed = False
    prod_acc = cons_acc = 0.0

    for _ in range(int(duration_s / dt)):
        prod_acc += producer_blocks_per_s * dt
        while prod_acc >= 1.0:
            prod_acc -= 1.0
            if queue < capacity:
                queue += 1
                received += 1
            else:
                dropped += 1

        cons_acc += consumer_blocks_per_s * dt
        while cons_acc >= 1.0:
            cons_acc -= 1.0
            if not primed:
                if queue >= prebuffer:
                    primed = True
                else:
                    continue
            if queue:
                queue -= 1
                consumed += 1
            else:
                underruns += 1

    return queue, received, dropped, underruns


def test_phase_resampler_matches_source_rate_at_all_supported_a2dp_rates() -> None:
    source = 22050
    for output in (16000, 32000, 44100, 48000):
        assert consumed_source_samples(output, source, output) == source


def test_legacy_fixed_two_x_repeat_drifts_at_32khz() -> None:
    source = 22050
    output = 32000
    legacy_consumed = output // 2
    assert legacy_consumed == 16000
    assert legacy_consumed != source


def test_legacy_32khz_model_saturates_like_the_hardware_log() -> None:
    producer = 22050 / 256
    legacy_32k = 32000 / 2 / 256
    queue, received, dropped, underruns = queue_model(
        producer,
        legacy_32k,
        duration_s=5.72,
    )

    assert received + dropped > 475
    assert 50 <= queue <= 55
    assert dropped > 70
    assert underruns == 0


def test_measured_rate_model_stays_stable_at_32khz() -> None:
    producer = 22050 / 256
    corrected = 22050 / 256
    queue, received, dropped, underruns = queue_model(
        producer,
        corrected,
        duration_s=5.72,
    )

    assert received > 475
    assert queue < 12
    assert dropped == 0
    assert underruns == 0
