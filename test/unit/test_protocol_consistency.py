from pathlib import Path
import json
import re

ROOT = Path(__file__).parents[2]


def parse_enum(source: str, enum_name: str) -> dict[str, int]:
    block = re.search(
        rf"enum class {enum_name}.*?\}};",
        source,
        re.S,
    )
    assert block is not None
    return {
        name: int(value, 16)
        for name, value in re.findall(
            r"\b(\w+)\s*=\s*(0x[0-9A-Fa-f]+)",
            block.group(0),
        )
    }


def test_protocol_schema_matches_firmware_and_host() -> None:
    schema = json.loads(
        (ROOT / "docs/protocol-schema.json").read_text(encoding="utf-8")
    )
    firmware = (ROOT / "include/protocol/Protocol.h").read_text(encoding="utf-8")
    host = (ROOT / "host/dongle/protocol_ids.py").read_text(encoding="utf-8")

    fw_commands = parse_enum(firmware, "CommandId")
    fw_events = parse_enum(firmware, "EventId")

    host_commands = {
        name: int(value, 16)
        for name, value in re.findall(
            r'"([A-Z0-9_]+)":\s*(0x[0-9A-Fa-f]+)',
            host.split("EVENTS =", 1)[0],
        )
    }
    host_events = {
        name: int(value, 16)
        for name, value in re.findall(
            r'"([A-Z0-9_]+)":\s*(0x[0-9A-Fa-f]+)',
            host.split("EVENTS =", 1)[1],
        )
    }

    assert schema["wire_version"] == 1
    assert schema["frame_types"] == {"request": 1, "response": 2, "event": 3, "error": 4}
    assert schema["commands"] == {name: value for name, value in fw_commands.items()}
    assert schema["events"] == {name: value for name, value in fw_events.items()}

    reverse_host_commands = {
        {
            "GetInfo": "GET_INFO",
            "GetVersion": "GET_VERSION",
            "Ping": "PING",
            "A2dpTestTone": "A2DP_TEST_TONE",
            "A2dpStatus": "A2DP_STATUS",
            "AudioData": "AUDIO_DATA",
        }.get(name, name.upper()): value
        for name, value in fw_commands.items()
    }
    assert host_commands.get("GET_INFO") == fw_commands["GetInfo"]
    assert host_commands.get("GET_VERSION") == fw_commands["GetVersion"]
    assert host_commands.get("PING") == fw_commands["Ping"]
    assert host_commands.get("A2DP_TEST_TONE") == fw_commands["A2dpTestTone"]
    assert host_commands.get("A2DP_STATUS") == fw_commands["A2dpStatus"]
    assert host_commands.get("AUDIO_DATA") == fw_commands["AudioData"]
    assert host_events.get("SERIAL_LOG") == fw_events["SerialLog"]


def test_async_a2dp_commands_do_not_start_from_command_dispatcher_inline() -> None:
    source = (ROOT / "src/services/a2dp/A2DPManager.cpp").read_text(encoding="utf-8")
    assert "processPendingConnection();" in source
    assert "_pendingConnection = PendingConnection::Auto;" in source

def test_preferences_are_initialized_during_begin() -> None:
    header = (ROOT / "include/services/a2dp/A2DPManager.h").read_text(encoding="utf-8")
    source = (ROOT / "src/services/a2dp/A2DPManager.cpp").read_text(encoding="utf-8")
    assert '_preferences.begin("a2dp", false)' in source
    assert "A2DPManager::A2DPManager(" in source
    constructor_block = source.split("A2DPManager::A2DPManager(", 1)[1].split("}", 1)[0]
    assert "_preferences.begin" not in constructor_block


def test_a2dp_connect_commands_are_accepted_async() -> None:
    dispatcher = (ROOT / "src/protocol/CommandDispatcher.cpp").read_text(encoding="utf-8")
    manager = (ROOT / "src/services/a2dp/A2DPManager.cpp").read_text(encoding="utf-8")
    assert "case CommandId::A2dpConnect:" in dispatcher
    assert "case CommandId::A2dpConnectName:" in dispatcher
    assert "case CommandId::A2dpConnectAuto:" in dispatcher
    assert "PendingConnection::Auto" in manager
    assert "processPendingConnection();" in manager


def test_all_wire_commands_are_explicitly_dispatched() -> None:
    firmware = (ROOT / "include/protocol/Protocol.h").read_text(encoding="utf-8")
    dispatcher = (ROOT / "src/protocol/CommandDispatcher.cpp").read_text(encoding="utf-8")
    block = re.search(r"enum class CommandId.*?\};", firmware, re.S)
    assert block is not None
    names = re.findall(r"\b(\w+)\s*=\s*0x[0-9A-Fa-f]+", block.group(0))
    for name in names:
        assert f"CommandId::{name}" in dispatcher


def test_version_components_match_version_string() -> None:
    source = (ROOT / "include/config/Version.h").read_text(encoding="utf-8")
    match = re.search(
        r"MAJOR (\d+).*?MINOR (\d+).*?PATCH (\d+).*?VERSION \"(\d+)\.(\d+)\.(\d+)\"",
        source,
        re.S,
    )
    assert match is not None
    assert match.group(1, 2, 3) == match.group(4, 5, 6)
