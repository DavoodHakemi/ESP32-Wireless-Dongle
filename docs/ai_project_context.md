# ESP32 Wireless Dongle - AI Project Context

## 0. Purpose of this file

This file is the project handoff/context document for any AI that will review, debug, refactor, extend, test, or modify the ESP32 Wireless Dongle project.

Treat this file as a compact operational map, not as a substitute for source code.

**Source of truth order:**

```text
Actual source implementation
        -> build configuration
        -> tests and real runtime evidence
        -> documentation
        -> comments / names / assumptions
```

When this document conflicts with current source code, current source code wins. Do not silently reconcile the conflict; report it.

Current repository snapshot at the time this document was created:

- Repository: `DavoodHakemi/ESP32-Wireless-Dongle`
- Default branch: `master`
- Current master commit: `d2b980dcbc1baa85e9a1cd524aefc65e02a264fe`
- Current commit message: `fix: replace embedded null literal in Bluetooth scan source`
- Firmware version: `2.3.12`
- Wire protocol version: `1`

---

# 1. Project identity

## 1.1 Goal

The project turns an ESP32-WROOM-32 / ESP32 DevKit V1 into a Windows-controlled wireless dongle exposing a binary serial protocol over USB/UART.

The device provides:

- Wi-Fi discovery, connection, status and disconnect
- TCP client
- TCP server
- UDP
- Bluetooth Classic initialization, GAP discovery, SPP connect/disconnect and status
- BLE advertisement scanning
- Bluetooth A2DP Source
- A2DP connection by Classic MAC
- A2DP connection by Classic name
- A2DP automatic reconnect using cached Classic target data
- A2DP test tone
- A2DP status and disconnect
- PC audio -> raw PCM -> ESP32 -> A2DP
- structured ESP firmware logging delivered as protocol events

The firmware is a modular PlatformIO/C++ refactor of an older single-file Arduino firmware that was already working.

---

# 2. Target hardware and development environment

## 2.1 Hardware

- MCU: ESP32-WROOM-32
- Board: ESP32 DevKit V1 / PlatformIO board `esp32dev`
- USB-UART: CP2102
- Flash: 4 MB
- CPU target: 240 MHz
- Flash mode: QIO
- Flash frequency: 80 MHz
- Partition: `partitions/huge_app.csv`
- Primary UART/USB serial baud: `921600`

The project was historically tested through a Windows COM port such as `COM5`.

Do not hard-code a user's actual COM port into firmware or project architecture. Host configuration may select it at runtime.

## 2.2 PlatformIO

Current `platformio.ini` target:

```ini
[platformio]
default_envs = esp32dev

[env:esp32dev]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/51.03.07/platform-espressif32.zip
board = esp32dev
framework = arduino
monitor_speed = 921600
upload_speed = 921600
upload_protocol = esptool
board_build.flash_size = 4MB
board_build.flash_mode = qio
board_build.f_flash = 80000000L
board_build.partitions = partitions/huge_app.csv
board_build.filesystem = spiffs
build_flags =
    -I include
    -std=gnu++17
    -D CORE_DEBUG_LEVEL=0
lib_deps =
    https://github.com/pschatzmann/ESP32-A2DP.git#v1.8.10
lib_ldf_mode = chain+
extra_scripts = pre:scripts/platformio_framework_compat.py
```

## 2.3 Framework versions

Pinned environment:

- pioarduino PlatformIO platform `51.03.07`
- Arduino-ESP32 `3.0.7`
- ESP-IDF `5.1.4` through the selected framework package
- ESP32-A2DP `v1.8.10`

`ESP32-A2DP v1.8.11` is intentionally not used. The project documentation records an incompatibility with `ESP_A2D_AUDIO_STATE_SUSPEND` under the user's Arduino-ESP32 3.0.7 / ESP-IDF 5.1 environment.

## 2.4 Important PlatformIO compatibility script

`scripts/platformio_framework_compat.py` intentionally exposes and builds the exact Arduino-ESP32 `Network` framework library because PlatformIO LDF can omit it when `WiFi.h` includes `Network.h`.

Do not replace this with a duplicated project copy of the framework Network library unless there is a documented reason.

---

# 3. Project architecture

## 3.1 Layer model

The intended dependency direction is:

```text
Application
    -> Protocol
        -> Services
            -> Hardware/Framework/Third-party APIs
```

Transport is a separate abstraction used by the protocol layer.

## 3.2 Main components

### Application

Primary application orchestrator:

`src/application/DongleApplication.cpp`

Responsibilities:

- lifecycle
- service construction
- update ordering
- transport/protocol integration
- draining domain events
- mapping domain `EventType` to wire `protocol::EventId`

`src/main.cpp` is intentionally tiny:

```cpp
void setup() {
    application.begin();
}

void loop() {
    application.update();
}
```

Do not put business logic in `main.cpp` or `loop()`.

### Protocol

Core protocol pieces live under:

- `include/protocol/`
- `src/protocol/`

Relevant concepts:

- `ProtocolCommand`
- `CommandId`
- `EventId`
- parser
- codec
- dispatcher
- response status mapping

The dispatcher orchestrates services. It should not absorb low-level hardware implementation.

### Services

Current major service boundaries:

