import re
import time
import threading
from collections import deque
from typing import Optional

from frame import (
    TYPE_ERROR,
    Frame,
    FrameParser,
    TYPE_EVENT,
    TYPE_REQUEST,
    TYPE_RESPONSE,
    build_frame,
)
from serial_transport import SerialTransport


# ============================================================
# Commands
# ============================================================

CMD_GET_INFO = 0x01
CMD_GET_VERSION = 0x02
CMD_PING = 0x03
CMD_RESET = 0x04
CMD_SET_BAUD = 0x05  # Legacy command; fixed transport is 921,600 baud

CMD_WIFI_SCAN = 0x10
CMD_WIFI_CONNECT = 0x11
CMD_WIFI_DISCONNECT = 0x12
CMD_WIFI_STATUS = 0x13

CMD_TCP_CONNECT = 0x20
CMD_TCP_SEND = 0x21
CMD_TCP_RECEIVE = 0x22
CMD_TCP_CLOSE = 0x23
CMD_TCP_SERVER_START = 0x24
CMD_TCP_SERVER_ACCEPT = 0x25
CMD_TCP_SERVER_SEND = 0x26
CMD_TCP_SERVER_RECEIVE = 0x27
CMD_TCP_SERVER_CLOSE = 0x28
CMD_UDP_OPEN = 0x29
CMD_UDP_SEND = 0x2A
CMD_UDP_RECEIVE = 0x2B
CMD_UDP_CLOSE = 0x2C

# Bluetooth Classic commands (protocol v1.3; existing commands retained)
CMD_BT_INIT = 0x40
CMD_BT_GET_INFO = 0x41
CMD_BT_SCAN_START = 0x42
CMD_BT_SCAN_STOP = 0x43
CMD_BT_STATUS = 0x44
CMD_BT_CONNECT = 0x45
CMD_BT_DISCONNECT = 0x46
CMD_BT_BLE_SCAN_START = 0x47
CMD_BT_BLE_SCAN_STOP = 0x48

# A2DP Source commands (protocol v1.5; retained in v1.6)
CMD_A2DP_GET_INFO = 0x60
CMD_A2DP_CONNECT = 0x61
CMD_A2DP_TEST_TONE = 0x62
CMD_A2DP_STOP_TONE = 0x63
CMD_A2DP_DISCONNECT = 0x64
CMD_A2DP_STATUS = 0x65
CMD_A2DP_CONNECT_NAME = 0x66
CMD_A2DP_CONNECT_AUTO = 0x67
CMD_A2DP_CLEAR_CACHE = 0x68
CMD_A2DP_GET_CACHE = 0x69
CMD_AUDIO_START = 0x6A
CMD_AUDIO_STOP = 0x6B
CMD_AUDIO_STATUS = 0x6C
CMD_AUDIO_DATA = 0x6D

# Events
EVT_WIFI_NETWORK_FOUND = 0x14
EVT_WIFI_SCAN_DONE = 0x15
EVT_WIFI_CONNECTED = 0x16
EVT_WIFI_DISCONNECTED = 0x17
EVT_WIFI_GOT_IP = 0x18
EVT_WIFI_CONNECT_FAILED = 0x19
EVT_TCP_CONNECTED = 0x30
EVT_TCP_DISCONNECTED = 0x31
EVT_TCP_SERVER_CONNECTED = 0x32
EVT_BT_DEVICE_FOUND = 0x50
EVT_BT_SCAN_DONE = 0x51
EVT_BT_CONNECTING = 0x52
EVT_BT_CONNECTED = 0x53
EVT_BT_CONNECT_FAIL = 0x54
EVT_BT_DISCONNECTED = 0x55
EVT_BT_DEVICE_DETAIL = 0x56
EVT_BT_BLE_DEVICE_FOUND = 0x57
EVT_BT_BLE_SCAN_DONE = 0x58

# A2DP Source events
EVT_A2DP_CONNECTING = 0x70
EVT_A2DP_CONNECTED = 0x71
EVT_A2DP_AUDIO_STARTED = 0x72
EVT_A2DP_AUDIO_STOPPED = 0x73
EVT_A2DP_DISCONNECTED = 0x74
EVT_A2DP_CONNECT_FAIL = 0x75
EVT_A2DP_CLASSIC_FOUND = 0x76
EVT_AUDIO_UNDERRUN = 0x77
EVT_SERIAL_LOG = 0x78


# ============================================================
# Status
# ============================================================

STATUS_SUCCESS = 0x00
STATUS_INVALID_COMMAND = 0x01
STATUS_INVALID_PARAMETER = 0x02
STATUS_BUSY = 0x03
STATUS_TIMEOUT = 0x04
STATUS_NOT_CONNECTED = 0x05
STATUS_INTERNAL_ERROR = 0x06
STATUS_CRC_ERROR = 0x07
STATUS_NOT_SUPPORTED = 0x08


