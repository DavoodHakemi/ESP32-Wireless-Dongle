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
    wait_match = re.search(r"_wait_a2dp_result\(device, wait_seconds: float = ([0-9.]+)\)", main)
    assert timeout_match and wait_match
    assert float(wait_match.group(1)) * 1000.0 > int(timeout_match.group(1))


def test_a2dp_auto_uses_fast_library_fallback_budget():
    manager = (ROOT / "src/services/a2dp/A2DPManager.cpp").read_text(encoding="utf-8")
    assert "_pendingRetries = 1;" in manager
    assert "Three retries would push the library's discovery fallback beyond" in manager
