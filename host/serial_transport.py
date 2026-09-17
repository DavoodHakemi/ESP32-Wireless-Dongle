import threading
import time

import serial

try:
    from console import install_console_contract
except ImportError:
    from host.console import install_console_contract

install_console_contract()


class SerialTransport:
    """Low-level serial transport."""

    def __init__(
        self,
        port: str,
        baudrate: int = 921600,
        timeout: float = 0.05,
    ) -> None:
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.serial: serial.Serial | None = None
        self._write_lock = threading.Lock()

    def connect(self) -> None:
        if self.serial is not None and self.serial.is_open:
            return

        # Open the port without the automatic DTR/RTS transition that can reset
        # ESP32 DevKit boards through the USB-UART bridge.
        self.serial = serial.Serial(port=None)
        self.serial.port = self.port
        self.serial.baudrate = self.baudrate
        self.serial.bytesize = serial.EIGHTBITS
        self.serial.parity = serial.PARITY_NONE
        self.serial.stopbits = serial.STOPBITS_ONE
        self.serial.timeout = self.timeout
        self.serial.write_timeout = 1.0
        self.serial.xonxoff = False
        self.serial.rtscts = False
        self.serial.dsrdtr = False
        self.serial.dtr = False
        self.serial.rts = False
        self.serial.open()

        # Allow the UART/driver to settle without resetting the ESP32.
        time.sleep(0.20)
        self.serial.reset_input_buffer()
        self.serial.reset_output_buffer()

    def set_baudrate(self, baudrate: int) -> None:
        if not self.is_connected():
            raise RuntimeError("Serial port is not connected")
        self.serial.baudrate = int(baudrate)
        self.baudrate = int(baudrate)

    def disconnect(self) -> None:
        if self.serial is not None:
            self.serial.close()

        self.serial = None

    def is_connected(self) -> bool:
        return (
            self.serial is not None
            and self.serial.is_open
        )

    def write(self, data: bytes) -> None:
        if not self.is_connected():
            raise RuntimeError("Serial port is not connected")

        with self._write_lock:
            self.serial.write(data)
            self.serial.flush()

    def read(self, size: int = 512) -> bytes:
        if not self.is_connected():
            raise RuntimeError("Serial port is not connected")

        return self.serial.read(size)