- Wi-Fi
- Network
- Bluetooth Classic
- BLE
- A2DP
- System

Each service owns its own domain state and publishes domain events through `IEventSink`.

### Transport

Current concrete transport:

- UART0 / CP2102 via `SerialTransport`

Transport means how bytes move. Protocol means what bytes mean. These boundaries must remain separate.

---

# 4. Runtime scheduling contract

`DongleApplication::update()` currently follows this ordering:

```text
UART RX / parser input
        |
        v
Transport.update()
        |
        v
WiFi.update()
        |
        +--> drainEvents()
        |
Network.update()
        |
        +--> drainEvents()
        |
Bluetooth.update()
        |
        +--> drainEvents()
        |
Bluetooth.postUpdate()
        |
A2DP.update()
        |
        +--> drainEvents()
```

This ordering is important.

Reasons:

- asynchronous service events are serialized by the application task
- protocol responses are not allowed to race asynchronous event frames
- event bursts are drained between service phases
- Bluetooth/A2DP callbacks do not write protocol frames directly

Do not reorder services casually.

---

# 5. Event architecture

## 5.1 Domain events

`include/core/Event.h` contains `EventType`.

Current domain events include:

- Wi-Fi network found
- Wi-Fi scan done
- Wi-Fi connected/disconnected/got IP/connect failed
- TCP connected/disconnected/server connected
- Bluetooth device found
- Bluetooth scan done
- Bluetooth connecting/connected/connect failed/disconnected
- Bluetooth device detail
- BLE device found
- BLE scan done
- A2DP connecting/connected/audio started/audio stopped/disconnected/connect failed/classic found
- audio underrun
- serial log

## 5.2 Event queue

`include/core/EventQueue.h` is a fixed-size queue.

Current values from source:

```text
CAPACITY = 16
MAX_PAYLOAD = 328 bytes
```

The queue was intentionally reduced from an older much larger allocation because every record contains a fixed payload buffer and ESP32 internal DRAM is constrained.

Current scan publishers use bounded batches so a scan does not require an enormous queue burst.

Approximate design intent:

```text
Classic/Wi-Fi batch <= 4 result entries/update
Classic result publication = device-found + detail per device + final DONE
BLE result publication <= 4 entries/update + final DONE
```

Never increase the queue just to hide an event-publication design problem without measuring DRAM impact.

## 5.3 Event flow

```text
Hardware callback / framework async result
        |
        v
Service state / domain event
        |
        v
EventQueue
        |
        v
DongleApplication::drainEvents()
        |
        v
protocol::EventId
        |
        v
ProtocolCodec
```

---

# 6. Wire protocol

## 6.1 Frame

Wire protocol version: `1`.

Frame:

```text
AA 55 | version | type | command/event | sequence | payload_length LE | payload | CRC16-CCITT-FALSE LE
```

Protocol schema is defined in:

`docs/protocol-schema.json`

Frame properties:

- SOF: `AA55`
- CRC: `CRC16-CCITT-FALSE`
- CRC initial: `0xFFFF`
- header size: 6 bytes
- payload length: little-endian
- request frame type: `1`
- response frame type: `2`
- event frame type: `3`
- error frame type: `4`

## 6.2 Response rules

A response must:

- match the request sequence
- match the command ID
- carry status as payload byte 0

Events use sequence `0` and must never be consumed as command responses.

`AUDIO_DATA (0x6D)` is deliberately one-way and has no normal response.

## 6.3 Command classes

### Immediate

Examples:

- PING
- GET_VERSION
- GET_INFO
- RESET
- SET_BAUD compatibility operation
- Wi-Fi disconnect/status
- network send/close operations
- Bluetooth init/info/status/stop/disconnect
- A2DP status/info/cache/tone controls/disconnect
- audio start/stop/status

### Async accepted

The response means acceptance, not completion.

Important async commands:

- Wi-Fi scan
- Wi-Fi connect
- Bluetooth Classic scan
- BLE scan
- Bluetooth Classic connect
- A2DP connect by MAC
- A2DP connect by name
- A2DP automatic reconnect

Completion/failure is reported by events.

### Blocking

Examples:

- TCP receive
- TCP server accept/receive
- UDP receive

These intentionally keep a request open for their explicit timeout.

---

# 7. Exact command IDs

## System

| Command | Hex |
|---|---:|
| GetInfo | `0x01` |
| GetVersion | `0x02` |
| Ping | `0x03` |
| Reset | `0x04` |
| SetBaud | `0x05` |

## Wi-Fi

| Command | Hex |
|---|---:|
| WifiScan | `0x10` |
| WifiConnect | `0x11` |
| WifiDisconnect | `0x12` |
| WifiStatus | `0x13` |

## TCP / UDP

| Command | Hex |
|---|---:|
| TcpConnect | `0x20` |
| TcpSend | `0x21` |
| TcpReceive | `0x22` |
| TcpClose | `0x23` |
| TcpServerStart | `0x24` |
| TcpServerAccept | `0x25` |
| TcpServerSend | `0x26` |
| TcpServerReceive | `0x27` |
| TcpServerClose | `0x28` |
| UdpOpen | `0x29` |
| UdpSend | `0x2A` |
| UdpReceive | `0x2B` |
| UdpClose | `0x2C` |

