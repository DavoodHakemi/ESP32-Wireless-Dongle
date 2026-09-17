# Regression Matrix

| Area | Legacy capability | Refactored implementation |
|---|---|---|
| Core | PING, GET_VERSION, GET_INFO, RESET, SET_BAUD | `SystemService` + `CommandDispatcher` |
| Wi-Fi | scan/connect/disconnect/status | `WiFiManager` |
| TCP | client/server connect, send, receive, close | `NetworkService` |
| UDP | open/send/receive/close | `NetworkService` |
| Bluetooth Classic | info/scan/SPP connect/disconnect | `BluetoothClassic` |
| BLE | scan/start/stop/results | `BluetoothLE` |
| A2DP | status/MAC/name/auto/cache/tone/disconnect | `A2DPManager` + `A2DPSourceAdapter` |
| Audio | start/stop/status/raw PCM blocks | `AudioStreamBuffer` + `A2DPManager` |
| Events | wireless state changes + ESP logs | domain `Event` -> application wire mapping |
| Transport | UART/CP2102 at 921600 | `SerialTransport` behind `ITransport` |

Legacy wire command/event numeric IDs and response layouts are retained. `AUDIO_STATUS` remains 36 bytes; callback timing diagnostics are log/internal only.