AUTH_NAMES = {
    0: "OPEN",
    1: "WEP",
    2: "WPA-PSK",
    3: "WPA2-PSK",
    4: "WPA/WPA2-PSK",
    5: "WPA2-ENTERPRISE",
    6: "WPA3-PSK",
    7: "WPA2/WPA3-PSK",
    8: "WPA3-ENTERPRISE-192",
    9: "OWE",
    10: "WPA3-EXT-PSK",
    11: "WPA3-EXT-PSK-SAE",
}


class Esp32Device:
    def __init__(self, port: str = "COM5", baudrate: int = 921600) -> None:
        self.transport = SerialTransport(port=port, baudrate=baudrate)
        self.parser = FrameParser()
        self.sequence = 0
        self.events: deque[Frame] = deque()
        self.responses: deque[Frame] = deque()
        self._rx_stop = threading.Event()
        self._rx_condition = threading.Condition()
        self._rx_thread: threading.Thread | None = None
        self._request_lock = threading.Lock()
        self._rx_error: Exception | None = None

    def connect(self) -> None:
        self.transport.connect()
        self.parser.reset()
        with self._rx_condition:
            self.events.clear()
            self.responses.clear()
        self._rx_error = None
        self._rx_stop.clear()
        # Drain ESP32 boot chatter before starting the framed protocol reader.
        while True:
            data = self.transport.read(1024)
            if not data:
                break
        self._rx_thread = threading.Thread(
            target=self._rx_loop,
            name="esp32-uart-reader",
            daemon=True,
        )
        self._rx_thread.start()

    def disconnect(self) -> None:
        self._rx_stop.set()
        if self._rx_thread is not None and self._rx_thread.is_alive():
            self._rx_thread.join(timeout=1.0)
        self._rx_thread = None
        self.transport.disconnect()

    def _rx_loop(self) -> None:
        try:
            while not self._rx_stop.is_set():
                data = self.transport.read(4096)
                if not data:
                    continue
                frames = self.parser.feed(data)
                for frame in frames:
                    if frame.frame_type == TYPE_EVENT:
                        if frame.cmd == EVT_SERIAL_LOG:
                            self._print_esp32_log(frame)
                        else:
                            with self._rx_condition:
                                self.events.append(frame)
                                self._rx_condition.notify_all()
                    elif frame.frame_type == TYPE_ERROR:
                        with self._rx_condition:
                            self.responses.append(frame)
                            self._rx_condition.notify_all()
                    elif frame.frame_type == TYPE_RESPONSE:
                        with self._rx_condition:
                            self.responses.append(frame)
                            self._rx_condition.notify_all()
        except Exception as exc:
            self._rx_error = exc
            with self._rx_condition:
                self._rx_condition.notify_all()

    def _print_esp32_log(self, frame: Frame) -> None:
        event = self.decode_event(frame)
        if event is None:
            return
        stamp = event.get("timestamp_ms")
        message = event.get("message", "")
        log_type = str(event.get("type", "Log"))
        if stamp is None:
            print(f"[ESP, {log_type}, 0 ms] {message}", flush=True)
        else:
            print(f"[ESP, {log_type}, {int(stamp)} ms] {message}", flush=True)

    def set_baudrate(self, baudrate: int) -> None:
        """Compatibility helper; v2.2 keeps the UART fixed at 921,600 baud."""
        if int(baudrate) != 921600:
            raise ValueError("ESP32 Wireless Dongle transport is fixed at 921,600 baud")
        if not self.transport.is_connected():
            raise RuntimeError("Serial port is not connected")
        self.transport.set_baudrate(921600)

    def _next_sequence(self) -> int:
        value = self.sequence
        self.sequence = (self.sequence + 1) & 0xFF
        return value

    def poll_events(self) -> list[Frame]:
        with self._rx_condition:
            items = list(self.events)
            self.events.clear()
            return items

    def request(
        self,
        cmd: int,
        payload: bytes = b"",
        timeout: float = 2.0,
        retries: int = 0,
    ) -> Frame:
        with self._request_lock:
            seq = self._next_sequence()
            frame = build_frame(TYPE_REQUEST, cmd, seq, payload)

            for attempt in range(retries + 1):
                self.transport.write(frame)
                deadline = time.monotonic() + timeout

                while time.monotonic() < deadline:
                    remaining = deadline - time.monotonic()
                    with self._rx_condition:
                        for idx, response in enumerate(self.responses):
                            if response.seq == seq and response.cmd == cmd:
                                del self.responses[idx]
                                if response.frame_type == TYPE_ERROR:
                                    raise RuntimeError(
                                        f"Protocol error for command 0x{cmd:02X}: "
                                        f"{response.payload.hex()}"
                                    )
                                return response

                        if self._rx_error is not None:
                            raise RuntimeError(
                                f"UART reader stopped: {self._rx_error}"
                            )

                        self._rx_condition.wait(
                            timeout=max(0.01, min(0.05, remaining))
                        )

                if attempt < retries:
                    continue

            stats = self.parser.stats
            with self._rx_condition:
                unmatched = [
                    f"type=0x{frame.frame_type:02X},cmd=0x{frame.cmd:02X},seq={frame.seq}"
                    for frame in list(self.responses)[-8:]
                ]
            detail = "; ".join(unmatched) if unmatched else "none"
            raise TimeoutError(
                f"Timeout waiting for command 0x{cmd:02X}; "
                f"rx_frames={stats.frames_ok}, crc_errors={stats.crc_errors}, "
                f"version_errors={stats.version_errors}, length_errors={stats.length_errors}; "
                f"unmatched_responses={detail}"
            )

    @staticmethod
    def _check_status(frame: Frame) -> bytes:
        if len(frame.payload) < 1:
            raise RuntimeError("Invalid response payload")
        status = frame.payload[0]
        if status != STATUS_SUCCESS:
            names = {
                STATUS_INVALID_COMMAND: "INVALID_COMMAND",
                STATUS_INVALID_PARAMETER: "INVALID_PARAMETER",
                STATUS_BUSY: "BUSY",
                STATUS_TIMEOUT: "TIMEOUT",
                STATUS_NOT_CONNECTED: "NOT_CONNECTED",
                STATUS_INTERNAL_ERROR: "INTERNAL_ERROR",
                STATUS_CRC_ERROR: "CRC_ERROR",
                STATUS_NOT_SUPPORTED: "NOT_SUPPORTED",
            }
            raise RuntimeError(names.get(status, f"UNKNOWN_STATUS_{status}"))
        return frame.payload[1:]

    # --------------------------------------------------------
    # System
    # --------------------------------------------------------

    def ping(self) -> str:
        return self._check_status(self.request(CMD_PING, timeout=3.0, retries=1)).decode("ascii", errors="replace")

    def get_version(self) -> str:
        return self._check_status(self.request(CMD_GET_VERSION, timeout=3.0, retries=1)).decode("ascii", errors="replace")

    def get_info(self) -> dict:
        payload = self._check_status(self.request(CMD_GET_INFO, timeout=3.0, retries=2))
        if len(payload) < 7:
            raise RuntimeError("Invalid GET_INFO response")
        name_length = payload[0]
        if len(payload) < 1 + name_length + 6:
            raise RuntimeError("Incomplete GET_INFO response")
        device_name = payload[1:1 + name_length].decode("ascii", errors="replace")
        mac_bytes = payload[1 + name_length:1 + name_length + 6]
        mac = ":".join(f"{byte:02X}" for byte in reversed(mac_bytes))
        return {"device_name": device_name, "mac": mac}

    # --------------------------------------------------------
    # Wi-Fi
    # --------------------------------------------------------

    @staticmethod
    def _pack_string(value: str, max_length: int) -> bytes:
        encoded = value.encode("utf-8")
        if len(encoded) > max_length:
            raise ValueError(f"String is longer than {max_length} bytes")
        return bytes([len(encoded)]) + encoded

    def wifi_scan(self) -> None:
        self.request(CMD_WIFI_SCAN)

    def wifi_connect(self, ssid: str, password: str) -> None:
        payload = self._pack_string(ssid, 32) + self._pack_string(password, 64)
        self.request(CMD_WIFI_CONNECT, payload=payload)

    def wifi_disconnect(self) -> None:
        self.request(CMD_WIFI_DISCONNECT)

    def wifi_status(self) -> dict:
        payload = self._check_status(self.request(CMD_WIFI_STATUS))
        if len(payload) < 8:
            raise RuntimeError("Invalid WIFI_STATUS response")
        state = payload[0]
        rssi = int.from_bytes(payload[1:3], byteorder="little", signed=True)
        ip = ".".join(str(byte) for byte in payload[3:7])
        ssid_length = payload[7]
        if len(payload) < 8 + ssid_length:
            raise RuntimeError("Incomplete WIFI_STATUS response")
        ssid = payload[8:8 + ssid_length].decode("utf-8", errors="replace")
        state_names = {0: "DISCONNECTED", 1: "CONNECTING", 2: "CONNECTED", 3: "FAILED"}
        return {
            "state": state_names.get(state, f"UNKNOWN({state})"),
            "rssi": rssi,
            "ip": ip,
            "ssid": ssid,
        }

    # --------------------------------------------------------
    # TCP client
    # --------------------------------------------------------

    @staticmethod
    def _pack_ip(ip: str) -> bytes:
        parts = ip.split(".")
        if len(parts) != 4:
            raise ValueError(f"Invalid IPv4 address: {ip}")
        values = []
        for part in parts:
            value = int(part)
            if not 0 <= value <= 255:
                raise ValueError(f"Invalid IPv4 address: {ip}")
            values.append(value)
        return bytes(values)

    def tcp_connect(self, host: str, port: int, timeout: float = 8.0) -> dict:
        if not 1 <= port <= 65535:
            raise ValueError("TCP port must be 1..65535")
        payload = self._pack_ip(host) + port.to_bytes(2, "little")
        info = self._check_status(self.request(CMD_TCP_CONNECT, payload, timeout=timeout))
        if len(info) != 6:
            raise RuntimeError("Invalid TCP_CONNECT response")
        return {"host": ".".join(map(str, info[:4])), "port": int.from_bytes(info[4:6], "little")}

    def tcp_send(self, data: bytes) -> int:
        if len(data) > 2048:
            raise ValueError("TCP payload is limited to 2048 bytes")
        payload = self._check_status(self.request(CMD_TCP_SEND, data))
        if len(payload) != 2:
            raise RuntimeError("Invalid TCP_SEND response")
        return int.from_bytes(payload, "little")

    def tcp_receive(self, timeout_ms: int = 3000, max_len: int = 1024) -> bytes:
        if not 1 <= timeout_ms <= 65535 or not 1 <= max_len <= 2048:
            raise ValueError("Invalid TCP receive parameters")
        payload = timeout_ms.to_bytes(2, "little") + max_len.to_bytes(2, "little")
        return self._check_status(self.request(CMD_TCP_RECEIVE, payload, timeout=timeout_ms / 1000 + 1.0))

    def tcp_close(self) -> None:
        self._check_status(self.request(CMD_TCP_CLOSE))

    # --------------------------------------------------------
    # TCP server
    # --------------------------------------------------------

    def tcp_server_start(self, port: int) -> None:
        if not 1 <= port <= 65535:
            raise ValueError("TCP server port must be 1..65535")
        self._check_status(self.request(CMD_TCP_SERVER_START, port.to_bytes(2, "little")))

    def tcp_server_accept(self, timeout_ms: int = 5000) -> dict:
        payload = timeout_ms.to_bytes(2, "little")
        info = self._check_status(self.request(CMD_TCP_SERVER_ACCEPT, payload, timeout=timeout_ms / 1000 + 1.0))
        if len(info) != 6:
            raise RuntimeError("Invalid TCP_SERVER_ACCEPT response")
        return {"host": ".".join(map(str, info[:4])), "port": int.from_bytes(info[4:6], "little")}

    def tcp_server_send(self, data: bytes) -> int:
        if len(data) > 2048:
            raise ValueError("TCP payload is limited to 2048 bytes")
        result = self._check_status(self.request(CMD_TCP_SERVER_SEND, data))
        if len(result) != 2:
            raise RuntimeError("Invalid TCP_SERVER_SEND response")
        return int.from_bytes(result, "little")

    def tcp_server_receive(self, timeout_ms: int = 3000, max_len: int = 1024) -> bytes:
        if not 1 <= timeout_ms <= 65535 or not 1 <= max_len <= 2048:
            raise ValueError("Invalid TCP server receive parameters")
        payload = timeout_ms.to_bytes(2, "little") + max_len.to_bytes(2, "little")
        return self._check_status(self.request(CMD_TCP_SERVER_RECEIVE, payload, timeout=timeout_ms / 1000 + 1.0))

    def tcp_server_close(self) -> None:
        self._check_status(self.request(CMD_TCP_SERVER_CLOSE))

    # --------------------------------------------------------
    # UDP
    # --------------------------------------------------------

    def udp_open(self, local_port: int = 0) -> None:
        if not 0 <= local_port <= 65535:
            raise ValueError("UDP local port must be 0..65535")
        self._check_status(self.request(CMD_UDP_OPEN, local_port.to_bytes(2, "little")))

    def udp_send(self, host: str, port: int, data: bytes) -> int:
        if not 1 <= port <= 65535:
            raise ValueError("UDP port must be 1..65535")
        if len(data) > 2048:
            raise ValueError("UDP payload is limited to 2048 bytes")
        payload = self._pack_ip(host) + port.to_bytes(2, "little") + data
        result = self._check_status(self.request(CMD_UDP_SEND, payload))
        if len(result) != 2:
            raise RuntimeError("Invalid UDP_SEND response")
        return int.from_bytes(result, "little")

    def udp_receive(self, timeout_ms: int = 3000, max_len: int = 1024) -> dict:
        if not 1 <= timeout_ms <= 65535 or not 1 <= max_len <= 2048:
            raise ValueError("Invalid UDP receive parameters")
        payload = timeout_ms.to_bytes(2, "little") + max_len.to_bytes(2, "little")
        result = self._check_status(self.request(CMD_UDP_RECEIVE, payload, timeout=timeout_ms / 1000 + 1.0))
        if len(result) < 6:
            raise RuntimeError("Invalid UDP_RECEIVE response")
        return {
            "host": ".".join(map(str, result[:4])),
            "port": int.from_bytes(result[4:6], "little"),
            "data": result[6:],
        }

    def udp_close(self) -> None:
        self._check_status(self.request(CMD_UDP_CLOSE))

    # --------------------------------------------------------
    # Bluetooth Classic
    # --------------------------------------------------------

    def bt_init(self) -> None:
        self._check_status(self.request(CMD_BT_INIT))

    def bt_get_info(self) -> dict:
        payload = self._check_status(self.request(CMD_BT_GET_INFO))
        offset = 0

        if len(payload) < 1:
            raise RuntimeError("Invalid BT_GET_INFO response")

        name_length = payload[offset]
        offset += 1

        if len(payload) < offset + name_length + 1:
            raise RuntimeError("Incomplete BT_GET_INFO name")

        name = payload[offset:offset + name_length].decode(
            "utf-8", errors="replace"
        )
        offset += name_length

        mac_length = payload[offset]
        offset += 1

        if len(payload) < offset + mac_length + 3:
            raise RuntimeError("Incomplete BT_GET_INFO response")

        mac = payload[offset:offset + mac_length].decode(
            "ascii", errors="replace"
        )
        offset += mac_length

        return {
            "name": name,
            "mac": mac,
            "initialized": bool(payload[offset]),
            "scanning": bool(payload[offset + 1]),
            "connected": bool(payload[offset + 2]),
        }

    def bt_status(self) -> dict:
        payload = self._check_status(self.request(CMD_BT_STATUS))
        if len(payload) != 3:
            raise RuntimeError("Invalid BT_STATUS response")
        return {
            "initialized": bool(payload[0]),
            "scanning": bool(payload[1]),
            "connected": bool(payload[2]),
        }

    def bt_scan_start(self, seconds: int = 10) -> None:
        if not 1 <= seconds <= 30:
            raise ValueError("Bluetooth scan duration must be 1..30 seconds")
        self._check_status(
            self.request(
                CMD_BT_SCAN_START,
                seconds.to_bytes(2, "little"),
            )
        )

    def bt_scan_stop(self) -> None:
        self._check_status(self.request(CMD_BT_SCAN_STOP))

    def bt_ble_scan_start(self, seconds: int = 10) -> None:
        if not 1 <= seconds <= 30:
            raise ValueError("BLE scan duration must be 1..30 seconds")
        self._check_status(
            self.request(
                CMD_BT_BLE_SCAN_START,
                seconds.to_bytes(2, "little"),
            )
        )

    def bt_ble_scan_stop(self) -> None:
        self._check_status(self.request(CMD_BT_BLE_SCAN_STOP))

    def bt_connect(self, address: str, timeout: float = 12.0) -> None:
        normalized = address.strip().upper()
        parts = normalized.split(":")
        if len(parts) != 6 or any(len(part) != 2 for part in parts):
            raise ValueError("Bluetooth MAC must be in XX:XX:XX:XX:XX:XX format")
        try:
            bytes(int(part, 16) for part in parts)
        except ValueError as exc:
            raise ValueError("Bluetooth MAC contains non-hex characters") from exc

        self._check_status(
            self.request(
                CMD_BT_CONNECT,
                normalized.encode("ascii"),
                timeout=timeout,
            )
        )

    def bt_disconnect(self) -> None:
        self._check_status(self.request(CMD_BT_DISCONNECT, timeout=12.0))

    def a2dp_get_info(self) -> dict:
        payload = self._check_status(self.request(CMD_A2DP_GET_INFO))
        if len(payload) != 23:
            raise RuntimeError("Invalid A2DP_GET_INFO response")
        target_len = payload[5]
        target = ""
        if target_len:
            if target_len != 17:
                raise RuntimeError("Invalid A2DP target address length")
            target = payload[6:23].decode("ascii", errors="replace")
        return {
            "active": bool(payload[0]),
            "connecting": bool(payload[1]),
            "tone": bool(payload[2]),
            "connection_state": payload[3],
            "audio_state": payload[4],
            "target": target,
        }

    def a2dp_get_cache(self) -> dict:
        payload = self._check_status(self.request(CMD_A2DP_GET_CACHE))
        if len(payload) != 35:
            raise RuntimeError("Invalid A2DP_GET_CACHE response")
        mac_len = payload[0]
        if mac_len not in (0, 17):
            raise RuntimeError("Invalid cached A2DP MAC length")
        cached_mac = payload[1:18].decode("ascii", errors="replace") if mac_len == 17 else ""
        name_len = payload[18]
        if name_len > 16 or 19 + name_len > len(payload):
            raise RuntimeError("Invalid cached A2DP name length")
        cached_name = payload[19:19 + name_len].decode("utf-8", errors="replace")
        return {"cached_target": cached_mac, "cached_name": cached_name}

    def a2dp_connect(self, mac: str, timeout: float = 3.0) -> None:
        # Legacy/manual path. The address MUST be a Classic Bluetooth MAC,
        # not a BLE advertisement address.
        mac = mac.strip().upper()
        if not re.fullmatch(r"[0-9A-F]{2}(?::[0-9A-F]{2}){5}", mac):
            raise ValueError("Bluetooth Classic MAC must be in XX:XX:XX:XX:XX:XX format")
        self._check_status(
            self.request(CMD_A2DP_CONNECT, mac.encode("ascii"), timeout=timeout)
        )

    def a2dp_connect_name(self, name: str, timeout: float = 3.0) -> None:
        # Preferred path: the ESP32-A2DP library performs Classic inquiry and
        # selects the actual Classic MAC from the device name.
        name = name.strip()
        encoded = name.encode("utf-8")
        if not 1 <= len(encoded) <= 64:
            raise ValueError("Bluetooth device name must be 1..64 UTF-8 bytes")
        self._check_status(
            self.request(CMD_A2DP_CONNECT_NAME, encoded, timeout=timeout)
        )

    def a2dp_connect_auto(self, timeout: float = 5.0) -> None:
        # Preferred reconnect path: the ESP32 tries its cached Classic MAC
        # first, then falls back to the cached/default device name.
        self._check_status(
            self.request(CMD_A2DP_CONNECT_AUTO, timeout=timeout)
        )

    def a2dp_clear_cache(self) -> None:
        self._check_status(self.request(CMD_A2DP_CLEAR_CACHE))

    def a2dp_test_tone(self) -> None:
        self._check_status(self.request(CMD_A2DP_TEST_TONE))

    def a2dp_stop_tone(self) -> None:
        self._check_status(self.request(CMD_A2DP_STOP_TONE))

    def a2dp_disconnect(self) -> None:
        self._check_status(self.request(CMD_A2DP_DISCONNECT, timeout=12.0))

    def a2dp_status(self) -> dict:
        payload = self._check_status(self.request(CMD_A2DP_STATUS))
        if len(payload) != 5:
            raise RuntimeError("Invalid A2DP_STATUS response")
        return {
            "active": bool(payload[0]),
            "connecting": bool(payload[1]),
            "tone": bool(payload[2]),
            "connection_state": payload[3],
            "audio_state": payload[4],
        }

    # --------------------------------------------------------
    # PC audio streaming
    # --------------------------------------------------------

    def audio_start(self, sample_rate: int = 22050, channels: int = 1, bits: int = 16) -> None:
        if sample_rate not in (11025, 22050, 44100) or channels not in (1, 2) or bits != 16:
            raise ValueError("Supported audio profiles: 11025/22050/44100 Hz, mono/stereo, 16-bit")
        payload = int(sample_rate).to_bytes(4, "little") + bytes([channels, bits])
        self._check_status(self.request(CMD_AUDIO_START, payload=payload))

    def audio_stop(self) -> None:
        self._check_status(self.request(CMD_AUDIO_STOP))

    def audio_send(self, pcm_block: bytes) -> None:
        self.audio_send_batch(pcm_block)

    def audio_send_batch(self, pcm_payload: bytes) -> None:
        if not pcm_payload:
            raise ValueError("Audio PCM payload cannot be empty")
        if len(pcm_payload) < 5 or pcm_payload[0] != 0x50:
            raise ValueError("Audio PCM payload must be a complete raw PCM block")
        if len(pcm_payload) > 8192:
            raise ValueError("Audio PCM block is too large")
        frame = build_frame(
            TYPE_REQUEST, CMD_AUDIO_DATA, self._next_sequence(), pcm_payload
        )
        self.transport.write(frame)

    def audio_status(self) -> dict:
        payload = self._check_status(self.request(CMD_AUDIO_STATUS, timeout=5.0))
        if len(payload) != 36:
            raise RuntimeError("Invalid AUDIO_STATUS response")
        return {
            "streaming": bool(payload[0]),
            "primed": bool(payload[1]),
            "sample_rate": int.from_bytes(payload[2:6], "little"),
            "channels": payload[6],
            "bits": payload[7],
            "buffer_used": int.from_bytes(payload[8:12], "little"),
            "buffer_capacity": int.from_bytes(payload[12:16], "little"),
            "bytes_received": int.from_bytes(payload[16:20], "little"),
            "bytes_dropped": int.from_bytes(payload[20:24], "little"),
            "underruns": int.from_bytes(payload[24:28], "little"),
            "a2dp_callback_count": int.from_bytes(payload[28:32], "little"),
            "a2dp_callback_bytes": int.from_bytes(payload[32:36], "little"),
        }

    # --------------------------------------------------------
    # Events
    # --------------------------------------------------------

    @staticmethod
    def decode_event(frame: Frame) -> Optional[dict]:
        if frame.frame_type != TYPE_EVENT:
            return None
        payload = frame.payload

        if frame.cmd == EVT_WIFI_NETWORK_FOUND:
            if len(payload) < 4:
                return None
            ssid_length = payload[0]
            if len(payload) < 1 + ssid_length + 4:
                return None
            offset = 1 + ssid_length
            return {
                "event": "WIFI_NETWORK_FOUND",
                "ssid": payload[1:offset].decode("utf-8", errors="replace"),
                "rssi": int.from_bytes(payload[offset:offset + 2], "little", signed=True),
                "channel": payload[offset + 2],
                "auth": payload[offset + 3],
                "auth_name": AUTH_NAMES.get(payload[offset + 3], f"AUTH_{payload[offset + 3]}"),
            }

        if frame.cmd == EVT_WIFI_SCAN_DONE:
            return {
                "event": "WIFI_SCAN_DONE",
                "count": int.from_bytes(payload[:2], "little") if len(payload) >= 2 else 0,
            }

        if frame.cmd == EVT_WIFI_CONNECTED:
            ssid = ""
            if payload:
                length = payload[0]
                if len(payload) >= 1 + length:
                    ssid = payload[1:1 + length].decode("utf-8", errors="replace")
            return {"event": "WIFI_CONNECTED", "ssid": ssid}

        if frame.cmd == EVT_WIFI_GOT_IP:
            if len(payload) != 4:
                return None
            return {"event": "WIFI_GOT_IP", "ip": ".".join(map(str, payload))}

        if frame.cmd == EVT_WIFI_DISCONNECTED:
            return {"event": "WIFI_DISCONNECTED"}

        if frame.cmd == EVT_WIFI_CONNECT_FAILED:
            return {"event": "WIFI_CONNECT_FAILED"}

        if frame.cmd in (EVT_TCP_CONNECTED, EVT_TCP_SERVER_CONNECTED):
            if len(payload) != 6:
                return None
            return {
                "event": "TCP_CONNECTED" if frame.cmd == EVT_TCP_CONNECTED else "TCP_SERVER_CONNECTED",
                "host": ".".join(map(str, payload[:4])),
                "port": int.from_bytes(payload[4:6], "little"),
            }

        if frame.cmd == EVT_TCP_DISCONNECTED:
            return {"event": "TCP_DISCONNECTED"}

        if frame.cmd == EVT_BT_DEVICE_FOUND:
            offset = 0

            if len(payload) < 1:
                return None

            address_length = payload[offset]
            offset += 1

            if len(payload) < offset + address_length + 1:
                return None

            address = payload[offset:offset + address_length].decode(
                "ascii", errors="replace"
            )
            offset += address_length

            name_length = payload[offset]
            offset += 1

            if len(payload) < offset + name_length + 2:
                return None

            name = payload[offset:offset + name_length].decode(
                "utf-8", errors="replace"
            )
            offset += name_length

            rssi = int.from_bytes(
                payload[offset:offset + 2],
                byteorder="little",
                signed=True,
            )

            return {
                "event": "BT_DEVICE_FOUND",
                "address": address,
                "name": name,
                "rssi": rssi,
            }

        if frame.cmd == EVT_BT_DEVICE_DETAIL:
            offset = 0

            if len(payload) < 1:
                return None

            address_length = payload[offset]
            offset += 1
            if len(payload) < offset + address_length + 1:
                return None

            address = payload[offset:offset + address_length].decode(
                "ascii", errors="replace"
            )
            offset += address_length

            name_length = payload[offset]
            offset += 1
            if len(payload) < offset + name_length + 1 + 1 + 4 + 1 + 1 + 2 + 1:
                return None

            name = payload[offset:offset + name_length].decode(
                "utf-8", errors="replace"
            )
            offset += name_length

            rssi = int.from_bytes(payload[offset:offset + 1], "little", signed=True)
            offset += 1

            has_cod = bool(payload[offset])
            offset += 1

            cod = int.from_bytes(payload[offset:offset + 4], "little")
            offset += 4

            major = payload[offset]
            offset += 1
            minor = payload[offset]
            offset += 1
            service = int.from_bytes(payload[offset:offset + 2], "little")
            offset += 2
            audio_capable = bool(payload[offset])

            return {
                "event": "BT_DEVICE_DETAIL",
                "address": address,
                "name": name,
                "rssi": rssi,
                "has_cod": has_cod,
                "cod": cod,
                "major": major,
                "minor": minor,
                "service": service,
                "audio_capable": audio_capable,
            }

        if frame.cmd == EVT_BT_BLE_DEVICE_FOUND:
            offset = 0
            if len(payload) < 1:
                return None

            address_length = payload[offset]
            offset += 1
            if len(payload) < offset + address_length + 1:
                return None
            address = payload[offset:offset + address_length].decode("ascii", errors="replace")
            offset += address_length

            name_length = payload[offset]
            offset += 1
            if len(payload) < offset + name_length + 1 + 2 + 1:
                return None
            name = payload[offset:offset + name_length].decode("utf-8", errors="replace")
            offset += name_length
            rssi = int.from_bytes(payload[offset:offset + 1], "little", signed=True)
            offset += 1
            detail_length = int.from_bytes(payload[offset:offset + 2], "little")
            offset += 2
            if len(payload) < offset + detail_length + 1:
                return None
            details = payload[offset:offset + detail_length].decode("utf-8", errors="replace")
            offset += detail_length
            qcy_hint = bool(payload[offset])

            return {
                "event": "BT_BLE_DEVICE_FOUND",
                "address": address,
                "name": name,
                "rssi": rssi,
                "details": details,
                "qcy_hint": qcy_hint,
            }

        if frame.cmd in (
            EVT_BT_CONNECTING,
            EVT_BT_CONNECTED,
            EVT_BT_CONNECT_FAIL,
            EVT_BT_DISCONNECTED,
        ):
            if len(payload) < 1:
                return None

            address_length = payload[0]
            if len(payload) < 1 + address_length:
                return None

            address = payload[1:1 + address_length].decode(
                "ascii", errors="replace"
            )

            event_names = {
                EVT_BT_CONNECTING: "BT_CONNECTING",
                EVT_BT_CONNECTED: "BT_CONNECTED",
                EVT_BT_CONNECT_FAIL: "BT_CONNECT_FAIL",
                EVT_BT_DISCONNECTED: "BT_DISCONNECTED",
            }

            return {
                "event": event_names[frame.cmd],
                "address": address,
            }

        if frame.cmd == EVT_BT_SCAN_DONE:
            return {
                "event": "BT_SCAN_DONE",
                "count": int.from_bytes(payload[:2], "little")
                if len(payload) >= 2 else 0,
            }

        if frame.cmd == EVT_BT_BLE_SCAN_DONE:
            return {
                "event": "BT_BLE_SCAN_DONE",
                "count": int.from_bytes(payload[:2], "little")
                if len(payload) >= 2 else 0,
            }

        if frame.cmd == EVT_SERIAL_LOG:
            if len(payload) < 5:
                return None
            timestamp_ms = int.from_bytes(payload[0:4], "little")
            type_length = payload[4]
            if len(payload) < 5 + type_length:
                return None
            log_type = payload[5:5 + type_length].decode("ascii", errors="replace") or "Log"
            message = payload[5 + type_length:].decode("utf-8", errors="replace")
            return {
                "event": "ESP32_SERIAL_LOG",
                "timestamp_ms": timestamp_ms,
                "type": log_type,
                "message": message,
            }

        if frame.cmd in (
            EVT_A2DP_CONNECTING,
            EVT_A2DP_CLASSIC_FOUND,
            EVT_A2DP_CONNECTED,
            EVT_A2DP_AUDIO_STARTED,
            EVT_A2DP_AUDIO_STOPPED,
            EVT_A2DP_DISCONNECTED,
            EVT_A2DP_CONNECT_FAIL,
        ):
            event_names = {
                EVT_A2DP_CONNECTING: "A2DP_CONNECTING",
                EVT_A2DP_CLASSIC_FOUND: "A2DP_CLASSIC_FOUND",
                EVT_A2DP_CONNECTED: "A2DP_CONNECTED",
                EVT_A2DP_AUDIO_STARTED: "A2DP_AUDIO_STARTED",
                EVT_A2DP_AUDIO_STOPPED: "A2DP_AUDIO_STOPPED",
                EVT_A2DP_DISCONNECTED: "A2DP_DISCONNECTED",
                EVT_A2DP_CONNECT_FAIL: "A2DP_CONNECT_FAIL",
            }

            if frame.cmd == EVT_A2DP_CONNECT_FAIL and len(payload) == 0:
                return {
                    "event": "A2DP_CONNECT_FAIL",
                    "address": "",
                }

            if len(payload) != 18 or payload[0] != 17:
                return None

            address = payload[1:18].decode("ascii", errors="replace")
            return {"event": event_names[frame.cmd], "address": address}

        return None