## Bluetooth

| Command | Hex |
|---|---:|
| BluetoothInit | `0x40` |
| BluetoothGetInfo | `0x41` |
| BluetoothScanStart | `0x42` |
| BluetoothScanStop | `0x43` |
| BluetoothStatus | `0x44` |
| BluetoothConnect | `0x45` |
| BluetoothDisconnect | `0x46` |
| BluetoothBleScanStart | `0x47` |
| BluetoothBleScanStop | `0x48` |

## A2DP / Audio

| Command | Hex |
|---|---:|
| A2dpGetInfo | `0x60` |
| A2dpConnect | `0x61` |
| A2dpTestTone | `0x62` |
| A2dpStopTone | `0x63` |
| A2dpDisconnect | `0x64` |
| A2dpStatus | `0x65` |
| A2dpConnectName | `0x66` |
| A2dpConnectAuto | `0x67` |
| A2dpClearCache | `0x68` |
| A2dpGetCache | `0x69` |
| AudioStart | `0x6A` |
| AudioStop | `0x6B` |
| AudioStatus | `0x6C` |
| AudioData | `0x6D` |

---

# 8. Exact event IDs

## Wi-Fi

| Event | Hex |
|---|---:|
| WifiNetworkFound | `0x14` |
| WifiScanDone | `0x15` |
| WifiConnected | `0x16` |
| WifiDisconnected | `0x17` |
| WifiGotIp | `0x18` |
| WifiConnectFailed | `0x19` |

## Network

| Event | Hex |
|---|---:|
| TcpConnected | `0x30` |
| TcpDisconnected | `0x31` |
| TcpServerConnected | `0x32` |

## Bluetooth

| Event | Hex |
|---|---:|
| BluetoothDeviceFound | `0x50` |
| BluetoothScanDone | `0x51` |
| BluetoothConnecting | `0x52` |
| BluetoothConnected | `0x53` |
| BluetoothConnectFailed | `0x54` |
| BluetoothDisconnected | `0x55` |
| BluetoothDeviceDetail | `0x56` |
| BluetoothBleDeviceFound | `0x57` |
| BluetoothBleScanDone | `0x58` |

## A2DP / Audio / Logging

| Event | Hex |
|---|---:|
| A2dpConnecting | `0x70` |
| A2dpConnected | `0x71` |
| A2dpAudioStarted | `0x72` |
| A2dpAudioStopped | `0x73` |
| A2dpDisconnected | `0x74` |
| A2dpConnectFailed | `0x75` |
| A2dpClassicFound | `0x76` |
| AudioUnderrun | `0x77` |
| SerialLog | `0x78` |

---

# 9. Logging / terminal contract

Firmware must not contain uncontrolled raw terminal output.

All firmware log messages go through the central logger and are encoded as `EVT_SERIAL_LOG (0x78)`.

Log event payload:

```text
timestamp_ms:u32 LE
 type_len:u8
 type:ASCII
 message:UTF-8
```

Host rendering:

```text
[ESP, Type, <timestamp> ms] message
```

Host-side UI/log output uses:

```text
[PC, Type, <elapsed> ms] message
```

When debugging terminal behavior, distinguish:

- protocol response
- structured ESP log event
- PC UI output
- raw transport bytes

Do not add `Serial.print()` / `Serial.println()` to arbitrary service code.

---

# 10. Application startup

`DongleApplication::begin()` currently does approximately:

1. initialize serial transport
2. wait 250 ms
3. initialize Wi-Fi
4. initialize A2DP subsystem
5. emit structured `Firmware %s initialized` log where the version comes from `DONGLE_FIRMWARE_VERSION`

A2DP initialization is a critical dependency for startup in current source.

The application uses dependency injection by constructing service objects and passing the logger/event sink references.

---

# 11. Bluetooth Classic architecture

Primary files:

- `include/services/bluetooth/BluetoothClassic.h`
- `src/services/bluetooth/BluetoothClassic.cpp`

## 11.1 Responsibilities

- Classic BT initialization
- Classic GAP discovery
- result collection
- SPP connection/disconnection
- local/remote address handling
- event generation
- A2DP suspension/restore boundary

## 11.2 Important lifecycle fact

Classic Bluetooth uses `BluetoothSerial` as a framework-level owner of Classic GAP/SPP state.

The current scan architecture deliberately does **not** register a second independent GAP callback.

The current code starts Classic GAP inquiry and then collects the framework-owned `BTScanResults` through `BluetoothSerial`.

This is a critical design constraint because replacing the framework callback broke the original working SPP/GAP lifecycle during earlier refactoring attempts.

## 11.3 Current scan model

```text
Command: BluetoothScanStart
        |
        v
Dispatcher accepts request
        |
        v
BluetoothClassic::scan()
        |
        v
StartPending if cold
        |
        v
BluetoothSerial / ESP GAP discovery
        |
        v
framework-owned result table
        |
        v
BluetoothClassic::completeScan()
        |
        v
bounded publication batches
        |
        v
BluetoothScanDone
```

No application-created scan worker task should be reintroduced without evidence that the framework API cannot perform the required operation.

## 11.4 Classic scan limits

