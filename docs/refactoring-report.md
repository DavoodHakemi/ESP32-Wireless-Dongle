# Refactoring Report

## Audit of the pre-refactor tree

The baseline firmware concentrated approximately 4,836 lines in `src/main.cpp`. Protocol framing/CRC, command handling, Wi-Fi, TCP/UDP, Bluetooth Classic, BLE, A2DP lifecycle, audio ring-buffering, callbacks, logging and global state were all co-located. The host also had large monolithic `main.py` and `esp32.py` files.

## New structure

- `application/`: composition root and lifecycle.
- `protocol/`: wire protocol, command parser, codec and dispatcher.
- `transport/`: byte transport interfaces and UART implementation.
- `services/wifi/`: asynchronous Wi-Fi scan/connect/status.
- `services/network/`: TCP client/server and UDP.
- `services/bluetooth/`: Classic SPP and BLE scan services.
- `services/a2dp/`: A2DP adapter, connection state, tone, PC PCM buffer and media lifecycle.
- `services/system/`: version/device information/reset/baud compatibility.
- `core/`: errors, results, strong types, domain events and interfaces.
- `utils/`: centralized protocol-backed logger.

## Key changes

### Globals -> object ownership
Protocol parser buffers, Wi-Fi state, network sockets, Bluetooth scan tables, A2DP state and audio ring-buffer state are now members of the owning class. The sole callback-routing static is documented in the A2DP boundary.

### Protocol/transport separation
The parser consumes bytes and produces `ProtocolCommand`. The codec emits wire frames through `ITransport`. No service knows that UART0/CP2102 exists.

### Domain events
Services publish `core::Event` values. `DongleApplication` maps them to the unchanged wire event IDs. This prevents services from depending on protocol constants.

### State machines
A2DP has a typed `ConnectionState`. Bluetooth Classic/BLE scanner state is also encapsulated. A2DP connection and media/audio state are represented separately because the ESP32-A2DP lifecycle can report them at different times.

### Callback discipline
No realtime A2DP callback performs logging, UART writes, allocation or protocol publication. The callback only copies PCM and updates fixed-width counters/flags.

### Memory discipline
Large buffers are fixed-size. BLE entries are reset by assignment, not `memset`, because they contain Arduino `String` objects. No recurring heap allocation is introduced in the A2DP callback.

## Compatibility and risks

Numeric wire commands/events remain unchanged. `AUDIO_STATUS` remains the legacy 36-byte wire layout. Callback timing diagnostics (`callbackCount`, `callbackBytes`, `maxGapMs`, `stalls`) remain service-internal so existing hosts remain compatible.

The project uses the previously validated Arduino-ESP32 3.0.7 + ESP32-A2DP v1.8.10 combination. A full PlatformIO build cannot be executed in this environment because the PlatformIO/pioarduino packages are not installed locally and package download is unavailable. Structural, Python, protocol-reference and static architecture tests are executed locally before packaging.

## 2.3.1 Compile-correctness follow-up

The first modular package exposed four integration defects when compiled against the actual Arduino-ESP32 3.0.7 / ESP-IDF 5.1.4 toolchain:

- `FrameType` was referenced by `CommandParser` without an explicit frame-definition dependency. It is now defined in `protocol/ProtocolFrame.h` and included where needed.
- `A2DPSourceAdapter::discoveryActive()` was incorrectly declared `const` while the ESP32-A2DP v1.8.10 API is non-const. The adapter now follows the third-party API's qualifier.
- `A2DPManager::cachedMac()` and `cachedName()` were incorrectly declared `const` while Arduino `Preferences::getString()` is non-const. They now reflect the actual API contract.
- `BluetoothClassic::begin()` was declared `bool` while its implementation returns `Result<void>`. The declaration/definition now use `Result<void>`.
- `A2DPManager::update()` had an extra closing brace introduced during minification of the initial modular package. The implementation was rewritten in readable multi-line form and brace-balanced.
- Audio buffer counters and underrun flag access now use the existing FreeRTOS critical section instead of volatile increments, removing the compiler's volatile deprecation warnings while keeping the ISR/task boundary explicit.

This package deliberately remains pinned to ESP32-A2DP v1.8.10 because that is the compatibility point established by the actual Arduino-ESP32 3.0.7 environment.


## 2.3.10 Reliability follow-up

The modular implementation is now explicitly aligned with the asynchronous behavior of the Arduino 2.2.7 baseline at subsystem boundaries:

- Wi-Fi scan result publication is bounded per application update so the fixed EventQueue cannot overflow before `WIFI_SCAN_DONE` is emitted.
- Cold-start Bluetooth Classic/BLE initialization is deferred from the command dispatch path into the service update phase, preserving the immediate response contract.
- Application event delivery is drained between service updates, restoring the legacy Wi-Fi → Network → Bluetooth → A2DP processing order more closely.
- A2DP Classic-device discovery callback state and application notification state are separate flags.
