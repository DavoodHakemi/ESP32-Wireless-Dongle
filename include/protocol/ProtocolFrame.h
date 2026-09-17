#pragma once

#include <cstdint>

namespace dongle::protocol {

constexpr uint8_t SOF1 = 0xAA;
constexpr uint8_t SOF2 = 0x55;
constexpr uint8_t FRAME_HEADER_SIZE = 6;
constexpr uint8_t FRAME_CRC_SIZE = 2;
constexpr uint16_t MAX_FRAME_PAYLOAD = 4096;

enum class FrameType : uint8_t {
    Request = 0x01,
    Response = 0x02,
    Event = 0x03,
    Error = 0x04,
};

struct FrameHeader {
    uint8_t version;
    FrameType type;
    uint8_t command;
    uint8_t sequence;
    uint16_t payloadLength;
};

}  // namespace dongle::protocol