- scan duration: 1..30 seconds; default 10
- maximum local result table: 32 entries
- publication: max 4 entries per application update
- event ordering: result events before `BluetoothScanDone`

## 11.5 Classic result fields

Tracked per entry:

- address
- RSSI when available
- Class of Device when available
- device name

A Classic found event carries:

```text
address length
address ASCII
name length
name UTF-8
RSSI int16
```

A detail event also carries:

- address
- name
- RSSI
- COD availability
- COD
- major class
- minor class
- service bits
- audio-capable indication

## 11.6 SPP

Classic connection uses a 17-character ASCII MAC address in the form:

```text
XX:XX:XX:XX:XX:XX
```

Do not confuse a BLE advertisement address with a Classic A2DP/SPP target address.

---

# 12. BLE architecture

Primary files:

- `include/services/bluetooth/BluetoothLE.h`
- `src/services/bluetooth/BluetoothLE.cpp`

## 12.1 Responsibilities

- BLE stack initialization
- active BLE advertisement scan
- result collection
- device name/address/RSSI extraction
- details extraction
- QCY/T13 hint generation
- bounded result publication
- scan timeout/cleanup

## 12.2 Current initialization contract

For Arduino-ESP32 3.0.7:

```cpp
BLEDevice::init("");
```

has a `void` return in the actual framework API. Do not assume a boolean-returning implementation from another framework version.

The code validates initialization using the framework state/API and records controller/Bluedroid status in structured ESP logs.

## 12.3 Current BLE scan model

BLE scanning is framework asynchronous.

The application must not create an unnecessary large FreeRTOS scan task merely to wrap a framework operation that already has an asynchronous callback API.

Conceptually:

```text
BLE scan request
      |
      v
start framework async scan
      |
      v
BLE GAP advertisements
      |
      v
completion callback
      |
      v
main application processes results
      |
      v
bounded publication
      |
      v
BluetoothBleScanDone
```

## 12.4 BLE result fields

Each result can include:

- address
- name
- RSSI
- textual advertisement/service/manufacturer details
- QCY/T13 hint

`BLE_DETAIL_MAX = 240` bytes.

Maximum local BLE result table: 32 entries.

## 12.5 Known real hardware evidence

A successful BLE scan was observed on the target hardware and found:

```text
QCY T13 ANC2-APP
```

with a real BLE advertisement and manufacturer data.

This was an important milestone proving that the BLE scan path, controller, framework integration, and host event decoding were functioning in the repaired 2.3.11 runtime.

---

# 13. Bluetooth manager

`include/services/bluetooth/BluetoothManager.h`

Current role is intentionally thin:

- owns `BluetoothClassic`
- owns `BluetoothLE`
- delegates `begin()` to Classic
- calls both `update()` methods
- exposes a small post-update hook for lifecycle ordering

Do not turn `BluetoothManager` into a Bluetooth God Class.

Classic, BLE and A2DP must remain explicit subsystems.

---

# 14. A2DP architecture

Primary files:

- `include/services/a2dp/A2DPManager.h`
- `src/services/a2dp/A2DPManager.cpp`
- `include/services/a2dp/A2DPSource.h`
- `src/services/a2dp/A2DPSource.cpp`

Third-party dependency:

`ESP32-A2DP v1.8.10`

## 14.1 Responsibilities of A2DPManager

- connection state machine
- address connection
- name connection
- automatic reconnect
- cached MAC/name management
- media state handling
- connection timeout
- disconnect debounce
- test tone
- PCM audio streaming
- audio status
- callback statistics
- error/event publication

## 14.2 Bluetooth ownership boundary

Before starting A2DP, Classic Bluetooth is temporarily suspended:

```text
BluetoothClassic.suspendForA2dp()
        |
        v
A2DP source start
        |
        v
A2DP lifecycle
        |
        v
finish shutdown
        |
        v
BluetoothClassic.restoreAfterA2dp()
```

This is intentional. A2DP and Classic SPP cannot casually own the same resources independently.

## 14.3 A2DP connection modes

### By Classic MAC

Input must be a Classic Bluetooth MAC.

The address is retained through the A2DP adapter and passed to the ESP32-A2DP source path.

### By name

Preferred for headphones such as the QCY target when the Classic address is not known in advance.

The third-party library performs Classic inquiry/name selection.

### Automatic reconnect

Conceptually:

```text
cached Classic MAC exists?
        |
       yes
        v
try MAC path
        |
        +-- success -> Connected
        |
        +-- failure -> cached/default Classic name fallback

no cached MAC
        |
        v
cached/default name path
```

Current default target name in source:

```text
QCY T13 ANC2
```

## 14.4 A2DP async startup

The third-party A2DP library owns an internal Bluetooth/application task during startup and connection handling.

The project wraps this lifecycle so the protocol command response can be an acceptance response while connection completion arrives as asynchronous events.

Do not block the main application loop waiting for a full A2DP connection unless an explicit blocking contract is required.

## 14.5 A2DP states

Current conceptual states include:

```text
Disconnected
Connecting
Connected
Streaming
Disconnecting
```

In addition, asynchronous shutdown state and pending connection/fallback state exist in `A2DPManager`.

Connection and audio/media states are tracked separately.

A disconnect around media startup is debounced to prevent false disconnects caused by transient A2DP startup sequencing.

