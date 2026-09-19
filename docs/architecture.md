# Architecture

## Layering

```text
+--------------------------- Application ---------------------------+
| DongleApplication: lifecycle, orchestration, domain-event mapping |
+-------------------------------+----------------------------------+
                                |
                    +-----------v-----------+
                    |       Protocol        |
                    | Parser / Codec /      |
                    | Dispatcher / IDs      |
                    +-----------+-----------+
                                |
                    +-----------v-----------+
                    |        Services       |
                    | WiFi / Network / BT / |
                    | BLE / A2DP / System   |
                    +-----------+-----------+
                                |
                    +-----------v-----------+
                    | Hardware + 3rd party   |
                    | Arduino/ESP-IDF/A2DP  |
                    +-------------------------+

Transport is a separate byte-movement abstraction used by ProtocolCodec/Parser.
```

## Dependency direction

Application owns all concrete objects. Protocol only knows the transport abstraction and command/service APIs. Services use `ILogger` and a domain `IEventSink`; they do not include protocol IDs and do not write UART directly. `DongleApplication` receives domain events through a fixed-size `EventQueue`, then maps them to wire `protocol::EventId` values from the application update loop. This prevents asynchronous Bluetooth/A2DP callbacks from writing protocol frames concurrently with command responses.

## Callback boundary

A2DP and BLE callbacks are treated as real-time/background boundaries. The A2DP frame callback only updates counters and fills PCM frames; it never allocates, logs or emits protocol events. Callback state is converted to domain events from `A2DPManager::update()`.

The third-party A2DP library has a callback without a context for Classic name selection; one static `A2DPManager*` is retained solely at that adapter boundary. Connection/audio callbacks use the library-provided `void*` context and therefore route directly to the owning manager instance.

## Transport independence

`ITransport` contains only byte transport operations. `SerialTransport` is the present UART0/CP2102 implementation. A future SPI or USB transport can be added without changing the protocol parser, dispatcher or service logic.


### Protocol framing

`protocol/ProtocolFrame.h` owns SOF markers and `FrameType`, keeping transport-neutral wire framing separate from command IDs and response payload definitions.

### Asynchronous A2DP startup

The ESP32-A2DP library performs a startup delay while bringing up its Bluetooth/A2DP stack. `A2DPSourceAdapter` isolates this blocking third-party call in a one-shot FreeRTOS task so the application update loop and protocol command path remain responsive. The command returns an immediate acknowledgement while the A2DP state machine continues asynchronously.


## Application scheduling contract

`DongleApplication::update()` preserves the legacy service order:

```text
UART RX / command dispatch
        |
        v
WiFi.update -> drain events
        v
Network.update -> drain events
        v
Bluetooth.update -> drain events
        v
A2DP.update -> drain events
```

Command responses are written immediately from the protocol dispatcher. Domain events are drained only from the application task, after each service phase, so asynchronous event traffic cannot overtake a command response or remain queued behind another service's burst.

The EventQueue is deliberately fixed at 65 records: Classic discovery can emit up to 32 device-found events, 32 detail events, and one completion event. Wi-Fi scan publication therefore uses bounded batches rather than relying on a larger queue.

Cold-start Bluetooth Classic/BLE scan initialization is deferred from the command path into the corresponding service update. The acceptance response therefore remains bounded by the protocol/transport path, while completion and initialization failures remain asynchronous service events.


## PC audio transport timing

The default PC audio profile is 22.05 kHz, mono, 16-bit PCM. Windows loopback capture is performed at 44.1 kHz with 256-frame reads, giving an exact 2:1 resampling ratio. Two capture reads therefore produce exactly one 256-sample transport block (11.61 ms of audio), avoiding the fractional 48 kHz -> 22.05 kHz packet cadence that previously emitted alternating 470/471-sample output bursts.

The host sender emits one complete audio block at the block playback interval instead of forwarding capture bursts directly to UART. The host queue is intentionally bounded to eight blocks (~93 ms at 22.05 kHz); when the transport falls behind, the oldest queued block is discarded rather than blocking WASAPI capture and allowing live latency to grow into seconds.

At 921600 baud, a typical 523-byte mono audio frame occupies about 5.7 ms of line time, below the 11.61 ms audio-block period. The transport therefore has approximately 2x line-rate headroom for the default profile; continuity depends on scheduler/OS/UART jitter rather than raw serial bandwidth.
