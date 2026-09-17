# Protocol

Wire protocol version: **1**. Firmware semantic version: **2.3.2**. These versions are intentionally independent.

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
