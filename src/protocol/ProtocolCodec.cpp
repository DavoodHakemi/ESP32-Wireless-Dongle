#include "protocol/ProtocolCodec.h"
#include "protocol/Protocol.h"
#include "protocol/ProtocolVersion.h"
#include <cstring>

namespace dongle::protocol {

namespace {
    uint16_t crcUpdate(uint16_t crc, uint8_t byte) {
        crc ^= static_cast<uint16_t>(byte) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1) ^ 0x1021u)
            : static_cast<uint16_t>(crc << 1);
        }
        return crc;
    }
}

uint16_t crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFFu;
    if (!data) return crc;
    for (size_t i = 0; i < length; ++i) crc = crcUpdate(crc, data[i]);
    return crc;
}

bool ProtocolCodec::sendFrame(FrameType type, uint8_t command, uint8_t sequence,
    const uint8_t* payload, uint16_t length) {
    if (length > 4096) return false;
    const uint8_t header[6] = {
        WIRE_VERSION,
        static_cast<uint8_t>(type),
        command,
        sequence,
        static_cast<uint8_t>(length & 0xFF),
        static_cast<uint8_t>(length >> 8)
    };
    uint16_t crc = crc16(header, sizeof(header));
    if (payload && length) {
        for (uint16_t i = 0; i < length; ++i) crc = crcUpdate(crc, payload[i]);
    }

    if (_transport.write(&SOF1, 1) != 1) return false;
    if (_transport.write(&SOF2, 1) != 1) return false;
    if (_transport.write(header, sizeof(header)) != sizeof(header)) return false;
    if (payload && length && _transport.write(payload, length) != length) return false;
    const uint8_t crcBytes[2] = {
        static_cast<uint8_t>(crc & 0xFF), static_cast<uint8_t>(crc >> 8)
    };
    return _transport.write(crcBytes, sizeof(crcBytes)) == sizeof(crcBytes);
}

bool ProtocolCodec::sendResponse(uint8_t command, uint8_t sequence, StatusCode status,
    const uint8_t* payload, uint16_t length) {
    if (length > 4095) return false;
    uint8_t buffer[4096];
    buffer[0] = static_cast<uint8_t>(status);
    if (payload && length) std::memcpy(buffer + 1, payload, length);
    return sendFrame(FrameType::Response, command, sequence, buffer,
        static_cast<uint16_t>(length + 1));
}

bool ProtocolCodec::sendEvent(uint8_t eventId, const uint8_t* payload, uint16_t length) {
    return sendFrame(FrameType::Event, eventId, 0, payload, length);
}
}
