#include "protocol/ProtocolCodec.h"

#include "protocol/Protocol.h"
#include "protocol/ProtocolVersion.h"

namespace dongle::protocol {

namespace {

uint16_t crcUpdate(uint16_t crc, uint8_t byte) {
    crc ^= static_cast<uint16_t>(byte) << 8;

    for (uint8_t bit = 0; bit < 8; ++bit) {
        crc = (crc & 0x8000u)
            ? static_cast<uint16_t>((crc << 1) ^ 0x1021u)
            : static_cast<uint16_t>(crc << 1);
    }

    return crc;
}

}  // namespace

uint16_t crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFFu;

    if (!data) {
        return crc;
    }

    for (size_t index = 0; index < length; ++index) {
        crc = crcUpdate(crc, data[index]);
    }

    return crc;
}

bool ProtocolCodec::sendFrameParts(
    FrameType type,
    uint8_t command,
    uint8_t sequence,
    const uint8_t* first,
    uint16_t firstLength,
    const uint8_t* second,
    uint16_t secondLength) {
    const uint32_t totalLength =
        static_cast<uint32_t>(firstLength) + secondLength;

    if (totalLength > MAX_FRAME_PAYLOAD) {
        return false;
    }

    const uint8_t header[FRAME_HEADER_SIZE] = {
        WIRE_VERSION,
        static_cast<uint8_t>(type),
        command,
        sequence,
        static_cast<uint8_t>(totalLength & 0xFFu),
        static_cast<uint8_t>((totalLength >> 8u) & 0xFFu),
    };

    uint16_t crc = crc16(header, sizeof(header));

    if (first && firstLength) {
        for (uint16_t index = 0; index < firstLength; ++index) {
            crc = crcUpdate(crc, first[index]);
        }
    }

    if (second && secondLength) {
        for (uint16_t index = 0; index < secondLength; ++index) {
            crc = crcUpdate(crc, second[index]);
        }
    }

    if (_transport.write(&SOF1, 1) != 1 ||
        _transport.write(&SOF2, 1) != 1 ||
        _transport.write(header, sizeof(header)) != sizeof(header)) {
        return false;
    }

    if (first && firstLength &&
        _transport.write(first, firstLength) != firstLength) {
        return false;
    }

    if (second && secondLength &&
        _transport.write(second, secondLength) != secondLength) {
        return false;
    }

    const uint8_t crcBytes[FRAME_CRC_SIZE] = {
        static_cast<uint8_t>(crc & 0xFFu),
        static_cast<uint8_t>((crc >> 8u) & 0xFFu),
    };

    return _transport.write(crcBytes, sizeof(crcBytes)) ==
        sizeof(crcBytes);
}

bool ProtocolCodec::sendFrame(
    FrameType type,
    uint8_t command,
    uint8_t sequence,
    const uint8_t* payload,
    uint16_t length) {
    return sendFrameParts(
        type,
        command,
        sequence,
        payload,
        length,
        nullptr,
        0);
}

bool ProtocolCodec::sendResponse(
    uint8_t command,
    uint8_t sequence,
    StatusCode status,
    const uint8_t* payload,
    uint16_t length) {
    if (length >= MAX_FRAME_PAYLOAD) {
        return false;
    }

    const uint8_t statusByte = static_cast<uint8_t>(status);

    return sendFrameParts(
        FrameType::Response,
        command,
        sequence,
        &statusByte,
        1,
        payload,
        length);
}

bool ProtocolCodec::sendEvent(
    uint8_t eventId,
    const uint8_t* payload,
    uint16_t length) {
    return sendFrame(
        FrameType::Event,
        eventId,
        0,
        payload,
        length);
}

}  // namespace dongle::protocol
