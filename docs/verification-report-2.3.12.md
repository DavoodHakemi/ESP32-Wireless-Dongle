# ESP32 Wireless Dongle - Verification Report 2.3.12

## Scope

Repair pass over the repository state left after the realtime-audio UART RX work: resolve regression-test drift, restore level 2 build verification, and repair two firmware compile defects that had silently entered master.

## Change classification

- Type: TYPE A - Bug Fix (two compile defects) + TYPE C - Test Alignment (two drifted regression tests)
- Scope: CROSS-LAYER
- Protected interfaces: wire command IDs, event IDs, payload layouts, service ownership and update ordering — all unchanged

## Findings fixed

1. **A2DP address path did not compile against the pinned library.** Commit `d7b782d` removed the `const_cast<uint8_t*>(address)` from `A2DPSourceAdapter::startByAddress` while the ESP32-A2DP v1.8.10 signature is `set_auto_reconnect(esp_bd_addr_t addr, int max_retries)` with `esp_bd_addr_t` = `uint8_t[6]` (mutable). Passing the caller's `const uint8_t address[6]` directly terminates compilation. Fix: copy the address into a local `uint8_t targetAddress[6]` and pass the copy; the library only reads the value. No behavioral change; verified by mock compile and real PlatformIO build.
2. **Invalid `<ESP.h>` include in `SystemService.cpp`.** The arduino-esp32 3.0.7 core contains `Esp.h` (included by `Arduino.h`) and has no top-level `ESP.h`, so the real toolchain stopped with "compilation terminated" at this translation unit. Fix: include `<Arduino.h>` instead.
3. **Stale audio regression test.** `test_audio_transport_uses_low_jitter_capture_settings` still pinned `CAPTURE_RECORDER_BLOCKSIZE = 1024`, contradicting the intentional, CHANGELOG-documented reduction to 256 frames and the newer `test_low_latency_capture_quantum_matches_audio_read_quantum`. Test updated to pin 256 with a rationale comment.
4. **A2DP test asserted an uncommitted refactor.** `test_a2dp_mac_path_restores_arduino_selector_semantics` (renamed `test_a2dp_address_path_preserves_library_reconnect_semantics`) asserted source strings (`self->_remoteAddress.length() == 17`, memcmp-based selection, `(void)retries;`) that no committed source revision ever contained — a refactor spec that was never implemented. Per the project source-of-truth rules the test now pins the preserved, hardware-verified semantics: library-owned reconnect with an explicit retry budget, address copy for the mutable library buffer, name-path reconnect disable, retained discovered Classic address for the cache, and the cached-MAC-first automatic-reconnect flow with name fallback. Status of the asserted selector refactor: NOT IMPLEMENTED (documented, not silently reconciled).
5. **Committed mock verification harness (`test/mock/`).** The 2.3.11 verification session used an ad-hoc stub environment that was never stored in the repository. The harness is now a committed, rerunnable artifact (`bash test/mock/run_mock_build.sh`): mock Arduino/ESP32/FreeRTOS/WiFi/BLE/BluetoothSerial/ESP32-A2DP headers, mock runtime, mock entry point, runner script. It reproduced both compile findings (1) and (2) before the real build confirmed them, proving its regression value.

## Protocol compatibility

No command IDs, event IDs, command payloads, response payloads or event payloads were changed. No service update ordering, ownership or lifecycle behavior was changed.

## Verification

| Check | Status | Evidence |
|---|---|---|
| Python regression tests | PASS | `python -m pytest -q`: 55 passed |
| Host Python bytecode compilation | PASS | `python -m compileall -q host test` |
| Mock C++ compile | PASS | All 16 src/*.cpp compiled with mock Arduino/ESP32/FreeRTOS/BLE/A2DP stubs under `-Wall -Wextra -Werror` |
| Mock C++ link + boot | PASS | Objects linked to `build/firmware_mock.elf`; mock entry ran `setup()` + 3 `loop()` iterations |
| PlatformIO full build/link/image | PASS | `pio run -e esp32dev` SUCCESS on pioarduino 51.03.07: RAM 33.2% (108796 B), Flash 42.0% (1763525 B), esp32 image created with esptool v4.8.1.1 |
| Hardware runtime | NOT VERIFIED | No ESP32 hardware attached to this environment; flashing and end-to-end hardware regression remain with the owner |

## Hardware evidence

None produced in this session (no hardware attached). The two compile fixes restore the tree's ability to produce the same firmware the owner previously hardware-tested; runtime equivalence is argued from the no-behavior-change property of the fixes, not from hardware evidence.

## Documentation updates

- `CHANGELOG.md`: added the 2.3.12 section.
- `VERSION.txt` + `include/config/Version.h`: firmware version 2.3.11 → 2.3.12 (startup log uses the version macro, so it follows automatically).
- `docs/ai_project_context.md`: snapshot version references and the startup-log description updated to stay current; snapshot commit block remains historically dated.
- `.gitignore`: added the mock build output directory (`build/`).

## Final status

**PARTIAL** - static, mock and full PlatformIO build/link verification are complete, and the Python regression suite is green. Hardware runtime verification remains with the owner as required by the project regression checklist.
