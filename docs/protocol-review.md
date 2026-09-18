# Protocol Reliability Review — v2.3.10

## Findings

The wire format itself was not fundamentally wrong; the main reliability problems were in the framing/transaction implementation around it.

1. **Large temporary response buffer**
   `ProtocolCodec::sendResponse()` previously created a 4096-byte stack buffer for every response, even when the response payload was a few bytes. This created unnecessary stack pressure on the ESP32 main task and made small control commands less reliable under a memory-heavy firmware.

2. **Request/response handling had no retry policy**
   The host assumed every control response would arrive on the first transmission. The bootstrap commands are idempotent, so they now support bounded retransmission using the same sequence number.

3. **Control transactions were not explicitly serialized on the host**
   A transaction lock now prevents two control requests from sharing the same response queue at the same time. Streaming `AUDIO_DATA` remains independent and one-way.

4. **Frame types and parser failures were not diagnosed**
   The host parser now validates frame type and maintains parser statistics for accepted frames, version errors, length errors and CRC errors. Timeouts include these diagnostics.

5. **Asynchronous events and command responses have different semantics**
   Responses are correlated by `(sequence, command)`. Events use sequence `0` and are never treated as a response. This distinction is now explicit in the documentation and implementation.

6. **The protocol intentionally contains one response-less command**
   `AUDIO_DATA (0x6D)` is a high-rate streaming path and remains one-way to preserve the existing audio transport behavior. All other current control commands return a response.

## Preserved wire contract

The following are intentionally unchanged:

- SOF: `AA 55`
- Wire version: `0x01`
- Header order and size: 6 bytes
- CRC: CRC-16/CCITT-FALSE, initial `0xFFFF`, polynomial `0x1021`
- Legacy command IDs
- Legacy event IDs
- Status byte as the first response payload byte
- Existing response payload layouts, including the 36-byte audio status payload

## Transaction rules

- Host assigns an 8-bit sequence number to each control request.
- Firmware copies the request sequence into the response.
- Host consumes a response only when both sequence and command ID match the outstanding request.
- Events use sequence `0` and are processed independently.
- Bootstrap commands `PING`, `GET_VERSION`, and `GET_INFO` may be retried because they are side-effect free.
- Commands that start asynchronous work return an acceptance status and later publish completion/failure events.

## Future extension path

The wire version remains `0x01` for compatibility. New commands/events should be added without reusing an existing ID. If an incompatible frame-layout change is ever required, the wire version—not the firmware semantic version—must be incremented.
7. **Asynchronous command handlers must not execute subsystem startup inline.** A2DP connection commands now enqueue a pending connection operation and return the protocol acknowledgement immediately. The application update loop starts the actual Bluetooth/A2DP lifecycle after the transaction has completed.

8. **Preferences must be initialized after Arduino startup.** A2DP NVS preferences are opened from `A2DPManager::begin()`, not from the global-object constructor. This avoids using framework-dependent storage services before `setup()`.

