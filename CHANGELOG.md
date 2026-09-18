# Changelog

## 2.3.10

- Restored the asynchronous command boundary for cold-start Bluetooth Classic and BLE scans so first-use stack initialization cannot delay command acknowledgements.
- Bounded Wi-Fi scan event publication to fixed-size batches so `WIFI_SCAN_DONE` cannot be lost when many access points are discovered.
- Drained domain events between service updates to preserve the legacy processing order and reduce queue burst pressure.
- Split the A2DP Classic-device discovery callback flag from the application notification flag to eliminate state races during deferred callback processing.
- Host firmware-version validation now reads `VERSION.txt` instead of maintaining a second hard-coded version string.

## 2.3.9

- Restored `BluetoothA2DPSource::start()` to the main application loop; the A2DP library already owns its internal FreeRTOS worker task.

- Hardened timeout cleanup so A2DP is stopped and Bluetooth Classic is restored before the connection-failure event is emitted.

- Added runtime diagnostics for cached A2DP target selection and source-start mode.


## 2.3.8

- Made A2DP source startup asynchronous using a one-shot FreeRTOS task.
- Prevented the A2DP library startup delay from blocking the application command loop.
- Preserved address/name reconnect behavior and cancellation handling.
- Kept the existing protocol response timing so A2DP connect commands can acknowledge immediately.

## 2.3.5
- Hold queued asynchronous events until the initial GET_INFO protocol handshake completes.
- Prevent startup log/event traffic from racing with initial request/response exchange.

## 2.3.5
- Reduced the fixed EventQueue capacity from 72 to 32 records to fit ESP32-WROOM-32 DRAM without changing event payload limits or protocol behavior.
- Retained the 336-byte event payload size for BLE detail events.

## 2.3.5
- Added a fixed-size thread-safe domain EventQueue between service/callback producers and ProtocolCodec transport output.
- Prevented asynchronous Logger/Bluetooth/A2DP events from interleaving with command response frames.
- Preserved event payloads by copying them into queue-owned storage before the source stack frame returns.
- Standardized firmware version metadata to 2.3.5.

# 2.3.5 — Arduino-ESP32 3.0.7 compile compatibility

- Corrected the A2DP ESP-IDF header include to `esp_a2dp_api.h`.
- Aligned BluetoothClassic const-qualification with the non-const Arduino-ESP32 3.0.7 BluetoothSerial API.
- Replaced deprecated volatile increment/compound-assignment operations in the A2DP callback statistics with relaxed GCC atomic builtins.
- Kept the ESP32-A2DP dependency pinned to v1.8.10 for the project’s Arduino-ESP32 3.0.7 / ESP-IDF 5.1.x environment.

# 2.3.1 — Modular Architecture Refactor

- Replaced the monolithic firmware implementation with layered Application / Protocol / Transport / Service / Utility modules.
- Added `ITransport`, domain `Result`/`ErrorCode`, domain events, typed subsystem states and centralized protocol-backed logging.
- Preserved legacy wire command/event IDs and the 36-byte `AUDIO_STATUS` response layout.
- Preserved ESP32-WROOM-32 / Arduino-ESP32 3.0.7 compatibility and pinned ESP32-A2DP v1.8.10.
- Kept A2DP callbacks allocation-free and isolated from protocol/transport layers.
- Added architecture, protocol, state-machine, dependency and regression documentation.

## 2.3.1 - Compile correctness follow-up

- Fixed `FrameType` qualification in `CommandParser.cpp`.
- Fixed const-correctness against Arduino Preferences and ESP32-A2DP v1.8.10 APIs.
- Fixed an extra closing brace in `A2DPManager::update()`.
- Fixed `BluetoothClassic::begin()` return type.
- Reworked AudioStreamBuffer counters/underrun flag access under the existing critical-section guard to remove volatile deprecation warnings.
