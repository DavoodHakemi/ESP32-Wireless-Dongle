# ESP32 Wireless Dongle — Verification Report 2.3.10

## Scope

This report covers the migration audit against the Arduino 2.2.7 baseline and the reliability corrections applied to the PlatformIO snapshot.

## Evidence levels

| Area | Status | Evidence |
|---|---|---|
| Project/source audit | PASS | Current source tree extracted and inspected; command/event registry compared with Arduino 2.2.7 |
| Wire command compatibility | PASS | 45/45 legacy command IDs preserved |
| Legacy wire event compatibility | PASS | 26/26 legacy event IDs preserved; `SerialLog (0x78)` remains additive |
| Python/static regression tests | PASS | `pytest -q`: 44 passed |
| Python bytecode compilation | PASS | `python -m compileall -q host test` |
| C++ syntax verification | PASS | All current `src/*.cpp` passed `g++ -fsyntax-only` using local Arduino/ESP32/A2DP API stubs |
| PlatformIO full build | NOT VERIFIED | `pio`/pioarduino packages are not installed in the current environment and package download is unavailable |
| Hardware runtime | NOT VERIFIED | Physical ESP32/WROOM-32 execution was not available in this session |

## Corrections

1. Cold-start Bluetooth Classic and BLE scan initialization is deferred into service update after the command response.
2. Wi-Fi scan results are published in bounded batches to preserve `WIFI_SCAN_DONE` ordering with a fixed 65-record EventQueue.
3. Application-domain events are drained after each service phase.
4. A2DP Classic-device callback state is separated from application notification state.
5. Callback-owned A2DP playback state is reset through an explicit protected request rather than direct cross-context mutation.
6. Host firmware-version validation reads `VERSION.txt` as the source of truth.

## Known unverified items

- A complete PlatformIO compile/link/size report on the target Arduino-ESP32 3.0.7 / ESP-IDF 5.1.x environment.
- End-to-end hardware tests for Wi-Fi, Classic GAP, BLE, A2DP, PC Audio, TCP and UDP.

## Final status

**PARTIAL** — static/source verification is complete for the changes in this audit; build and hardware verification remain explicitly unverified.
