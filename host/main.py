import ipaddress
import re
import socket
import subprocess
import sys
import threading

from audio_config import AudioProfile, DEFAULT_AUDIO_PROFILE, SUPPORTED_SAMPLE_RATES, SUPPORTED_CHANNELS, AUDIO_BLOCK_SAMPLES, format_audio_profile
from audio_transport import SerialAudioTransport

from audio import (
    PcAudioStreamer,
    get_loopback_devices,
    describe_default_audio_path,
    probe_loopback_devices,
    choose_active_loopback,
    set_preferred_loopback_by_index,
    get_preferred_loopback_id,
    self_test_loopback,
    monitor_real_audio,
)
import time

from esp32 import Esp32Device, AUTH_NAMES

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM5"
BAUDRATE = 921600
PC_START_MONOTONIC = time.monotonic()


def pc_log(message: str, log_type: str = "Log") -> None:
    elapsed_ms = int((time.monotonic() - PC_START_MONOTONIC) * 1000.0)
    print(f"[PC, {log_type}, {elapsed_ms} ms] {message}", flush=True)


def separator() -> None:
    print("-" * 60)


def pass_line(name: str, detail: str = "") -> None:
    print(f"[PASS] {name}" + (f": {detail}" if detail else ""))


def fail_line(name: str, detail: str) -> None:
    print(f"[FAIL] {name}: {detail}")


EXPECTED_FIRMWARE_VERSION = "2.3.9"


def print_system_tests(device: Esp32Device) -> None:
    print("PING")
    print(f"  -> {device.ping()}")

    print("GET_VERSION")
    version = device.get_version()
    print(f"  -> {version}")
    if version != EXPECTED_FIRMWARE_VERSION:
        print(
            f"  [WARNING] Expected firmware {EXPECTED_FIRMWARE_VERSION}, "
            f"received {version}."
        )

    print("GET_INFO")
    info = device.get_info()
    print(f"  -> Device: {info['device_name']}")
    print(f"  -> MAC:    {info['mac']}")


def print_wifi_status(device: Esp32Device) -> None:
    status = device.wifi_status()
    print(
        f"State={status['state']}, "
        f"SSID={status['ssid']!r}, "
        f"RSSI={status['rssi']} dBm, "
        f"IP={status['ip']}"
    )


def run_wifi_scan(device: Esp32Device) -> None:
    print("Starting Wi-Fi scan...")
    device.wifi_scan()
    print("Waiting for scan events...")
    networks = []
    deadline = time.monotonic() + 15.0
    while time.monotonic() < deadline:
        for frame in device.poll_events():
            event = device.decode_event(frame)
            if event is None:
                continue
            if event["event"] == "WIFI_NETWORK_FOUND":
                networks.append(event)
                auth = event["auth_name"]
                print(
                    f"  {len(networks):2d}. "
                    f"{event['ssid']!r:32} "
                    f"{event['rssi']:4d} dBm "
                    f"Ch {event['channel']:2d} "
                    f"{auth}"
                )
            elif event["event"] == "WIFI_SCAN_DONE":
                print(f"Scan complete: {event['count']} network(s)")
                return
        time.sleep(0.02)
    print("Scan timeout.")


def wait_for_wifi_connection(device: Esp32Device, timeout: float = 15.0) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for frame in device.poll_events():
            event = device.decode_event(frame)
            if not event:
                continue
            if event["event"] == "WIFI_CONNECTED":
                print(f"Connected to: {event['ssid']!r}")
            elif event["event"] == "WIFI_GOT_IP":
                print(f"IP address: {event['ip']}")
                return True
            elif event["event"] == "WIFI_CONNECT_FAILED":
                print("Wi-Fi connection failed.")
                return False
            elif event["event"] == "WIFI_DISCONNECTED":
                print("Wi-Fi disconnected.")
                return False
        time.sleep(0.05)
    print("Connection timeout.")
    return False


def run_wifi_connect(device: Esp32Device) -> None:
    print()
    print("Wi-Fi connection test")
    print("Enter the SSID and password.")
    ssid = input("SSID: ").strip()
    password = input("Password: ")
    if not ssid:
        print("SSID cannot be empty.")
        return
    device.wifi_connect(ssid, password)
    print("Connection request accepted.")
    if wait_for_wifi_connection(device):
        print_wifi_status(device)


def require_connected(device: Esp32Device) -> str | None:
    status = device.wifi_status()
    if status["state"] != "CONNECTED":
        print("Wi-Fi is not connected.")
        return None
    return status["ip"]


