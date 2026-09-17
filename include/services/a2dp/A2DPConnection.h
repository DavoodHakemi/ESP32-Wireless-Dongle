#pragma once
#include <cstdint>

namespace dongle::services::a2dp {
enum class ConnectionState : uint8_t {
    Disconnected, Connecting, Connected, Streaming, Disconnecting, Error
};
}
