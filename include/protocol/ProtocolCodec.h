#pragma once
#include <cstddef>
#include <cstdint>
#include "protocol/ProtocolResponse.h"
#include "transport/ITransport.h"

namespace dongle::protocol {
uint16_t crc16(const uint8_t* data, size_t length);

class ProtocolCodec final {
public:
    explicit ProtocolCodec(transport::ITransport& transport) : _transport(transport) {}
    bool sendFrame(FrameType type, uint8_t command, uint8_t sequence,
        const uint8_t* payload, uint16_t length);
    bool sendResponse(uint8_t command, uint8_t sequence, StatusCode status,
        const uint8_t* payload = nullptr, uint16_t length = 0);
    bool sendEvent(uint8_t eventId, const uint8_t* payload = nullptr, uint16_t length = 0);
private:
    transport::ITransport& _transport;
};
}
