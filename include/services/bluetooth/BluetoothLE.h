#pragma once
#include <atomic>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "core/Result.h"
#include "core/Interfaces.h"

namespace dongle::services::bluetooth {
struct BleScanEntry {
    bool used{false};
    String address;
    String name;
    String details;
    int8_t rssi{-127};
};
class BluetoothLE final {
public:
    BluetoothLE(ILogger& logger, IEventSink& events) : _logger(logger), _events(events) {}
    Result<void> begin();
    Result<void> scan(uint16_t seconds);
    Result<void> stop();
    bool scanning() const { return _scanning; }
    bool scanPending() const { return _scanStartPending; }
    bool busy() const { return _scanning || _scanStartPending || _publishing; }
    void update();
private:
    static constexpr uint8_t MAX_ENTRIES = 32;
    static constexpr uint8_t PUBLISH_ENTRIES_PER_UPDATE = 4;
    ILogger& _logger;
    IEventSink& _events;
    BLEScan* _scanner{nullptr};
    std::atomic_bool _scanning{false};
    std::atomic_bool _done{false};
    bool _scanStartPending{false};
    bool _publishing{false};
    uint8_t _publishIndex{0};
    uint32_t _scanDeadlineMs{0};
    bool _initialized{false};
    uint16_t _seconds{10};
    BleScanEntry _entries[MAX_ENTRIES]{};
    uint8_t _count{0};
    static std::atomic<BluetoothLE*> _instance;
    static void scanCompleteCallback(BLEScanResults results);
    void processResults();
    void clearEntries();
};
}