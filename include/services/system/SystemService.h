#pragma once
#include <Arduino.h>
#include "core/Result.h"
#include "core/Interfaces.h"

namespace dongle::services::system {
class SystemService final {
public:
    explicit SystemService(ILogger& logger) : _logger(logger) {}
    Result<void> reset();
    Result<void> setBaud(uint32_t baud);
    String version() const;
    String deviceName() const;
    uint16_t getInfo(uint8_t* out, uint16_t capacity) const;
private:
    ILogger& _logger;
};
}
