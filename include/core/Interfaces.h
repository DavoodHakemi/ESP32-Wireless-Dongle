#pragma once
#include <cstdint>
#include "core/Event.h"

namespace dongle {
class IEventSink {
public:
    virtual ~IEventSink() = default;
    virtual void publish(const Event& event) = 0;
};

class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void info(const char* fmt, ...) = 0;
    virtual void warn(const char* fmt, ...) = 0;
    virtual void error(const char* fmt, ...) = 0;
    virtual void debug(const char* fmt, ...) = 0;
};
}
