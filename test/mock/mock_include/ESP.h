// Mock ESP.h — esp32 chip API subset used by the firmware.
#pragma once

#include <cstdint>
#include "Arduino.h"

class EspClass {
public:
    void restart() {}
    uint64_t getEfuseMac() const { return 0x0011223344556677ULL; }
    uint32_t getHeapSize() const { return 320000; }
    uint32_t getFreeHeap() const { return 200000; }
    uint32_t getMinFreeHeap() const { return 150000; }
    const char* getChipModel() const { return "ESP32-D0WD-V3"; }
    uint8_t getChipRevision() const { return 3; }
    uint32_t getCpuFreqMHz() const { return 240; }
};

extern EspClass ESP;
