# ESP32 Wireless Dongle - Verification Report 2.3.11

## Scope

This bug-fix audit reviews the Bluetooth subsystem from the command boundary through Classic GAP, BLE scanning, A2DP selection and lifecycle management. The Arduino 2.2.7 firmware is the behavioral baseline where current source behavior can be established from the original implementation.

## Change classification

- Type: TYPE A - Bug Fix
- Scope: CROSS-LAYER
- Protected interfaces: wire command IDs, event IDs, payload layouts and existing service ownership

## Findings fixed

1. Classic Bluetooth discovery now starts GAP inquiry directly without registering a second GAP callback. The framework-owned BluetoothSerial callback remains responsible for its scan-result set.
2. The scan completion path no longer tears down and recreates BluetoothSerial, avoiding unnecessary Bluetooth controller lifecycle churn before later BLE/SPP operations.
3. BLE scan publication now closes its publish state after the final bounded batch, emits BT_BLE_SCAN_DONE once, and includes a bounded watchdog for framework completion.
4. Host Bluetooth scan loops display the existing firmware SerialLog events using the project PC/ESP console contract.
5. EventQueue capacity and scan publication remain bounded; no wire command/event ID or payload layout was intentionally changed.

## Protocol compatibility

No command IDs, event IDs, command payloads, response payloads or event payloads were intentionally changed.

## Verification

| Check | Status | Evidence |
|---|---|---|
| Python regression tests | PASS | python -m pytest -q: 49 passed |
| Host Python bytecode compilation | PASS | python -m compileall -q host test |
| Mock C++ compile | PASS | All src/*.cpp compiled with Arduino/ESP32/FreeRTOS/BLE/A2DP stubs under -Wall -Wextra -Werror |
| Mock C++ link | PASS | All firmware objects plus runtime stubs linked to firmware_mock.elf |
| PlatformIO full build/link | NOT VERIFIED | PlatformIO is not installed in this environment |
| Hardware runtime | PARTIAL | Physical ESP32/WROOM-32 testing was performed for Bluetooth scans in the working 2.3.11 snapshot |

## Hardware evidence

- BLE scan completed successfully and discovered QCY T13 ANC2-APP.
- Classic scan no longer allocates an application-created scan worker task or performs post-scan BluetoothSerial teardown.
- Remaining Bluetooth runtime validation is documented by the project regression rules.

## Final status

**PARTIAL** - static/mock verification and targeted Bluetooth hardware verification are complete. Full PlatformIO build/link and complete end-to-end hardware regression remain outside this repository-session verification.
