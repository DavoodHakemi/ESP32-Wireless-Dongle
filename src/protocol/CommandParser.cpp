#include "protocol/CommandParser.h"
#include "protocol/Protocol.h"

namespace dongle::protocol {

namespace {
    uint16_t updateCrc(uint16_t crc, uint8_t byte) {
        crc ^= static_cast<uint16_t>(byte) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000u) ? static_cast<uint16_t>((crc << 1) ^ 0x1021u)
            : static_cast<uint16_t>(crc << 1);
        }
        return crc;
    }
}

void CommandParser::reset() {
    _state = State::WaitSof1;
    _headerIndex = 0;
    _payloadIndex = 0;
    _expectedPayload = 0;
    _crcIndex = 0;
}

void CommandParser::feed(const uint8_t* data, uint16_t length) {
    if (!data) return;
    for (uint16_t i = 0; i < length; ++i) feed(data[i]);
}

void CommandParser::feed(uint8_t byte) {
    switch (_state) {
    case State::WaitSof1:
        if (byte == SOF1) _state = State::WaitSof2;
        break;
    case State::WaitSof2:
        if (byte == SOF2) {
            _headerIndex = 0;
            _state = State::ReadHeader;
        }
        else if (byte == SOF1) {
            _state = State::WaitSof2;
        }
        else {
            _state = State::WaitSof1;
        }
        break;
    case State::ReadHeader:
        _header[_headerIndex++] = byte;
        if (_headerIndex == FRAME_HEADER_SIZE) {
            _expectedPayload = static_cast<uint16_t>(_header[4]) |
            (static_cast<uint16_t>(_header[5]) << 8);
            if (_expectedPayload > MAX_PAYLOAD) {
                reset();
                break;
            }
            _payloadIndex = 0;
            _crcIndex = 0;
            _state = _expectedPayload ? State::ReadPayload : State::ReadCrc;
        }
        break;
    case State::ReadPayload:
        _payload[_payloadIndex++] = byte;
        if (_payloadIndex == _expectedPayload) _state = State::ReadCrc;
        break;
    case State::ReadCrc:
        _crc[_crcIndex++] = byte;
        if (_crcIndex == 2) {
            processFrame();
            reset();
        }
        break;
    }
}

void CommandParser::processFrame() {
    const uint16_t declared = static_cast<uint16_t>(_header[4]) |
    (static_cast<uint16_t>(_header[5]) << 8);
    if (_header[0] != WIRE_VERSION ||
        _header[1] != static_cast<uint8_t>(FrameType::Request) ||
        declared != _payloadIndex ||
        declared > MAX_FRAME_PAYLOAD) {
        return;
    }

    uint16_t crc = 0xFFFFu;
    for (uint8_t byte : _header) crc = updateCrc(crc, byte);
    for (uint16_t i = 0; i < _payloadIndex; ++i) crc = updateCrc(crc, _payload[i]);

    const uint16_t received = static_cast<uint16_t>(_crc[0]) |
    (static_cast<uint16_t>(_crc[1]) << 8);
    if (crc != received) return;

    _sink.onCommand({static_cast<CommandId>(_header[2]), _header[3], _payload, _payloadIndex});
}
}
