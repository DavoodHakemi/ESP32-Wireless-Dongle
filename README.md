# ESP32 Wireless Dongle — Modular Architecture 2.3.9

This package is a full architectural refactor of the ESP32 Wireless Dongle for ESP32-WROOM-32 / ESP32 DevKit V1 under PlatformIO.

## Preserved capabilities

PING, GET_VERSION, GET_INFO, RESET, fixed 921600 baud, Wi-Fi scan/connect/disconnect/status, TCP client/server, UDP, Bluetooth Classic info/scan/SPP connect/disconnect, BLE scan, A2DP MAC/name/automatic reconnect/cache/status, A2DP test tone, A2DP disconnect, and PC raw PCM audio streaming.

## Architecture

```text
DongleApplication
      |
      +---- Protocol: CommandParser -> Dispatcher -> ProtocolCodec
      |
      +---- Services: WiFi / Network / Bluetooth Classic / BLE / A2DP / System
      |
      +---- Transport: ITransport -> SerialTransport
      |
      +---- Arduino / ESP-IDF / ESP32-A2DP
```

Services communicate upward through domain events and results. Protocol and transport are deliberately independent.

## Compatibility

- ESP32-WROOM-32 / ESP32 DevKit V1
- 4 MB flash, Huge APP partition
- Arduino-ESP32 3.0.7 / ESP-IDF 5.1.4 via pioarduino 51.03.07
- ESP32-A2DP v1.8.10
- UART0 via CP2102 at 921600 baud

## Build

See `docs/BUILD.md`. The environment used for this package does not contain the PlatformIO platform cache, so hardware compilation must be performed in the user's PlatformIO environment.

## Host logs

ESP32 events use `[ESP, Type, xxxx ms]`. Host-side diagnostics use `[PC, Type, xxxx ms]`.
