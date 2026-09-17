# Build

## Target

- Board: `esp32dev` (ESP32-WROOM-32 / DevKit V1)
- Framework: Arduino
- Arduino-ESP32: 3.0.7 / ESP-IDF 5.1.4
- Platform source: pioarduino `51.03.07` (pinned ZIP URL in `platformio.ini`)
- Flash: 4 MB, QIO, 80 MHz
- Partition: `partitions/huge_app.csv`
- Serial: 921600 baud
- A2DP: `ESP32-A2DP v1.8.10`

## Clean build

```powershell
Remove-Item -Recurse -Force .pio -ErrorAction SilentlyContinue
pio run
pio run -t upload --upload-port COM5
pio device monitor -p COM5 -b 921600
```

## Compatibility note

`ESP32-A2DP v1.8.11` is intentionally not used. It references `ESP_A2D_AUDIO_STATE_SUSPEND`, which is unavailable in the user's Arduino-ESP32 3.0.7 / ESP-IDF 5.1 environment.

## Verification status

Static architecture tests and Python syntax checks are run before packaging. A full PlatformIO compile/link is not executed in this packaging environment because the PlatformIO toolchain/package cache is unavailable here. Hardware regression testing must therefore be performed in the user's PlatformIO environment.

## Runtime note
A2DP source startup is asynchronous in the modular architecture; the protocol acknowledgement is returned without blocking the main application loop.
