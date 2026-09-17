#pragma once
#include <cstdint>

namespace dongle::application {
enum class ApplicationState : uint8_t {
    Booting, Running, Fault
};
}
