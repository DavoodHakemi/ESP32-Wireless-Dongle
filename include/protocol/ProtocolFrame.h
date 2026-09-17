#pragma once

#include <cstdint>

namespace dongle::protocol {

constexpr uint8_t SOF1 = 0xAA;
constexpr uint8_t SOF2 = 0x55;

enum class FrameType : uint8_t {
    Request = 0x01,
    Response = 0x02,
    Event = 0x03,
    Error = 0x04
};

}
