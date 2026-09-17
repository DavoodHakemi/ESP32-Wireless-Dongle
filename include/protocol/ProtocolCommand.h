#pragma once
#include <cstdint>
#include "protocol/Protocol.h"

namespace dongle::protocol {
struct ProtocolCommand {
    CommandId id{
        CommandId::GetInfo
    };
    uint8_t sequence{
        0
    };
    const uint8_t* payload{
        nullptr
    };
    uint16_t length{
        0
    };
};
}
