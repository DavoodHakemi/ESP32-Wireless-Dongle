from pathlib import Path

ROOT = Path(__file__).parents[2]


def test_wire_ids_match_legacy_values():
    source = (ROOT / "include/protocol/Protocol.h").read_text(encoding="utf-8")
    expected = {
        "Ping": "0x03", "GetVersion": "0x02", "GetInfo": "0x01",
        "A2dpConnect": "0x61", "A2dpTestTone": "0x62", "A2dpStatus": "0x65",
        "AudioStart": "0x6A", "AudioData": "0x6D",
    }
    for name, value in expected.items():
        assert f"{name} = {value}" in source


def test_wire_version_is_one():
    source = (ROOT / "include/protocol/ProtocolVersion.h").read_text(encoding="utf-8")
    assert "WIRE_VERSION = 0x01" in source


def test_response_and_event_frame_contracts():
    source = (ROOT / "include/protocol/ProtocolFrame.h").read_text(encoding="utf-8")
    assert "Request = 0x01" in source
    assert "Response = 0x02" in source
    assert "Event = 0x03" in source
    assert "Error = 0x04" in source
    assert "MAX_FRAME_PAYLOAD = 4096" in source


def test_protocol_codec_avoids_large_response_stack_buffer():
    source = (ROOT / "src/protocol/ProtocolCodec.cpp").read_text(encoding="utf-8")
    assert "uint8_t buffer[4096]" not in source
    assert "sendFrameParts" in source
    assert "const uint8_t statusByte" in source
