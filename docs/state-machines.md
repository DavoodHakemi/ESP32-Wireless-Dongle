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


### Wi-Fi scan publication

```text
Idle
  | SCAN request
  v
Scanning
  | framework reports completion
  v
PublishingBatch[0..N]
  | one bounded batch per application update
  v
Idle + WIFI_SCAN_DONE
```

The scan result set is retained until all result events have been queued and `WIFI_SCAN_DONE` is queued. This prevents a large scan result set from overflowing the fixed EventQueue before completion.

### Bluetooth Classic / BLE cold-start scan

```text
Idle
  | SCAN request
  v
StartPending
  | next service update
  +--> Initializing --> AsyncScanning --> Publishing --> Done
  |
  +--> InitializationFailure --> Done(0)
```

The command response is sent before `StartPending` is executed. The framework-owned Classic/BLE scan APIs then run asynchronously; the application task only collects results, publishes bounded events, and closes the scan state.

### A2DP playback reset boundary

Playback cursor/buffer state is callback-owned. Application-task operations such as `startAudio`, `stopAudio`, and connection reset only raise a protected reset request; the A2DP frame callback consumes that request and resets its playback cursor state in its own execution context.