def run_tcp_client(device: Esp32Device) -> None:
    print("TCP client test")
    host = input("Target IPv4: ").strip()
    port = int(input("Target TCP port: ").strip())
    message = input("Message: ").encode("utf-8")
    device.tcp_connect(host, port)
    print(f"TCP connected to {host}:{port}")
    sent = device.tcp_send(message)
    print(f"Sent {sent} byte(s). Waiting for response...")
    try:
        data = device.tcp_receive(5000, 2048)
        print(f"Received {len(data)} byte(s): {data!r}")
    finally:
        device.tcp_close()
        print("TCP client closed.")


def run_tcp_server(device: Esp32Device) -> None:
    print("TCP server test")
    port = int(input("Listen port: ").strip())
    device.tcp_server_start(port)
    print(f"ESP32 TCP server listening on {port}")
    peer = device.tcp_server_accept(30000)
    print(f"Client connected: {peer['host']}:{peer['port']}")
    try:
        data = device.tcp_server_receive(10000, 2048)
        print(f"Received {len(data)} byte(s): {data!r}")
        reply = input("Reply: ").encode("utf-8")
        print(f"Sent {device.tcp_server_send(reply)} byte(s).")
    finally:
        device.tcp_server_close()
        print("TCP server closed.")


def run_udp(device: Esp32Device) -> None:
    print("UDP test")
    host = input("Destination IPv4: ").strip()
    port = int(input("Destination UDP port: ").strip())
    message = input("Message: ").encode("utf-8")
    device.udp_open(0)
    try:
        sent = device.udp_send(host, port, message)
        print(f"Sent {sent} byte(s). Waiting for response...")
        packet = device.udp_receive(5000, 2048)
        print(
            f"Received from {packet['host']}:{packet['port']} "
            f"{len(packet['data'])} byte(s): {packet['data']!r}"
        )
    finally:
        device.udp_close()
        print("UDP socket closed.")


def _ipv4_prefix24(ip: str) -> str:
    parts = ip.split(".")
    if len(parts) != 4:
        return ""
    return ".".join(parts[:3])


