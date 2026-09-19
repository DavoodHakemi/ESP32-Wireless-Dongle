# Changelog

## 2.3.12

- Fixed a firmware compile regression in the A2DP address path: the pinned ESP32-A2DP v1.8.10 `set_auto_reconnect(esp_bd_addr_t, int)` signature takes a mutable address buffer, so `A2DPSourceAdapter::startByAddress` now passes a local copy of the caller's const address instead of relying on the `const_cast` that was silently removed on 2026-09-17. The removed cast had left the master tree uncompilable against the pinned library.
- Replaced the invalid `<ESP.h>` include in `SystemService` with `<Arduino.h>`; the arduino-esp32 3.0.7 core exposes `Esp.h` through `Arduino.h` and has no top-level `ESP.h` header, so the previous include terminated compilation of the real build.
- Aligned drifted regression tests with preserved behavior: the WASAPI recorder quantum assertion now pins the intentional 256-frame value documented in the 2.3.11 entries, and the former "Arduino selector" A2DP test was restored to pin the hardware-verified library-reconnect semantics instead of a refactor that was never committed to source.
- Added `test/mock/`: a committed mock compile/link harness (Arduino/ESP32/FreeRTOS/BLE/A2DP stub headers, mock runtime, mock entry point and runner script) so verification level 2 remains available when the PlatformIO platform cache is unavailable. The harness reproduced both compile findings above before the real build confirmed them.
- Verification evidence: full PlatformIO build/link/image creation passed on pioarduino 51.03.07 / arduino-esp32 3.0.7 / ESP-IDF 5.1.4 / ESP32-A2DP v1.8.10 (RAM 33.2%, Flash 42.0%); mock compile/link/boot passed under `-Wall -Wextra -Werror`; the complete Python regression suite (55 tests) passed.
- Rebased the 2.3.12 fix set onto the A2DP rate-drift work (`Fix A2DP PCM rate drift`): the two compile fixes were still required because the rate-drift commit was authored on the pre-fix base; real PlatformIO build, mock harness and the full 61-test regression suite were re-run green on the integrated tree (evidence in `docs/verification-report-2.3.12.md`, re-verification section).
- Aligned `test_audio_ring_capacity_keeps_prebuffer_margin` with the intentional prebuffer raise from 4 to 8 blocks, keeping the ring-capacity margin invariant it actually guards.

## 2.3.11

- Resolved A2DP PCM rate drift by measuring the actual source callback cadence and phase-converting 22.05 kHz PCM to the measured A2DP output rate; increased audio prebuffer to 8 blocks for scheduler jitter tolerance.
- PC audio now probes the selected WASAPI loopback at startup and can switch to an active loopback when the default endpoint is silent.


- Reworked realtime audio UART writes to bypass per-frame flush while retaining serialized writes for control/audio ordering; control traffic keeps the existing flushed path.
- Reduced the WASAPI recorder quantum from 1024 to 256 frames so the capture buffer no longer adds an unnecessary ~17.4 ms quantum at 44.1 kHz.


- Stabilized PC audio transport timing by capturing at 44.1 kHz with 256-frame reads, pacing 256-sample UART audio blocks to their real-time playback interval, and bounding the host audio queue to prevent multi-second latency accumulation.
- Preserved stereo packetization for non-mono profiles and added host-side queue/send timing diagnostics for audio transport verification.
- Added a dedicated streaming resampler module and regression coverage for the exact 44.1 kHz -> 22.05 kHz block cadence.


- Reworked Classic Bluetooth discovery to start GAP discovery directly while leaving BluetoothSerial as the sole GAP callback owner and scan-result collector; no second callback or scan task is created.
- Removed post-scan BluetoothSerial teardown/reinitialization because the framework-owned discovery path preserves the SPP/GAP lifecycle.
- Restored post-event lifecycle ordering without changing the wire protocol or command/event IDs.
- Fixed BLE scan publication completion so `BT_BLE_SCAN_DONE` is emitted once after all bounded publication batches are drained.
- Added bounded BLE scan watchdog diagnostics and exposed existing firmware SerialLog events during host-side Bluetooth scan tests.
- Preserved A2DP direct-MAC selection, name fallback ownership and all existing Bluetooth, BLE and A2DP wire command/event IDs.
- Removed the Classic discovery worker task after hardware evidence showed that even a 3072-byte task could not be allocated while BLE resources were active.
- Reworked BLE scanning to use the Arduino-ESP32 asynchronous `BLEScan::start(duration, callback, false)` API without a dedicated scan task.


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
