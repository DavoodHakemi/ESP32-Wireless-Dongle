#pragma once
#include <WiFi.h>
#include "core/Result.h"
#include "core/Interfaces.h"

namespace dongle::services::wifi {
struct Status {
    uint8_t state{
        0
    };
    // 0 disconnected, 1 connecting, 2 connected, 3 failed
    int16_t rssi{
        -127
    };
    IPAddress ip{};
    String ssid;
};
class WiFiManager final {
public:
    WiFiManager(ILogger& logger, IEventSink& events) : _logger(logger), _events(events) {}
    Result<void> begin();
    Result<void> scan();
    Result<void> connect(const String& ssid, const String& password);
    Result<void> disconnect();
    Status status() const;
    void update();
private:
    ILogger& _logger;
    IEventSink& _events;
    static constexpr uint8_t MAX_SCAN_RESULTS_PER_UPDATE = 8;
    bool _connecting{
        false
    };
    bool _connectedEventSent{
        false
    };
    bool _ipEventSent{
        false
    };
    wl_status_t _lastStatus{
        WL_IDLE_STATUS
    };
    bool _scanPublishing{false};
    int16_t _scanCount{-1};
    uint16_t _scanIndex{0};
    void publishScanResults();
    void publishConnected();
};
}
