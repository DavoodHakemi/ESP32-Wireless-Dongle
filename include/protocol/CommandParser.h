#pragma once
#include <cstdint>
#include "protocol/ProtocolCommand.h"
#include "protocol/ProtocolFrame.h"
#include "protocol/ProtocolVersion.h"

namespace dongle::protocol {
class ICommandSink {
public:
    virtual ~ICommandSink() = default;
    virtual void onCommand(const ProtocolCommand& command) = 0;
};

class CommandParser final {
public:
    static constexpr uint16_t MAX_PAYLOAD = 4096;
    explicit CommandParser(ICommandSink& sink) : _sink(sink) {}

    void reset();
    void feed(uint8_t byte);
    void feed(const uint8_t* data, uint16_t length);

private:
    enum class State : uint8_t {
        WaitSof1, WaitSof2, ReadHeader, ReadPayload, ReadCrc
    };
    State _state{
        State::WaitSof1
    };
    uint8_t _header[6]{};
    uint8_t _headerIndex{
        0
    };
    uint8_t _payload[MAX_PAYLOAD]{};
    uint16_t _payloadIndex{
        0
    };
    uint16_t _expectedPayload{
        0
    };
    uint8_t _crc[2]{};
    uint8_t _crcIndex{
        0
    };
    ICommandSink& _sink;

    void processFrame();
};
}
