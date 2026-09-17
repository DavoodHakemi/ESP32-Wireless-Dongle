# Windows Host Utility

`main.py` remains the compatibility CLI for the existing dongle protocol. Firmware services do not depend on the host implementation.

The host uses a background RX thread so asynchronous ESP32 events are printed while the menu waits for input. Log format:

- `[ESP, Type, <timestamp> ms] ...`
- `[PC, Type, <elapsed> ms] ...`

The packaged host expects firmware semantic version `2.3.2`.
