from pathlib import Path
import re

ROOT = Path(__file__).parents[1]


def test_host_console_contract_is_installed_at_transport_boundary():
    console = (ROOT / "host/console.py").read_text(encoding="utf-8")
    transport = (ROOT / "host/serial_transport.py").read_text(encoding="utf-8")

    assert "def install_console_contract()" in console
    assert "builtins.print = _console_print" in console
    assert "builtins.input = _console_input" in console
    assert "startswith(\"[PC,\")" in console
    assert "startswith(\"[ESP,\")" in console
    assert "install_console_contract()" in transport


def test_a2dp_host_wait_exceeds_firmware_timeout():
    config = (ROOT / "include/config/AppConfig.h").read_text(encoding="utf-8")
    main = (ROOT / "host/main.py").read_text(encoding="utf-8")

    timeout_match = re.search(r"A2DP_CONNECT_TIMEOUT_MS\s*=\s*(\d+)", config)
    wait_match = re.search(r"_wait_a2dp_result\(device: Esp32Device, wait_seconds: float = ([0-9.]+)\)", main)
    assert timeout_match and wait_match
    assert float(wait_match.group(1)) * 1000.0 > int(timeout_match.group(1))


def test_a2dp_auto_uses_library_name_fallback_without_extra_retry_budget():
    manager = (ROOT / "src/services/a2dp/A2DPManager.cpp").read_text(encoding="utf-8")
    assert "_pendingRetries = 0;" in manager
    assert "Classic name inquiry" in manager


def test_host_reads_firmware_version_from_project_source_of_truth():
    main = (ROOT / "host/main.py").read_text(encoding="utf-8")
    assert 'VERSION_FILE = Path(__file__).resolve().parents[1] / "VERSION.txt"' in main
    assert 'EXPECTED_FIRMWARE_VERSION = VERSION_FILE.read_text' in main


def test_audio_stream_uses_non_flushing_serial_path() -> None:
    serial = (ROOT / "host/serial_transport.py").read_text(encoding="utf-8")
    esp32 = (ROOT / "host/esp32.py").read_text(encoding="utf-8")

    start = serial.index("    def write_stream(")
    end = serial.index("    def read(", start)
    stream_method = serial[start:end]

    assert "self.serial.write(data)" in stream_method
    assert "self.serial.flush()" not in stream_method
    assert 'self.transport.write_stream(b"".join(frames))' in esp32


def test_low_latency_capture_quantum_matches_audio_read_quantum() -> None:
    source = (ROOT / "host/audio.py").read_text(encoding="utf-8")

    assert "CAPTURE_BLOCK_FRAMES = 256" in source
    assert "CAPTURE_RECORD_FRAMES = 256" in source
    assert "CAPTURE_RECORDER_BLOCKSIZE = 256" in source


def test_audio_transport_uses_short_two_block_batches() -> None:
    esp32 = (ROOT / "host/esp32.py").read_text(encoding="utf-8")
    audio = (ROOT / "host/audio.py").read_text(encoding="utf-8")

    assert "AUDIO_SEND_BATCH_BLOCKS = 2" in audio
    assert 'self.send_batch(b"".join(batch))' in audio
    assert 'self.transport.write_stream(b"".join(frames))' in esp32


def test_firmware_serial_rx_budget_is_large_enough_for_short_audio_batches() -> None:
    hardware = (ROOT / "include/config/HardwareConfig.h").read_text(encoding="utf-8")
    app = (ROOT / "src/application/DongleApplication.cpp").read_text(encoding="utf-8")

    assert "SERIAL_RX_BUFFER = 8192" in hardware
    assert "static uint8_t buffer[2048]" in app
