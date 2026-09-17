#pragma once
#include <cstdint>
#include "transport/ITransport.h"

namespace dongle::transport {
class SerialTransport final : public ITransport {
public:
    explicit SerialTransport(uint32_t baud);
    bool begin() override;
    bool available() const override;
    size_t read(uint8_t* buffer, size_t size) override;
    size_t write(const uint8_t* data, size_t size) override;
    void update() override;
    uint32_t baud() const {
        return _baud;
    }
private:
    uint32_t _baud;
};
}
