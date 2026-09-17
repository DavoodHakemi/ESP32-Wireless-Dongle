#include "core/Error.h"

namespace dongle {
const char* errorToString(ErrorCode error) {
    switch (error) {
    case ErrorCode::None: return"NONE";
    case ErrorCode::InvalidArgument: return"INVALID_ARGUMENT";
    case ErrorCode::NotInitialized: return"NOT_INITIALIZED";
    case ErrorCode::AlreadyConnected: return"ALREADY_CONNECTED";
    case ErrorCode::NotConnected: return"NOT_CONNECTED";
    case ErrorCode::Busy: return"BUSY";
    case ErrorCode::Timeout: return"TIMEOUT";
    case ErrorCode::ConnectionFailed: return"CONNECTION_FAILED";
    case ErrorCode::ScanFailed: return"SCAN_FAILED";
    case ErrorCode::HardwareError: return"HARDWARE_ERROR";
    case ErrorCode::Unsupported: return"UNSUPPORTED";
    case ErrorCode::InternalError: return"INTERNAL_ERROR";
    }
    return"UNKNOWN";
}
}
