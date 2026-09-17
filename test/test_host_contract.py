from pathlib import Path

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


def test_a2dp_connect_timeout_leaves_host_wait_margin():
    config = (ROOT / "include/config/AppConfig.h").read_text(encoding="utf-8")
    assert "A2DP_CONNECT_TIMEOUT_MS = 50000" in config