## 14.6 A2DP timing/config values

From `AppConfig.h`:

- connect timeout: 60 s
- media check delay: 300 ms
- media check retries: 2
- disconnect debounce: 1000 ms
- tone sample rate: 44100 Hz
- tone frequency: 1000 Hz
- tone frames: 441
- audio bits: 16
- audio block samples: 256
- audio header bytes: 3
- audio ring capacity: 28672 bytes
- prebuffer multiplier: 4

Supported PC audio sample rates in the current dispatcher/host path:

- 11025 Hz
- 22050 Hz
- 44100 Hz

Supported channels: 1 or 2.

Bits: 16 only.

---

# 15. PC audio path

High-level path:

```text
Windows system audio
        |
        v
WASAPI / host capture
        |
        v
Host raw PCM packetization
        |
        v
AUDIO_DATA (0x6D)
        |
        v
ESP32 protocol
        |
        v
A2DPManager::pushAudioData()
        |
        v
AudioStreamBuffer
        |
        v
A2DP frame callback
        |
        v
Bluetooth A2DP sink/headphones
```

Host supports:

- audio status
- start/stop
- WASAPI loopback device listing
- capture-path test
- loopback selection
- generated-tone self-test
- real Windows audio monitor
- audio profile configuration

The generated-tone test and the real Windows audio monitor are different tests. Do not conflate them.

---

# 16. Memory constraints

ESP32-WROOM-32 is memory-constrained, especially when Wi-Fi, Classic BT, BLE and A2DP resources overlap.

Always inspect:

- free internal heap
- largest free contiguous block
- stack requirements
- fixed queue sizes
- BLE allocations
- A2DP audio buffers
- String fragmentation
- temporary vectors/STL allocations

A recent real failure proved why this matters:

```text
Bluetooth Classic ready heap ~= 9 KB
largest free block ~= 1.9 KB
```

A scan worker requesting thousands of bytes of task stack failed even though total free heap looked non-zero.

Therefore:

**Do not solve low-memory Bluetooth failures by merely reducing task stack sizes repeatedly. Prefer framework-owned asynchronous APIs when available and avoid adding another RTOS task solely to wrap an already-asynchronous operation.**

This principle is now central to Bluetooth scan architecture.

---

# 17. Important migration history

## Before migration

The old Arduino firmware was a single large source file and was known to work.

Its working characteristics became the baseline.

## After PlatformIO modular migration

Many regressions appeared, including:

- Wi-Fi scan/connect failures
- Classic Bluetooth scan timeouts
- BLE scan timeouts
- A2DP automatic reconnect timeouts
- callback state corruption/races
- EventQueue overflow risk
- task stack allocation failures
- protocol/host output inconsistencies
- source/header mismatches causing linker failures

## Major root causes encountered

### 1. Refactor changed runtime behavior

The original behavior was preserved only partially.

### 2. Callback ownership regression

Custom callback registration could interfere with framework-owned Bluetooth callback behavior.

### 3. Oversized event queue

A permanently large fixed queue consumed significant internal DRAM.

### 4. Extra worker tasks

Wrapping framework async APIs in new tasks increased heap/stack pressure.

### 5. Lifecycle conflicts

Classic Bluetooth, BLE and A2DP all interact with the same ESP32 BTDM/Controller/Bluedroid resources.

### 6. Wrong framework API assumptions

APIs must be verified against the actual Arduino-ESP32 3.0.7 framework, not memory of another version.

### 7. Header/source drift

At one stage `allocateEntry()` existed in the header but its implementation had been lost, causing a linker-only failure. This is why compile and link must both be verified.

---

# 18. Known working hardware evidence from the project history

Important observations from real hardware tests:

### Wi-Fi

After the migration fixes, Wi-Fi scan/connect functionality was reported working again.

### BLE

A real BLE scan successfully found:

```text
QCY T13 ANC2-APP
```

Example observed result:

```text
84:ac:60:6e:1a:89
QCY T13 ANC2-APP
RSSI around -65 dBm
```

### QCY A2DP

The target headphones are:

```text
QCY T13 ANC2
```

Important distinction:

- `QCY T13 ANC2-APP` is a BLE advertisement/device name
- `QCY T13 ANC2` is the Classic/A2DP target name

Do not use a BLE advertisement address as if it were automatically the Classic A2DP MAC.

The historical working A2DP path included connection by Classic name and successful test-tone playback.

### Classic discovery regression and repair

The major Classic scan failure observed during migration looked like:

```text
Bluetooth Classic GAP discovery started...
then timeout
```

Later diagnostics proved the controller was initialized but application-created scan tasks could not be allocated because the largest contiguous heap block was too small.

The final repair removed the extra application-created scan task from the Classic/BLE scan architecture.

---

# 19. Host application

Primary host files:

- `host/main.py`
- `host/esp32.py`
- `host/frame.py`
- `host/serial_transport.py`
- PC audio helper modules

## 19.1 Host threading

`Esp32Device` uses:

- a serial transport
- a frame parser
- an RX thread
- response queue
- event queue
- request lock
- condition variable

One control request at a time is intentional.

The RX thread separates:

