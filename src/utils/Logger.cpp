#include "utils/Logger.h"
#include "core/Event.h"
#include "config/BuildConfig.h"
#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace dongle::utils {
void Logger::log(const char* type, const char* fmt, va_list args) {
    if (!fmt) return;
    char message[256];
    const int written = vsnprintf(message, sizeof(message), fmt, args);
    if (written < 0) return;
    const uint16_t textLength = static_cast<uint16_t>(written >= static_cast<int>(sizeof(message))
        ? sizeof(message) - 1 : written);
    const size_t rawTypeLength = type ? strlen(type) : 0;
    const uint8_t typeLength = static_cast<uint8_t>(rawTypeLength > 15 ? 15 : rawTypeLength);

    uint8_t payload[4 + 1 + 15 + 255]{};
    const uint32_t timestamp = millis();
    memcpy(payload, &timestamp, sizeof(timestamp));
    payload[4] = typeLength;
    if (typeLength) memcpy(payload + 5, type, typeLength);
    if (textLength) memcpy(payload + 5 + typeLength, message, textLength);
    _sink.publish({EventType::SerialLog, payload,
            static_cast<uint16_t>(5 + typeLength + textLength)});
}
void Logger::info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log("Log", fmt, args);
    va_end(args);
}
void Logger::warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log("Warn", fmt, args);
    va_end(args);
}
void Logger::error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log("Error", fmt, args);
    va_end(args);
}
void Logger::debug(const char* fmt, ...) {
#if CORE_DEBUG_LEVEL > 0
    va_list args;
    va_start(args, fmt);
    log("Debug", fmt, args);
    va_end(args);
#else
    (void)fmt;
#endif
}
}
