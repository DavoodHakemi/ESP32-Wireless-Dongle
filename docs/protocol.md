# Protocol

Wire protocol version: **1**. Firmware semantic version: **2.3.11**. These versions are intentionally independent.

Frame:

`AA 55 | version | type | command/event | sequence | payload_length LE | payload | CRC16-CCITT-FALSE LE`

Responses begin with one status byte. Existing command/event IDs and payload layouts are preserved from the pre-refactor firmware.

## Logging event

`EVT_SERIAL_LOG (0x78)` payload:

`timestamp_ms:u32 LE | type_len:u8 | type:ASCII | message:UTF-8`

The host renders it as `[ESP, Type, <timestamp> ms] message`; host-side diagnostics use `[PC, Type, <elapsed> ms] message`.

## Audio status

The wire payload remains the legacy 36-byte layout:

`streaming, primed, sample_rate:u32, channels, bits, used:u32, capacity:u32, received:u32, dropped:u32, underruns:u32, callback_count:u32, callback_bytes:u32`.

Additional A2DP callback timing diagnostics are internal/log-only and are not appended to the legacy wire payload.


## Reliability rules

- Every request uses a sequence number and is correlated with a response carrying the same sequence and command ID.
- Events always use sequence `0` and are not valid substitutes for command responses.
- Responses always place `StatusCode` in payload byte `0`; the remaining bytes are command-specific response data.
- `AUDIO_DATA (0x6D)` is intentionally one-way to preserve the legacy PC-audio streaming behavior.
- Host control requests are serialized; asynchronous notifications are handled independently.
- Bootstrap commands are idempotent and may be retransmitted after timeout using the same sequence number.


The machine-readable protocol registry is `docs/protocol-schema.json`. Tests compare the firmware enum assignments and host ID registry against it.


## Command execution classes

Control commands are classified by their wire-level completion semantics:

- **Immediate** commands complete their work before the response is sent.
- **Accepted/Asynchronous** commands return `Success` as an acceptance acknowledgement and continue through the service state machine; completion/failure is reported through events. This includes Wi-Fi scan/connect, Bluetooth Classic scan, BLE scan, and A2DP connect-by-address, connect-by-name, and automatic reconnect.
- For cold-start Bluetooth Classic/BLE scans, the acceptance response is emitted before the first-use framework stack initialization; initialization/start failure is reported through the corresponding scan-completion event and structured ESP log.
- Wi-Fi scan results are published in bounded batches so the fixed domain EventQueue is drained between application service phases and `WIFI_SCAN_DONE` remains ordered after the final result.
- **Blocking request** commands keep one request/response transaction open while waiting for an explicitly requested timeout, such as TCP/UDP receive.

The host uses one outstanding control transaction at a time. An event is never consumed as a response.

## Command transaction model

Control commands use one request/one response transactions. A response is correlated by both `sequence` and `command/event` ID. Events are asynchronous notifications and use sequence `0`; they are never consumed as command responses. Commands that initiate asynchronous work return `Success` for acceptance and report completion/failure through events. `AUDIO_DATA (0x6D)` remains the intentionally response-less streaming command.

## Protocol reliability contract

A response is an acknowledgement of the command transaction, not necessarily completion of the underlying subsystem operation. A2DP connection commands are accepted asynchronously: the response confirms acceptance and subsequent `A2DP_*` events report connection/media completion or failure.

Blocking network receive commands remain synchronous and use the timeout encoded in their request payload. `AUDIO_DATA` remains the only response-less command.
