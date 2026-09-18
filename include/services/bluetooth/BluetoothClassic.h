#pragma once
#include <atomic>
#include <BluetoothSerial.h>
#include <esp_gap_bt_api.h>
#include "core/Result.h"
#include "core/Interfaces.h"

namespace dongle::services::bluetooth {
struct ClassicScanEntry {
    bool used{
        false
    };
    uint8_t address[6]{};
    int8_t rssi{
        -127
    };
    uint32_t cod{
        0
    };
    bool hasRssi{
        false
    };
    bool hasCod{
        false
    };
    char name[65]{};
};
class BluetoothClassic final {
public:
    BluetoothClassic(ILogger& logger, IEventSink& events);
    Result<void> begin();
    Result<void> scan(uint16_t seconds);
    Result<void> stopScan();
    Result<void> connect(const String& address);
    Result<void> disconnect();
    bool connected();
    bool initialized() const {
        return _initialized;
    }
    bool scanning() const {
        return _scanning;
    }
    bool scanPending() const {
        return _scanStartPending;
    }
    bool busy() const {
        return _scanning || _scanStartPending || _scanPublishing ||
               _restoreAfterScanPending || _connecting;
    }
    bool connecting() const {
        return _connecting;
    }
    String localAddress();
    String remoteAddress() const {
        return _remoteAddress;
    }
    void update();
    void suspendForA2dp();
    void restoreAfterA2dp();
private:
    static constexpr uint8_t MAX_ENTRIES = 32;
    static constexpr uint8_t PUBLISH_ENTRIES_PER_UPDATE = 4;
    BluetoothSerial _serial;
    ILogger& _logger;
    IEventSink& _events;
    bool _initialized{false};
    std::atomic_bool _scanning{false};
    bool _connecting{false};
    std::atomic_bool _scanCompletionPending{false};
    bool _scanPublishing{false};
    bool _restoreAfterScanPending{false};
    uint8_t _scanPublishIndex{0};
    bool _scanStartPending{
        false
    };
    String _remoteAddress;
    ClassicScanEntry _entries[MAX_ENTRIES]{};
    uint8_t _count{
        0
    };
    uint16_t _scanSeconds{
        10
    };
    static std::atomic<BluetoothClassic*> _callbackInstance;
    static void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
    void handleGapEvent(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
    void startScanPublication();
    void publishScanBatch();
    Result<void> startScanNow();
    int findEntry(const esp_bd_addr_t address) const;
    int allocateEntry(const esp_bd_addr_t address);
    static bool parseMac(const String& text, uint8_t out[6]);
    static bool copyNameFromEir(uint8_t* eir, char* out, size_t outSize);
    static void formatAddress(const uint8_t address[6], char out[18]);
    static uint8_t majorClass(uint32_t cod);
    static uint8_t minorClass(uint32_t cod);
};
}
