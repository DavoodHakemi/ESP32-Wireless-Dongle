# Dependencies

| Dependency | Version | Purpose |
|---|---|---|
| pioarduino Espressif32 platform | 51.03.07 | reproducible PlatformIO platform |
| Arduino-ESP32 | 3.0.7 / ESP-IDF 5.1.4 | framework |
| ESP32-A2DP | 1.8.10 | Bluetooth A2DP Source |
| BluetoothSerial | framework-provided | Classic SPP |
| BLEDevice/BLEScan | framework-provided | BLE scanning |
| WiFi | framework-provided | Wi-Fi/TCP/UDP |
| Preferences | framework-provided | cached A2DP target |

`ESP32-A2DP` is intentionally pinned to **v1.8.10**. v1.8.11 was found incompatible with the actual Arduino-ESP32 3.0.7 environment because it references `ESP_A2D_AUDIO_STATE_SUSPEND`, which is absent there.

The `AudioTools` warning emitted by unused A2DP sink translation units is non-fatal; the project uses A2DP Source only and does not add AudioTools merely to suppress that warning.
