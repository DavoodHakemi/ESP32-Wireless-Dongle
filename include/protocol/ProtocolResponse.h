#pragma once
#include <cstdint>
#include "protocol/ProtocolFrame.h"

namespace dongle::protocol {
enum class StatusCode : uint8_t {
    Success = 0x00, InvalidCommand = 0x01, InvalidParameter = 0x02, Busy = 0x03,
    Timeout = 0x04, NotConnected = 0x05, InternalError = 0x06, CrcError = 0x07,
    NotSupported = 0x08
};
struct ProtocolResponse {
    uint8_t command{
        0
    };
    uint8_t sequence{
        0
    };
    StatusCode status{
        StatusCode::InternalError
    };
    const uint8_t* payload{
        nullptr
    };
    uint16_t length{
        0
    };
};
}
