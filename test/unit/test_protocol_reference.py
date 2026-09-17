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
