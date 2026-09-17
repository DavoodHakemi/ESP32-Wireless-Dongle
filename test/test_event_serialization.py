from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_events_are_queued_before_wire_output():
    source = (ROOT / "src/application/DongleApplication.cpp").read_text()
    assert "_eventQueue.enqueue(event);" in source
    assert "_codec.sendEvent" in source
    update_start = source.index("void DongleApplication::update()")
    on_command_start = source.index("void DongleApplication::onCommand")
    update_body = source[update_start:on_command_start]
    publish_start = source.index("void DongleApplication::publish")
    publish_body = source[publish_start:]
    assert "_codec.sendEvent" in update_body
    assert "_eventQueue.enqueue(event);" in publish_body


def test_event_queue_is_fixed_size_and_copies_payloads():
    source = (ROOT / "include/core/EventQueue.h").read_text()
    assert "static constexpr uint8_t CAPACITY = 32;" in source
    assert "static constexpr uint16_t MAX_PAYLOAD = 336;" in source
    assert "std::memcpy(record.payload, event.payload, event.length);" in source
    assert "portENTER_CRITICAL" in source


def test_firmware_version_is_consistent():
    version = (ROOT / "VERSION.txt").read_text().strip()
    header = (ROOT / "include/config/Version.h").read_text()
    assert version == "2.3.9"
    assert '#define DONGLE_FIRMWARE_VERSION "2.3.9"' in header


def test_boot_events_are_held_until_protocol_handshake():
    source = (ROOT / "src/application/DongleApplication.cpp").read_text()
    assert "if (!_protocolReady)" in source
    assert "command.id == protocol::CommandId::GetInfo" in source
    assert "_protocolReady = true;" in source
