#include "transport/SerialTransport.h"
#include "config/HardwareConfig.h"
#include <Arduino.h>

namespace dongle::transport {
SerialTransport::SerialTransport(uint32_t baud) : _baud(baud) {}
bool SerialTransport::begin() {
    Serial.setRxBufferSize(config::SERIAL_RX_BUFFER);
    Serial.setTxBufferSize(config::SERIAL_TX_BUFFER);
    Serial.begin(_baud, SERIAL_8N1);
    return true;
}
bool SerialTransport::available() const {
    return Serial.available() > 0;
}
size_t SerialTransport::read(uint8_t* buffer, size_t size) {
    if (!buffer || !size) return 0;
    return Serial.read(buffer, size);
}
size_t SerialTransport::write(const uint8_t* data, size_t size) {
    if (!data || !size) return 0;
    return Serial.write(data, size);
}
void SerialTransport::update() {}
}
