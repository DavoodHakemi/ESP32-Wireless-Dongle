#pragma once
#include <cstddef>
#include <cstdint>

namespace dongle::transport {
class ITransport {
public:
    virtual ~ITransport() = default;
    virtual bool begin() = 0;
    virtual bool available() const = 0;
    virtual size_t read(uint8_t* buffer, size_t size) = 0;
    virtual size_t write(const uint8_t* data, size_t size) = 0;
    virtual void update() = 0;
};
}