```text
TYPE_RESPONSE -> responses
TYPE_ERROR    -> responses
TYPE_EVENT    -> events
```

`EVT_SERIAL_LOG` is rendered immediately as an `[ESP, ...]` diagnostic.

## 19.2 Menu

Current host menu includes 31 operations:

1. Wi-Fi scan
2. Wi-Fi connect
3. Wi-Fi disconnect
4. Wi-Fi status
5. TCP client test
6. TCP server test
7. UDP test
8. system tests
9. Bluetooth info/init
10. Bluetooth Classic scan
11. BLE scan
12. Bluetooth Classic SPP connect
13. Bluetooth disconnect
14. A2DP status
15. A2DP connect by Classic MAC
16. A2DP connect by Classic name
17. A2DP automatic reconnect
18. A2DP tone ON
19. A2DP tone OFF
20. A2DP disconnect
21. clear A2DP cache
22. PC audio status
23. start PC audio -> A2DP
24. stop PC audio
25. list WASAPI loopback devices
26. test WASAPI capture
27. loopback selection
28. Windows generated-tone self-test
29. real Windows audio monitor
30. PC audio profile
31. exit

---

# 20. Host logging contract

Host uses structured prefixes such as:

```text
[PC, UI, <ms>] ...
[PC, Prompt, <ms>] ...
[PC, Result, <ms>] ...
[PC, Log, <ms>] ...
[PC, Error, <ms>] ...

[ESP, Log, <firmware timestamp> ms] ...
[ESP, Error, <firmware timestamp> ms] ...
```

Do not reintroduce free-form firmware `Serial.println()` output.

---

# 21. Critical state / ownership rules for Bluetooth

## Classic

Owner of Classic GAP/SPP behavior:

```text
BluetoothSerial / BluetoothClassic service
```

Do not create another GAP callback merely to collect scan results.

## BLE

Owner:

```text
BLEDevice / BLEScan / BluetoothLE service
```

Use the framework's callback/result model.

## A2DP

Owner:

```text
A2DPManager + A2DPSourceAdapter + ESP32-A2DP library
```

Callbacks update protected runtime state; application update publishes domain events.

## EventQueue

Owner:

```text
Application
```

Services publish. Application drains.

## Protocol

Owner:

```text
ProtocolCodec / CommandDispatcher
```

Services must not directly serialize protocol frames.

---

# 22. Error model

The project uses `ErrorCode` / `Result<T>` patterns instead of arbitrary `false`, `-1`, magic numbers, or hidden Serial messages.

Typical error concepts include:

- None
- InvalidArgument
- Busy
- NotConnected
- Timeout
- Unsupported
- HardwareError
- ScanFailed
- ConnectionFailed
- InternalError

`CommandDispatcher` maps internal errors to wire-level `StatusCode`.

Do not expose framework-specific error values directly as protocol semantics unless explicitly designed.

---

# 23. Important command semantics

## Bluetooth scan

`BluetoothScanStart (0x42)`:

- request acknowledged immediately
- scan happens asynchronously
- result events follow
- `BluetoothScanDone (0x51)` terminates the result stream

## BLE scan

`BluetoothBleScanStart (0x47)`:

- request acknowledged immediately
- framework async scan runs
- result events follow
- `BluetoothBleScanDone (0x58)` terminates the result stream

## A2DP connect

A2DP connect commands return acceptance rather than waiting for full connection.

Expected later events include connection/failure/media events.

## Audio data

`AudioData (0x6D)` is streaming traffic and intentionally has no normal response.

---

# 24. Current protocol payloads that matter most

## GET_INFO

The host expects a compact payload containing:

- device name length + ASCII name
- MAC bytes

The host converts the returned address representation into the usual colon-separated display form.

## Bluetooth GET_INFO

Current host expects:

```text
name_len
name
mac_len
mac
initialized
scanning
connected
```

## Bluetooth scan result

`0x50`:

```text
address_length:u8
address:ASCII
name_length:u8
name:UTF-8
rssi:int16 LE
```

`0x56` detail includes address/name plus COD-related metadata.

## BLE result

`0x57`:

```text
address_length
address
name_length
name
rssi:int8
detail_length:u16 LE
details:UTF-8
qcy_hint:u8
```

## A2DP audio status

Legacy wire payload is fixed at 36 bytes:

```text
streaming
primed
sample_rate:u32
channels
bits
used:u32
capacity:u32
received:u32
dropped:u32
underruns:u32
callback_count:u32
callback_bytes:u32
```

Do not append new fields to this legacy payload without an explicit protocol change.

---

# 25. Testing and verification strategy

Use four distinct levels:

```text
Level 1: Static/source verification
Level 2: Build and link verification
Level 3: Unit/integration verification
Level 4: Physical hardware runtime verification
```

Never report one level as proof of a later level.

The project has also used a mock environment to validate C++ compilation/linking when the actual PlatformIO package cache was unavailable. That environment is now committed under `test/mock/` and is rerunnable with `bash test/mock/run_mock_build.sh`.

Mock testing is useful for:

- declaration/definition mismatch
- undefined references
- missing methods
- header/API inconsistencies
- basic C++ compile failures

Mock testing does NOT prove:

- ESP32 ABI correctness
- actual framework linkage
- flash/RAM layout
- BTDM controller behavior
- hardware discovery
- A2DP RF/audio behavior