def _local_ipv4_interfaces() -> list[tuple[str, str]]:
    """Return local IPv4 address/subnet-mask pairs on Windows.

    getaddrinfo(hostname) is not reliable on Windows when VPN/virtual adapters
    are installed, so system tests use ipconfig output to identify the actual
    interface that shares a subnet with the ESP32.
    """
    interfaces = []

    # Windows-specific, but this project is a Windows PC test application.
    try:
        text = subprocess.check_output(
            ["ipconfig"],
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except (OSError, subprocess.SubprocessError):
        return interfaces

    ipv4 = None
    mask = None
    ipv4_re = re.compile(r"IPv4[^:]*:\s*([0-9.]+)", re.IGNORECASE)
    mask_re = re.compile(r"Subnet Mask[^:]*:\s*([0-9.]+)", re.IGNORECASE)

    lines = text.splitlines()
    for line in lines:
        m = ipv4_re.search(line)
        if m:
            ipv4 = m.group(1)
            mask = None
            continue

        m = mask_re.search(line)
        if m and ipv4:
            mask = m.group(1)
            if ipv4 != "127.0.0.1":
                interfaces.append((ipv4, mask))
            ipv4 = None
            mask = None

    # Also collect addresses known by Python, in case localized ipconfig output
    # prevented subnet-mask parsing. These entries intentionally have no mask and
    # are used only as a last-resort diagnostic, not for target selection.
    try:
        host = socket.gethostname()
        for item in socket.getaddrinfo(host, None, socket.AF_INET, socket.SOCK_STREAM):
            ip = item[4][0]
            if ip and ip != "127.0.0.1" and all(ip != addr for addr, _ in interfaces):
                interfaces.append((ip, ""))
    except OSError:
        pass

    return interfaces


def get_host_ip_for_target(target_ip: str) -> str:
    target = ipaddress.IPv4Address(target_ip)
    candidates = []

    for ip, mask in _local_ipv4_interfaces():
        try:
            local = ipaddress.IPv4Address(ip)
            if mask:
                network = ipaddress.IPv4Network((local, mask), strict=False)
                if target in network:
                    return ip
            elif _ipv4_prefix24(ip) == _ipv4_prefix24(target_ip):
                candidates.append(ip)
        except ValueError:
            continue

    if candidates:
        return candidates[0]

    interfaces = ", ".join(ip for ip, _ in _local_ipv4_interfaces()) or "none"
    raise RuntimeError(
        f"No Windows network interface is on the same subnet as ESP32 {target_ip}. "
        f"Local IPv4 interfaces: {interfaces}. "
        "Connect the PC to the same LAN/Wi-Fi as the ESP32 or check VPN routing."
    )


def find_free_port(host: str = "0.0.0.0") -> int:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.bind((host, 0))
        return sock.getsockname()[1]
    finally:
        sock.close()


def test_tcp_client(device: Esp32Device, esp32_ip: str) -> None:
    host_ip = get_host_ip_for_target(esp32_ip)
    print(f"Windows LAN IP selected for ESP32: {host_ip}")
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((host_ip, 0))
    port = server.getsockname()[1]
    server.listen(1)
    server.settimeout(10.0)

    ready = threading.Event()
    finished = threading.Event()
    received = {}
    worker_error = {}

    def server_worker() -> None:
        ready.set()
        try:
            conn, addr = server.accept()
            with conn:
                conn.settimeout(5.0)
                data = conn.recv(2048)
                received["data"] = data
                received["peer"] = addr
                conn.sendall(b"TCP_CLIENT_ACK")
        except Exception as exc:
            worker_error["error"] = exc
        finally:
            finished.set()

    thread = threading.Thread(target=server_worker, daemon=True)
    thread.start()
    ready.wait(1.0)

    try:
        try:
            device.tcp_connect(host_ip, port, timeout=8.0)
        except Exception as exc:
            raise RuntimeError(
                f"ESP32 could not connect to Windows TCP listener {host_ip}:{port}. "
                "If the reported Windows IP is not on the same LAN as the ESP32, "
                "check VPN/virtual adapters first. If it is correct, check Windows "
                "Defender Firewall for inbound Python TCP connections."
            ) from exc

        payload = b"ESP32_TCP_CLIENT_TEST"
        if device.tcp_send(payload) != len(payload):
            raise RuntimeError("TCP client send length mismatch")

        response = device.tcp_receive(5000, 2048)
        if response != b"TCP_CLIENT_ACK":
            raise RuntimeError(f"Unexpected TCP response: {response!r}")

        if not finished.wait(2.0):
            raise RuntimeError("Windows TCP test server did not finish")
        if "error" in worker_error:
            raise RuntimeError(f"Windows TCP server error: {worker_error['error']}")
        if received.get("data") != payload:
            raise RuntimeError(f"Server received {received.get('data')!r}")
    finally:
        device.tcp_close()
        server.close()
        thread.join(1.0)


def test_tcp_server(device: Esp32Device, esp32_ip: str) -> None:
    port = find_free_port()
    device.tcp_server_start(port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(8.0)
    try:
        sock.connect((esp32_ip, port))
        payload = b"PC_TCP_SERVER_TEST"
        sock.sendall(payload)
        peer = device.tcp_server_accept(5000)
        if not peer["host"]:
            raise RuntimeError("TCP server peer information is empty")
        data = device.tcp_server_receive(5000, 2048)
        if data != payload:
            raise RuntimeError(f"Unexpected server payload: {data!r}")
        reply = b"ESP32_TCP_SERVER_ACK"
        if device.tcp_server_send(reply) != len(reply):
            raise RuntimeError("TCP server send length mismatch")
        response = sock.recv(2048)
        if response != reply:
            raise RuntimeError(f"Unexpected PC response: {response!r}")
    finally:
        try:
            sock.close()
        finally:
            device.tcp_server_close()


def test_udp(device: Esp32Device, esp32_ip: str) -> None:
    host_ip = get_host_ip_for_target(esp32_ip)
    print(f"Windows LAN IP selected for ESP32: {host_ip}")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((host_ip, 0))
    port = sock.getsockname()[1]
    sock.settimeout(5.0)
    device.udp_open(0)
    try:
        payload = b"ESP32_UDP_TEST"
        if device.udp_send(host_ip, port, payload) != len(payload):
            raise RuntimeError("UDP send length mismatch")

        try:
            data, addr = sock.recvfrom(2048)
        except socket.timeout as exc:
            raise RuntimeError(
                f"Windows did not receive ESP32 UDP packet on {host_ip}:{port}. "
                "If the reported Windows IP is not on the same LAN as the ESP32, "
                "check VPN/virtual adapters first. If it is correct, check Windows "
                "Defender Firewall for inbound Python UDP traffic."
            ) from exc

        if data != payload:
            raise RuntimeError(f"Unexpected UDP payload: {data!r}")

        sock.sendto(b"ESP32_UDP_ACK", addr)
        packet = device.udp_receive(5000, 2048)
        if packet["data"] != b"ESP32_UDP_ACK":
            raise RuntimeError(f"Unexpected UDP response: {packet['data']!r}")
    finally:
        device.udp_close()
        sock.close()


def print_bluetooth_info(device: Esp32Device) -> None:
    info = device.bt_get_info()
    print(
        f"Name={info['name']!r}, "
        f"MAC={info['mac']}, "
        f"Initialized={info['initialized']}, "
        f"Scanning={info['scanning']}, "
        f"Connected={info['connected']}"
    )


def run_bluetooth_scan(device: Esp32Device) -> None:
    print("Starting Bluetooth Classic GAP discovery...")
    device.bt_scan_start(15)
    deadline = time.monotonic() + 22.0
    found = 0

    while time.monotonic() < deadline:
        for frame in device.poll_events():
            event = device.decode_event(frame)
            if event is None:
                continue

            if event["event"] == "BT_DEVICE_FOUND":
                found += 1
                print(
                    f"  {found:2d}. "
                    f"{event['address']}  "
                    f"{event['rssi']:4d} dBm  "
                    f"{event['name']!r}"
                )

            elif event["event"] == "BT_DEVICE_DETAIL":
                if event["has_cod"]:
                    cod_text = f"0x{event['cod']:06X}"
                    major_text = str(event["major"])
                    audio_text = "YES" if event["audio_capable"] else "NO"
                    print(
                        f"       CoD={cod_text}  Major={major_text}  "
                        f"Audio/Video={audio_text}"
                    )

            elif event["event"] == "BT_SCAN_DONE":
                print(f"Bluetooth GAP scan complete: {event['count']} device(s)")
                return

        time.sleep(0.02)

    print("Bluetooth scan timeout.")


def run_bluetooth_ble_scan(device: Esp32Device) -> None:
    print("Starting Bluetooth LE scan...")
    print("This scan is intended to catch BLE advertisements from earbuds such as QCY T13 ANC2.")
    device.bt_ble_scan_start(15)
    deadline = time.monotonic() + 22.0
    found = 0

    while time.monotonic() < deadline:
        for frame in device.poll_events():
            event = device.decode_event(frame)
            if event is None:
                continue

            if event["event"] == "BT_BLE_DEVICE_FOUND":
                found += 1
                marker = "  [QCY/T13 hint]" if event["qcy_hint"] else ""
                print(
                    f"  {found:2d}. {event['address']}  "
                    f"{event['rssi']:4d} dBm  {event['name']!r}{marker}"
                )
                if event["details"]:
                    print(f"       {event['details']}")

            elif event["event"] == "BT_BLE_SCAN_DONE":
                print(f"Bluetooth LE scan complete: {event['count']} device(s)")
                return

        time.sleep(0.02)

    print("Bluetooth LE scan timeout.")


def run_bluetooth_connect(device: Esp32Device) -> None:
    print("Bluetooth Classic connection test (SPP)")
    address = input("Target MAC [XX:XX:XX:XX:XX:XX]: ").strip()

    try:
        device.bt_connect(address, timeout=12.0)
    except Exception as exc:
        print(f"Bluetooth connection failed: {exc}")
        return

    print(f"Bluetooth SPP connected to {address.upper()}")
    print_bluetooth_info(device)


def print_a2dp_status(device: Esp32Device) -> None:
    status = device.a2dp_status()
    conn_map = {
        0: "DISCONNECTED",
        1: "CONNECTING",
        2: "CONNECTED",
        3: "DISCONNECTING",
        255: "UNKNOWN",
    }
    audio_map = {
        0: "SUSPEND",
        1: "STARTED",
        2: "STOPPED (deprecated)",
        3: "REMOTE_SUSPEND (deprecated)",
        255: "UNKNOWN",
    }
    print(f"A2DP active:      {status['active']}")
    print(f"A2DP connecting:  {status['connecting']}")
    print(f"Test tone:        {status['tone']}")
    print(f"Connection state: {conn_map.get(status['connection_state'], status['connection_state'])}")
    print(f"Audio state:      {audio_map.get(status['audio_state'], status['audio_state'])}")
    try:
        cache = device.a2dp_get_cache()
        print(f"Cached Classic MAC: {cache['cached_target'] or '(none)'}")
        print(f"Cached target name: {cache['cached_name'] or '(none)'}")
    except Exception:
        pass


def _wait_a2dp_result(device: Esp32Device, wait_seconds: float = 65.0) -> None:
    deadline = time.monotonic() + wait_seconds
    while time.monotonic() < deadline:
        for frame in device.poll_events():
            event = device.decode_event(frame)
            if not event:
                continue
            if event["event"] == "A2DP_CLASSIC_FOUND":
                print(f"Classic A2DP target discovered: {event['address']}")
            elif event["event"] == "A2DP_CONNECTING":
                print(f"A2DP connecting to {event['address']}")
            elif event["event"] == "A2DP_CONNECTED":
                print(f"A2DP connected to {event['address']}")
                return
            elif event["event"] == "A2DP_CONNECT_FAIL":
                address = event.get("address", "")
                if address:
                    print(f"A2DP connection failed: {address}")
                else:
                    print("A2DP connection failed: target not found or not discoverable")
                return
            elif event["event"] == "A2DP_DISCONNECTED":
                print(f"A2DP disconnected: {event['address']}")
                return
        time.sleep(0.05)
    try:
        status = device.a2dp_status()
        if status["connection_state"] == 2:
            print("A2DP connected (confirmed by status query).")
            return
    except Exception:
        pass
    print("A2DP connection wait timeout. Check device status.")


def run_a2dp_connect(device: Esp32Device) -> None:
    print("Bluetooth A2DP Source connection test (legacy Classic MAC)")
    print("Use a Classic Bluetooth MAC only; a BLE MAC from the BLE scan is not valid here.")
    address = input("Target Classic MAC [XX:XX:XX:XX:XX:XX]: ").strip()
    device.a2dp_connect(address)
    print("A2DP Source started; waiting for target...")
    _wait_a2dp_result(device)


def run_a2dp_connect_name(device: Esp32Device) -> None:
    print("Bluetooth A2DP Source connection by Classic Bluetooth name")
    print("The ESP32 will run Classic GAP inquiry and capture the actual Classic MAC.")
    print("For QCY T13 ANC2, use a prefix such as: QCY T13 ANC2")
    name = input("Target Classic name/prefix: ").strip()
    device.a2dp_connect_name(name)
    print("A2DP Source started; discovering Classic target...")
    _wait_a2dp_result(device)


def run_a2dp_connect_auto(device: Esp32Device) -> None:
    pc_log("A2DP automatic reconnect requested", "Log")
    print("Bluetooth A2DP Source automatic reconnect")
    print("The ESP32 will try the cached Classic MAC first and then fall back to the cached/default name.")
    device.a2dp_connect_auto()
    pc_log("A2DP automatic reconnect command accepted", "Result")
    print("A2DP automatic connection started; waiting up to 65s for result...")
    _wait_a2dp_result(device)


def run_a2dp_clear_cache(device: Esp32Device) -> None:
    device.a2dp_clear_cache()
    print("Cached A2DP Classic target cleared.")


def run_a2dp_tone(device: Esp32Device) -> None:
    pc_log("A2DP test tone requested", "Log")
    status = device.a2dp_status()
    # The underlying A2DP connection state is authoritative. The library
    # audio_state is deprecated on this ESP-IDF combination and can remain
    # STOPPED even when the PCM callback is already streaming.
    if status["connection_state"] != 2:
        print("A2DP is not connected.")
        print(f"Connection state: {status['connection_state']}")
        return

    device.a2dp_test_tone()
    pc_log("A2DP tone command accepted", "Result")
    print("1 kHz test tone command accepted; waiting for first PCM callback...")

    deadline = time.monotonic() + 3.0
    while time.monotonic() < deadline:
        for frame in device.poll_events():
            event = device.decode_event(frame)
            if not event:
                continue
            if event["event"] == "A2DP_AUDIO_STARTED":
                pc_log("A2DP first audio event received", "State")
                print("A2DP PCM callback active; test tone is streaming.")
                return
            if event["event"] == "A2DP_DISCONNECTED":
                print("A2DP disconnected while starting the tone.")
                return
        time.sleep(0.01)

    try:
        current = device.a2dp_status()
        print(
            "A2DP tone is enabled, but the first PCM callback was not observed "
            f"within 3s (connection={current['connection_state']}, "
            f"audio={current['audio_state']})."
        )
    except Exception:
        print("A2DP tone is enabled; status re-check failed.")


def run_a2dp_stop_tone(device: Esp32Device) -> None:
    pc_log("A2DP test tone stop requested", "Log")
    device.a2dp_stop_tone()
    print("A2DP test tone stopped.")


def run_a2dp_disconnect(device: Esp32Device) -> None:
    pc_log("A2DP disconnect requested", "Log")
    device.a2dp_disconnect()
    print("A2DP disconnect request accepted.")


def run_bluetooth_disconnect(device: Esp32Device) -> None:
    device.bt_disconnect()
    print("Bluetooth disconnect request accepted.")
    print_bluetooth_info(device)

def run_bluetooth(device: Esp32Device) -> None:
    print()
    separator()
    print("Bluetooth Classic + BLE Tests")
    separator()

    device.bt_init()
    print("Bluetooth initialization: OK")
    print_bluetooth_info(device)
    print()
    run_bluetooth_scan(device)
    print_bluetooth_info(device)


def run_network_system_tests(device: Esp32Device) -> None:
    print()
    separator()
    print("Network System Tests")
    separator()

    status = device.wifi_status()
    if status["state"] != "CONNECTED":
        print("[SKIP] Network tests: Wi-Fi is not connected.")
        return

    esp32_ip = status["ip"]
    print(f"ESP32 IP: {esp32_ip}")

    tests = [
        ("TCP client", lambda: test_tcp_client(device, esp32_ip)),
        ("TCP server", lambda: test_tcp_server(device, esp32_ip)),
        ("UDP", lambda: test_udp(device, esp32_ip)),
    ]

    for name, test in tests:
        try:
            test()
            pass_line(name)
        except Exception as exc:
            fail_line(name, str(exc))


def print_audio_status(device: Esp32Device, streamer: PcAudioStreamer | None) -> None:
    status = device.audio_status()
    used_pct = (
        100.0 * status["buffer_used"] / status["buffer_capacity"]
        if status["buffer_capacity"] else 0.0
    )
    print(f"Audio streaming:  {status['streaming']}")
    print(f"Audio primed:     {status['primed']}")
    print(f"PCM/A2DP format:  {status['sample_rate']} Hz, {status['channels']} ch, {status['bits']}-bit")
    print("PCM transport:   raw PCM16 (no compression)")
    print(f"Block size:       {AUDIO_BLOCK_SAMPLES} samples/channel")
    print(f"Buffer:           {status['buffer_used']}/{status['buffer_capacity']} bytes ({used_pct:.1f}%)")
    print(f"Received:         {status['bytes_received']} bytes")
    print(f"Dropped:          {status['bytes_dropped']} bytes")
    print(f"Underruns:        {status['underruns']}" )
    print(f"A2DP callbacks:   {status.get('a2dp_callback_count', 0)}")
    print(f"A2DP callback bytes: {status.get('a2dp_callback_bytes', 0)}")
    if streamer is not None:
        print(f"PC capture:       {streamer.status_text()}")


def choose_and_show_loopback() -> None:
    print()
    separator()
    print("WASAPI loopback selection")
    separator()
    try:
        devices = get_loopback_devices()
        if not devices:
            print("No WASAPI loopback devices found.")
            return
        for item in devices:
            print(
                f"{int(item['index']):3d}. {item['name']} | "
                f"channels={int(item['channels'])}"
            )
        print()
        selected_index = input("Enter loopback index, or press Enter for Windows default: ").strip()
        if selected_index:
            selected = set_preferred_loopback_by_index(int(selected_index))
            print(f"SELECTED LOOPBACK: {int(selected['index'])} | {selected['name']}")
        else:
            selected, _ = choose_active_loopback()
            print(f"AUTO-SELECTED LOOPBACK: {selected['name']}")
        preferred_id = get_preferred_loopback_id()
        if preferred_id:
            print("Selection saved for PC audio streaming in this session.")
    except Exception as exc:
        print(f"WASAPI loopback selection failed: {exc}")


def list_audio_devices() -> None:
    print()
    separator()
    print("WASAPI loopback devices")
    separator()
    try:
        devices = get_loopback_devices()
    except Exception as exc:
        print(f"Audio device query failed: {exc}")
        return
    if not devices:
        print("No WASAPI loopback devices found.")
        return
    for device in devices:
        print(
            f"{device['index']:3d}. {device['name']} | "
            f"rate={48000} Hz | "
            f"channels={int(device['channels'])}"
        )


def test_wasapi_capture_path() -> None:
    print()
    separator()
    print("WASAPI capture path diagnostic / generated-tone self-test")
    separator()
    try:
        output, default_loopback = describe_default_audio_path()
        print(f"Default WASAPI output : {output['name']}")
        print(f"Default loopback     : {default_loopback['name']}")
        print()
        print("Running a self-contained 1 kHz tone test on every Windows speaker.")
        print("This verifies that SoundCard/WASAPI loopback itself is functioning.")
        print("It does NOT identify where your real music application is routed.")
        print()
        results = self_test_loopback()
        if not results:
            print("No Windows speaker endpoints available.")
            return
        for item in results:
            if item.get("error"):
                print(f"- {item['speaker']} -> ERROR: {item['error']}")
            else:
                print(
                    f"- {item['speaker']} -> loopback={item['loopback']} | "
                    f"captured={int(item['captured_bytes'])} bytes | "
                    f"peak={int(item['peak'])} | rms={int(item['rms'])}"
                )
        active = [item for item in results if not item.get("error") and int(item.get("peak", 0)) > 100]
        print()
        if active:
            print("RESULT: Windows WASAPI loopback is working.")
            print("Do NOT use this generated-tone result to auto-select the PC music route.")
        else:
            print("RESULT: No speaker loopback captured the generated tone.")
    except Exception as exc:
        print(f"WASAPI capture diagnostic failed: {exc}")


def monitor_real_system_audio() -> None:
    print()
    separator()
    print("REAL Windows audio monitor / loopback routing test")
    separator()
    print("Play your normal Windows music BEFORE starting this test.")
    print("Each loopback is monitored for 3 seconds using the ACTUAL system mix.")
    print("No generated tone is used and no automatic route selection is performed.")
    print()
    try:
        results = monitor_real_audio(3.0)
    except Exception as exc:
        print(f"Real-audio monitor failed: {exc}")
        return
    if not results:
        print("No WASAPI loopback devices found.")
        return
    active = []
    for item in results:
        if item.get("error"):
            print(f"- {item['name']} | ERROR: {item['error']}")
        else:
            print(
                f"- {item['name']} | captured={int(item['captured_bytes'])} bytes | "
                f"peak={int(item['peak'])} | rms={int(item['rms'])}"
            )
            if int(item.get("peak", 0)) > 100:
                active.append(item)
    print()
    if active:
        print("RESULT: Real Windows audio was detected on at least one loopback.")
        print("Select the matching device explicitly with option 27, then use option 23.")
    else:
        print("RESULT: All loopbacks were silent while monitoring actual Windows audio.")
        print("This means the music is not reaching any of these captured endpoints.")
        print("Check Windows Sound Output and the per-app Volume Mixer routing.")

def configure_audio_profile(streamer: PcAudioStreamer) -> None:
    print()
    separator()
    print("PC audio format / profile")
    separator()
    print(f"Current: {format_audio_profile(streamer.profile)}")
    print("Supported sample rates: " + ", ".join(str(x) for x in SUPPORTED_SAMPLE_RATES) + " Hz")
    print("Channels: 1=mono, 2=stereo")
    rate_text = input("Sample rate [Enter=keep]: ").strip()
    channels_text = input("Channels [Enter=keep]: ").strip()
    try:
        rate = streamer.profile.sample_rate if not rate_text else int(rate_text)
        channels = streamer.profile.channels if not channels_text else int(channels_text)
        profile = AudioProfile(rate, channels, 16)
        profile.validate()
        streamer.set_profile(profile)
        print(f"Selected profile: {format_audio_profile(profile)}")
    except Exception as exc:
        print(f"Audio profile not changed: {exc}")

def run_pc_audio_start(device: Esp32Device, streamer: PcAudioStreamer) -> None:
    status = device.a2dp_status()
    if not status["active"] or status["connection_state"] != 2:
        print("A2DP is not connected. Connect to QCY T13 ANC2 first.")
        return
    if streamer.is_running():
        print("PC audio streaming is already running.")
        return

    print("Serial transport is fixed at 921,600 baud.")
    profile = streamer.profile
    device.audio_start(profile.sample_rate, profile.channels, profile.bits)
    streamer.start()
    status_summary = streamer.status_text().split(", sent=")[0]
    print(f"PC audio streaming started: {status_summary} -> raw PCM16 {format_audio_profile(profile)} -> ESP32 -> A2DP.")
    print(f"Transport PCM: {format_audio_profile(profile)}; A2DP output: 44.1 kHz stereo.")
    bytes_per_second = profile.sample_rate * profile.channels * (profile.bits // 8)
    print(f"UART payload: raw PCM, about {bytes_per_second/1000.0:.1f} kB/s plus protocol overhead.")


def run_pc_audio_stop(device: Esp32Device, streamer: PcAudioStreamer) -> None:
    status = device.audio_status()
    if not streamer.is_running() and not status["streaming"]:
        print("PC audio streaming is already stopped.")
        return
    streamer.stop()
    device.audio_stop()
    print("PC audio stream stopped. Serial transport remains at 921,600 baud.")

def menu(device: Esp32Device) -> None:
    streamer = PcAudioStreamer(SerialAudioTransport(device).send_audio_payload)
    while True:
        print()
        separator()
        print(f"ESP32 Wireless Dongle v{EXPECTED_FIRMWARE_VERSION}")
        separator()
        print("1. Wi-Fi scan")
        print("2. Wi-Fi connect")
        print("3. Wi-Fi disconnect")
        print("4. Wi-Fi status")
        print("5. TCP client test")
        print("6. TCP server test")
        print("7. UDP test")
        print("8. Run system tests")
        print("9. Bluetooth info / initialization")
        print("10. Bluetooth Classic GAP scan")
        print("11. Bluetooth LE scan")
        print("12. Bluetooth Classic connect (SPP)")
        print("13. Bluetooth disconnect")
        print("14. A2DP Source status")
        print("15. A2DP Source connect (Classic MAC - legacy)")
        print("16. A2DP Source connect by Classic name (recommended)")
        print("17. A2DP Source automatic reconnect (cached target)")
        print("18. A2DP test tone ON")
        print("19. A2DP test tone OFF")
        print("20. A2DP disconnect")
        print("21. Clear cached A2DP target")
        print("22. PC audio status")
        print("23. Start PC audio -> A2DP")
        print("24. Stop PC audio")
        print("25. List WASAPI loopback devices (SoundCard)")
        print("26. Test WASAPI capture path")
        print("27. WASAPI loopback selection")
        print("28. Windows audio self-test (generated tone)")
        print("29. REAL Windows audio monitor (no generated tone)")
        print("30. PC audio format / profile")
        print("31. Exit")
        choice = input("Select: ").strip()
        try:
            if choice == "1":
                print()
                run_wifi_scan(device)
            elif choice == "2":
                run_wifi_connect(device)
            elif choice == "3":
                device.wifi_disconnect()
                print("Disconnect request accepted.")
            elif choice == "4":
                print()
                print_wifi_status(device)
            elif choice == "5":
                print()
                if require_connected(device):
                    run_tcp_client(device)
            elif choice == "6":
                print()
                if require_connected(device):
                    run_tcp_server(device)
            elif choice == "7":
                print()
                if require_connected(device):
                    run_udp(device)
            elif choice == "8":
                print()
                print_system_tests(device)
                run_network_system_tests(device)
            elif choice == "9":
                print()
                device.bt_init()
                print("Bluetooth initialization: OK")
                print_bluetooth_info(device)
            elif choice == "10":
                print()
                run_bluetooth_scan(device)
            elif choice == "11":
                print()
                run_bluetooth_ble_scan(device)
            elif choice == "12":
                print()
                run_bluetooth_connect(device)
            elif choice == "13":
                print()
                run_bluetooth_disconnect(device)
            elif choice == "14":
                print()
                print_a2dp_status(device)
            elif choice == "15":
                print()
                run_a2dp_connect(device)
            elif choice == "16":
                print()
                run_a2dp_connect_name(device)
            elif choice == "17":
                print()
                run_a2dp_connect_auto(device)
            elif choice == "18":
                print()
                run_a2dp_tone(device)
            elif choice == "19":
                print()
                run_a2dp_stop_tone(device)
            elif choice == "20":
                print()
                run_a2dp_disconnect(device)
            elif choice == "21":
                print()
                run_a2dp_clear_cache(device)
            elif choice == "22":
                print()
                print_audio_status(device, streamer)
            elif choice == "23":
                print()
                run_pc_audio_start(device, streamer)
            elif choice == "24":
                print()
                run_pc_audio_stop(device, streamer)
            elif choice == "25":
                list_audio_devices()
            elif choice == "26":
                test_wasapi_capture_path()
            elif choice == "27":
                choose_and_show_loopback()
            elif choice == "28":
                test_wasapi_capture_path()
            elif choice == "29":
                monitor_real_system_audio()
            elif choice == "30":
                configure_audio_profile(streamer)
            elif choice == "31":
                if streamer.is_running():
                    run_pc_audio_stop(device, streamer)
                break
            else:
                print("Invalid selection.")
        except Exception as exc:
            print(f"ERROR: {exc}")


def main() -> None:
    device = Esp32Device(port=PORT, baudrate=BAUDRATE)
    try:
        separator()
        print("ESP32 Wireless Dongle - Protocol Test")
        print(f"Port: {PORT}")
        print(f"Baud: {BAUDRATE}")
        separator()
        device.connect()
        print("Connected.")
        print()
        print_system_tests(device)
        menu(device)
    except KeyboardInterrupt:
        print("\nInterrupted by user.")
    except Exception as exc:
        print(f"ERROR: {exc}")
    finally:
        device.disconnect()


if __name__ == "__main__":
    main()
