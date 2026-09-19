#include "services/system/SystemService.h"
#include "config/AppConfig.h"
#include "config/HardwareConfig.h"
#include "config/Version.h"
#include <Arduino.h>
#include <cstring>

namespace dongle::services::system {
Result<void> SystemService::reset() {
    _logger.info("System reset requested");
    delay(100);
    ESP.restart();
    return Result<void>::ok();
}
Result<void> SystemService::setBaud(uint32_t baud) {
    return baud==config::SERIAL_BAUD?Result<void>::ok():Result<void>::fail(ErrorCode::InvalidArgument);
}
String SystemService::version()const{
    return DONGLE_FIRMWARE_VERSION;
}
String SystemService::deviceName()const{
    return String("ESP32-WROOM-32");
}
uint16_t SystemService::getInfo(uint8_t*out, uint16_t capacity)const{
    if (!out||capacity<1+32+6)return 0;
    const String name=deviceName();
    const uint8_t n=static_cast<uint8_t>(name.length());
    out[0]=n;
    memcpy(out+1, name.c_str(), n);
    const uint64_t mac=ESP.getEfuseMac();
    for (uint8_t i=0;i<6;++i)out[1+n+i]=static_cast<uint8_t>((mac>>(8*i))&0xFF);
    return static_cast<uint16_t>(1+n+6);
}
}
