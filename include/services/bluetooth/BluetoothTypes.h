#pragma once
#include <cstdint>

namespace dongle::services::bluetooth {
enum class ClassicState : uint8_t {
    Idle, Scanning, Connecting, Connected, Error
};
enum class BleState : uint8_t {
    Idle, Scanning, Complete, Error
};
}
