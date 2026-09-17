#pragma once
#include <cstdarg>
#include "core/Interfaces.h"

namespace dongle::utils {
class Logger final : public ILogger {
public:
    explicit Logger(IEventSink& sink) : _sink(sink) {}
    void info(const char* fmt, ...) override;
    void warn(const char* fmt, ...) override;
    void error(const char* fmt, ...) override;
    void debug(const char* fmt, ...) override;
private:
    IEventSink& _sink;
    void log(const char* type, const char* fmt, va_list args);
};
}