---

# 26. Regression checklist

At minimum re-test:

### System

- PING
- GET_VERSION
- GET_INFO
- RESET

### Wi-Fi

- scan
- connect
- disconnect
- status

### Network

- TCP client
- TCP server
- UDP

### Bluetooth Classic

- init/info
- scan
- scan stop
- connect by MAC
- disconnect
- status

### BLE

- scan
- scan stop
- advertisement decoding
- device details
- completion event

### A2DP

- status
- connect by MAC
- connect by name
- automatic reconnect
- cache
- tone ON/OFF
- disconnect

### PC audio

- audio start
- audio stop
- audio status
- real PCM streaming
- underrun behavior

---

# 27. Edge cases that matter

Always consider:

- scan while another scan is active
- scan while A2DP is active
- scan during A2DP connection
- connect while Bluetooth service is busy
- BLE scan after Classic scan
- Classic scan after BLE scan
- Classic connect after BLE scan
- BLE scan after A2DP disconnect
- A2DP connect after Classic scan
- repeated failed A2DP reconnects
- headphone disappears during connection
- device name found but Classic address differs from BLE address
- event queue pressure
- low internal heap
- heap fragmentation
- task allocation failure
- timeout without callback completion
- framework callback arrives after stop request
- duplicate completion event
- event publication outruns queue drain
- protocol response arrives late
- stale response/event from previous command

---

# 28. Common failure patterns seen in this project

## Symptom: host times out waiting for `0x42`

Check in this order:

1. Did the firmware return an immediate response for `0x42`?
2. Did `BluetoothClassic::scan()` return `Busy` / `HardwareError` / `ScanFailed`?
3. Is Classic controller state enabled?
4. Did GAP discovery actually start?
5. Is the framework-owned GAP callback still intact?
6. Is the largest contiguous internal heap block sufficient?
7. Is `BluetoothScanDone` ever published?
8. Is EventQueue full?
9. Is the host waiting for the wrong response/event ID?

Do not immediately increase host timeout.

## Symptom: BLE scan times out

Check:

1. `BLEDevice::getInitialized()`
2. controller/Bluedroid state
3. async scan start return value
4. scan completion callback
5. watchdog
6. result processing
7. EventQueue publication
8. `BT_BLE_SCAN_DONE`

## Symptom: linker undefined reference

Check exact declaration/definition parity:

- namespace
- const
- pointer/reference
- array type
- overload
- static/non-static
- header/source mismatch

A previous real failure was:

```text
BluetoothClassic::allocateEntry(unsigned char const*)
```

The implementation had accidentally disappeared while the declaration remained in the header.

---

# 29. What NOT to do

Do not:

- rewrite the whole subsystem because one command timed out
- increase host timeout instead of debugging the firmware path
- add a new RTOS task when the framework already provides an async API
- register a second Bluetooth GAP callback without proving callback ownership
- tear down/reinitialize BluetoothSerial after every scan without evidence
- add large fixed buffers to solve queue pressure
- increase EventQueue capacity without measuring DRAM
- assume BLE MAC == Classic A2DP MAC
- treat a command response as proof that an asynchronous operation completed
- write protocol frames from callbacks
- add raw Serial logs in arbitrary services
- change wire payloads to make debugging easier
- declare PASS without evidence
- trust stale docs over current source
- remove old functionality simply because the modular design looks cleaner
- silently modify framework/dependency versions

---

# 30. Rules for future AI modifications

The project contains a mandatory rule document under the project's rules/docs area. Its principles are mandatory.

Most important operational requirements:

1. Audit before modifying.
2. Read the whole project structure before a major refactor.
3. Preserve capability unless deletion is explicitly justified.
4. Preserve protocol command/event IDs and payloads by default.
5. Keep Transport separate from Protocol.
6. Keep Services separate from CLI/output formatting.
7. Keep Application orchestration above Services.
8. Use `ErrorCode` / `Result<T>` for errors.
9. Use explicit state/lifecycle ownership.
10. Treat callbacks as execution-context boundaries.
11. Keep main loop small.
12. Avoid blocking work in the main path unless bounded and justified.
13. Control ESP32 heap, stack, queues and fragmentation.
14. Verify actual framework APIs against the pinned version.
15. Compare migration behavior with the known-good Arduino baseline when debugging regressions.
16. Verify compile, link, runtime and hardware separately.
17. Never claim hardware verification without hardware evidence.
18. Update documentation when architecture changes.
19. Make changes traceable from requirement -> design -> file -> method -> test -> result.
20. Never hide an unresolved `UNKNOWN` / `NOT VERIFIED` state.

---

# 31. Change workflow for an AI

For any future non-trivial change, use this sequence:

```text
1. Read current source tree
2. Read applicable rules
3. Read relevant build config
4. Identify actual owner/lifecycle
5. Compare with Arduino 2.2.7 baseline if the change is a migrated subsystem
6. Identify command/event contracts
7. Identify memory/task/callback impact
8. Write or update a regression/static test first when practical
9. Make the smallest coherent change
10. Compile
11. Link
12. Run tests
13. Inspect diff
14. Check protocol compatibility
15. Check memory impact
16. Update docs
17. Commit
18. Report exact evidence and remaining unknowns
```

