#pragma once
#include <cstdint>

namespace dongle {
enum class ErrorCode : uint8_t {
    None = 0,
    InvalidArgument,
    NotInitialized,
    AlreadyConnected,
    NotConnected,
    Busy,
    Timeout,
    ConnectionFailed,
    ScanFailed,
    HardwareError,
    Unsupported,
    InternalError
};

const char* errorToString(ErrorCode error);
}
