from pathlib import Path

ROOT = Path(__file__).parents[1]


def text(rel):
    return (ROOT / rel).read_text(encoding="utf-8")


def test_main_is_thin():
    lines = [x for x in text("src/main.cpp").splitlines() if x.strip()]
    assert len(lines) <= 8


def test_required_modules_exist():
    required = [
        "include/core/Event.h", "include/core/Error.h", "include/core/Result.h",
        "include/protocol/Protocol.h", "include/protocol/ProtocolFrame.h", "include/protocol/CommandParser.h",
        "include/protocol/CommandDispatcher.h", "include/protocol/ProtocolCodec.h",
        "include/transport/ITransport.h", "include/transport/SerialTransport.h",
        "include/services/wifi/WiFiManager.h", "include/services/network/NetworkService.h",
        "include/services/bluetooth/BluetoothClassic.h", "include/services/bluetooth/BluetoothLE.h",
        "include/services/a2dp/A2DPManager.h", "include/services/a2dp/A2DPSource.h",
        "include/services/a2dp/A2DPConnection.h", "include/services/system/SystemService.h",
        "include/application/DongleApplication.h", "include/utils/Logger.h",
        "docs/architecture.md", "docs/refactoring-report.md"
    ]
    assert all((ROOT / p).exists() for p in required)


def test_no_service_direct_serial_io():
    for p in (ROOT / "src/services").rglob("*.cpp"):
        source = p.read_text(encoding="utf-8")
        assert "Serial.print" not in source
        assert "Serial.write" not in source


def test_services_do_not_include_protocol_headers():
    for p in (ROOT / "src/services").rglob("*.cpp"):
        assert '#include "protocol/' not in p.read_text(encoding="utf-8")


def test_a2dp_callback_has_no_protocol_or_logger_calls():
    source = text("src/services/a2dp/A2DPManager.cpp")
    start = source.index("int32_t A2DPManager::frameCallback")
    end = source.index("bool A2DPManager::fillFrames")
    callback = source[start:end]
    assert "_logger." not in callback
    assert "_events." not in callback


def test_dependency_pin():
    pio = text("platformio.ini")
    assert "platform-espressif32/releases/download/51.03.07" in pio
    assert "ESP32-A2DP.git#v1.8.10" in pio


def test_version_source_of_truth():
    header = text("include/config/Version.h")
    version_txt = text("VERSION.txt").strip()
    import re
    match = re.search(r'#define DONGLE_FIRMWARE_VERSION "([0-9.]+)"', header)
    assert match and match.group(1) == version_txt


def test_no_placeholder_markers():
    for root in (ROOT / "src", ROOT / "include"):
        for p in root.rglob("*"):
            if p.suffix not in {".cpp", ".h"}:
                continue
            source = p.read_text(encoding="utf-8")
            assert "TODO" not in source
            assert "implementation here" not in source.lower()

def test_legacy_audio_status_payload_is_preserved():
    source = (ROOT / "src/protocol/CommandDispatcher.cpp").read_text(encoding="utf-8")
    assert 'case CommandId::AudioStatus' in source
    assert 'uint8_t p[36]{}' in source


def test_auto_reconnect_keeps_fallback_state():
    source = (ROOT / "src/services/a2dp/A2DPManager.cpp").read_text(encoding="utf-8")
    assert '_autoMode = true;' in source and '_fallbackName = name;' in source and '_fallbackAttempted = false;' in source

def test_compile_regressions_fixed():
    parser = text("src/protocol/CommandParser.cpp")
    a2dp_source = text("include/services/a2dp/A2DPSource.h")
    classic = text("src/services/bluetooth/BluetoothClassic.cpp")
    manager_h = text("include/services/a2dp/A2DPManager.h")
    manager_cpp = text("src/services/a2dp/A2DPManager.cpp")
    assert '#include "protocol/ProtocolFrame.h"' in parser or '#include "protocol/ProtocolFrame.h"' in text("include/protocol/CommandParser.h")
    assert 'bool discoveryActive() {' in a2dp_source
    assert 'Result<void> BluetoothClassic::begin()' in classic
    assert 'String cachedMac(); String cachedName();' in manager_h
    assert manager_cpp.count('\nvoid A2DPManager::update()') == 1


def test_a2dp_source_start_uses_library_owned_task():
    header = text("include/services/a2dp/A2DPSource.h")
    source = text("src/services/a2dp/A2DPSource.cpp")
    assert "freertos/task.h" not in header
    assert "xTaskCreate(" not in source
    assert "startTaskEntry" not in header
    assert "runStartTask" not in header
    assert source.count("_source.start();") == 2


def test_a2dp_timeout_restores_classic_before_failure_event():
    source = text("src/services/a2dp/A2DPManager.cpp")
    marker = '"A2DP connection timeout after'
    start = source.index(marker)
    timeout = source[start:]
    assert '_source.stop();' in timeout
    assert '_bluetooth.classic.restoreAfterA2dp();' in timeout
    assert '_events.publish({EventType::A2dpConnectFailed' in timeout
    assert timeout.index('_source.stop();') < timeout.index('_events.publish({EventType::A2dpConnectFailed')
