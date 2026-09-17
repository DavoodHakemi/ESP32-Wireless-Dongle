# State Machines

## A2DP

```text
Disconnected
    |
    v
Connecting ----timeout/error----> Error
    |
    v
Connected <------ media stopped
    |
    v
Streaming
    |
    v
Disconnecting -> Disconnected
```

Bluetooth link state and media state are stored separately. A short disconnect callback observed around media startup is debounced for one second; `AUDIO_STARTED`/PCM activity cancels the disconnect candidate. This preserves the known QCY startup behavior while keeping explicit disconnect deterministic.

## Wi-Fi

`Idle -> Scanning -> Idle`

`Idle -> Connecting -> Connected -> Idle`

## Bluetooth Classic / BLE

Classic SPP and BLE scan services have separate state and bounded scan tables. A2DP temporarily suspends Classic SPP before starting its source stack.
