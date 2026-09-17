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
