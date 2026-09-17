#pragma once
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "core/Result.h"
#include "core/Interfaces.h"

namespace dongle::services::bluetooth {
struct BleScanEntry {
    bool used{
        false
    };
    String address;
    String name;
    String details;
    int8_t rssi{
        -127
    };
};
class BluetoothLE final {
public:
    BluetoothLE(ILogger& logger, IEventSink& events) : _logger(logger), _events(events) {}
    Result<void> begin();
    Result<void> scan(uint16_t seconds);
    Result<void> stop();
    bool scanning() const {
        return _scanning;
    }
    void update();
private:
    static constexpr uint8_t MAX_ENTRIES = 32;
    ILogger& _logger;
    IEventSink& _events;
    BLEScan* _scanner{
        nullptr
    };
    volatile bool _scanning{
        false
    };
    volatile bool _done{
        false
    };
    bool _initialized{
        false
    };
    uint16_t _seconds{
        10
    };
    BleScanEntry _entries[MAX_ENTRIES]{};
    uint8_t _count{
        0
    };
    static BluetoothLE* _instance;
    static void worker(void*);
    void runWorker();
    void clearEntries();
};
}
