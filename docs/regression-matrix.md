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


## 2.3.10 migration corrections

| Regression source | Correction | Compatibility impact |
|---|---|---|
| Cold-start Classic/BLE initialization delayed command response | Deferred `StartPending` state | Wire IDs unchanged |
| Wi-Fi scan burst could fill the fixed EventQueue before completion | Eight-result publication batches + per-service event drains | Event ordering preserved |
| Service event bursts could remain queued across phases | Application drains after Wi-Fi, Network, Bluetooth and A2DP updates | No wire-format change |
| A2DP name-selector callback reused one flag for two contexts | Separate callback-pending and application-event-pending flags | No wire-format change |
| Callback-owned A2DP playback cursor was writable by the application task | Protected reset request consumed by callback context | Audio payload format unchanged |