For Bluetooth/A2DP, add:

```text
19. Check controller/Bluedroid ownership
20. Check callback ownership
21. Check Classic/BLE/A2DP resource overlap
22. Check largest contiguous internal heap
23. Check async completion and cancellation behavior
```

---

# 32. Documentation/source discrepancies to remember

At the current snapshot there are places where older documentation may describe the former implementation rather than the latest source.

Example: an older architecture description mentioned a 65-record EventQueue and scan workers. Current source uses a 16-record queue and framework-owned asynchronous Bluetooth scan APIs.

When this happens:

```text
trust source
identify stale documentation
update documentation
```

Do not adapt the source backwards merely to make old documentation appear consistent.

---

# 33. Current known status

## Confirmed from repository/source

- firmware version: 2.3.12
- protocol version: 1
- PlatformIO target: `esp32dev`
- Arduino-ESP32 3.0.7
- ESP-IDF 5.1.4
- ESP32-A2DP v1.8.10
- Huge APP partition
- 921600 UART
- modular Application/Protocol/Services/Transport architecture
- fixed-size EventQueue with bounded scan publication
- structured firmware logs
- async Classic/BLE scan architecture without application-created scan worker tasks
- async A2DP connection lifecycle
- PC raw PCM audio path

## Confirmed by real hardware history

- Wi-Fi was restored after the migration fixes.
- BLE scan successfully discovered `QCY T13 ANC2-APP`.
- QCY T13 ANC2 was successfully used as an A2DP target in the earlier working A2DP flow.
- Bluetooth scan failures were ultimately traced through real heap/task evidence and repaired by removing unnecessary application-created scan workers and preserving framework-owned Bluetooth lifecycle behavior.

## Not to be assumed automatically

- A green mock build does not equal ESP32 hardware verification.
- A successful BLE scan does not prove Classic A2DP connection.
- A successful A2DP tone does not prove Windows real-audio streaming.
- A command response does not prove asynchronous completion.
- Documentation is not more authoritative than current source.

---

# 34. High-value file map

```text
platformio.ini
VERSION.txt
partitions/huge_app.csv

include/config/AppConfig.h
include/config/HardwareConfig.h
include/config/Version.h

include/core/Event.h
include/core/EventQueue.h
include/core/Result.h
include/core/Error.h
include/core/Interfaces.h

src/main.cpp
src/application/DongleApplication.cpp

include/protocol/
src/protocol/

include/services/bluetooth/
src/services/bluetooth/
    BluetoothClassic
    BluetoothLE
    BluetoothManager

include/services/a2dp/
src/services/a2dp/
    A2DPManager
    A2DPSource
    AudioStreamBuffer

include/services/wifi/
src/services/wifi/

include/services/network/
src/services/network/

include/services/system/
src/services/system/

include/transport/
src/transport/

host/
    main.py
    esp32.py
    frame.py
    serial_transport.py
    PC audio helpers

docs/
test/
scripts/
```

---

# 35. First questions an AI should ask itself before changing code

```text
What is the actual current implementation?
Who owns this resource?
Is this operation synchronous or asynchronous?
Does the framework already provide an async API?
What callback owns this event?
Which task/core executes this code?
Can this operation allocate internal DRAM?
What is the largest contiguous free heap block?
Will this change alter command/event IDs or payloads?
Does the Arduino 2.2.7 baseline already solve this problem differently?
Which regression test proves the change?
What remains NOT VERIFIED?
```

If those answers are not known, audit first.

---

# 36. One-paragraph AI handoff

ESP32 Wireless Dongle is a PlatformIO/Arduino C++ modular refactor of a previously working single-file Arduino ESP32-WROOM-32 firmware. The target is ESP32 DevKit V1 / `esp32dev`, 4 MB flash, Huge APP partition, 921600 UART, Arduino-ESP32 3.0.7 / ESP-IDF 5.1.4 via pioarduino 51.03.07, and ESP32-A2DP v1.8.10. Architecture is Application -> Protocol -> Services -> hardware/framework, with Transport independent from Protocol and domain events flowing through a fixed EventQueue to the application before wire serialization. The protocol is version 1 with stable command/event IDs; asynchronous commands return acceptance and complete through events. Bluetooth Classic, BLE and A2DP share the ESP32 Bluetooth controller/BTDM resources, so callback ownership, lifecycle, stack/heap pressure and async completion are critical. Current Classic and BLE scanning are framework-owned asynchronous operations without extra application-created scan worker tasks; this was required after real hardware showed Classic task creation failures with only ~1.9 KB of largest contiguous internal heap while BLE was active. The target headphone is QCY T13 ANC2; its BLE advertisement name `QCY T13 ANC2-APP` must not be confused with its Classic/A2DP target name/address. The project requires audit-first changes, backward behavior preservation, protocol compatibility, centralized structured logging, explicit ownership/state handling, version-pinned framework API verification, multi-level verification, and honest separation of static, build, runtime, and hardware evidence.

---

# 37. End of context

When starting work from this file, immediately read the current repository source and the mandatory project rules document. Use this file to understand why the architecture looks the way it does, then verify all claims against the current checkout before modifying anything.
