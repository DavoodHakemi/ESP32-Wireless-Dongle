#include "services/wifi/WiFiManager.h"
#include <cstring>
#include "core/Event.h"

namespace dongle::services::wifi {
Result<void> WiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    return Result<void>::ok();
}
Result<void> WiFiManager::scan() {
    if (_scanPublishing || WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
        return Result<void>::fail(ErrorCode::Busy);
    }
    WiFi.scanDelete();
    return WiFi.scanNetworks(true, true) == WIFI_SCAN_FAILED
    ? Result<void>::fail(ErrorCode::ScanFailed) : Result<void>::ok();
}
Result<void> WiFiManager::connect(const String& ssid, const String& password) {
    if (!ssid.length()) return Result<void>::fail(ErrorCode::InvalidArgument);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    delay(50);
    _connecting = true;
    _connectedEventSent = false;
    _ipEventSent = false;
    WiFi.begin(ssid.c_str(), password.c_str());
    return Result<void>::ok();
}
Result<void> WiFiManager::disconnect() {
    _connecting = false;
    _connectedEventSent = false;
    _ipEventSent = false;
    WiFi.disconnect(false, false);
    return Result<void>::ok();
}
Status WiFiManager::status() const {
    Status result;
    const wl_status_t state = WiFi.status();
    result.state = state == WL_CONNECTED ? 2 : (_connecting ? 1 : 0);
    if (state == WL_NO_SSID_AVAIL || state == WL_CONNECT_FAILED) result.state = 3;
    result.rssi = state == WL_CONNECTED ? static_cast<int16_t>(constrain(WiFi.RSSI(), -127, 0)) : -127;
    result.ip = WiFi.localIP();
    result.ssid = WiFi.SSID();
    return result;
}
void WiFiManager::publishScanResults() {
    const int16_t completed = WiFi.scanComplete();

    if (!_scanPublishing && completed >= 0) {
        _scanPublishing = true;
        _scanCount = completed;
        _scanIndex = 0;
    }

    if (!_scanPublishing) {
        return;
    }

    const uint16_t count = static_cast<uint16_t>(_scanCount < 0 ? 0 : _scanCount);
    const uint16_t remaining = count > _scanIndex
        ? static_cast<uint16_t>(count - _scanIndex)
        : 0;
    const uint16_t batch = remaining > MAX_SCAN_RESULTS_PER_UPDATE
        ? MAX_SCAN_RESULTS_PER_UPDATE
        : remaining;

    for (uint16_t n = 0; n < batch; ++n) {
        const uint16_t i = static_cast<uint16_t>(_scanIndex + n);
        String ssid = WiFi.SSID(i);
        if (ssid.length() > 32) ssid = ssid.substring(0, 32);
        const int16_t rssi = static_cast<int16_t>(WiFi.RSSI(i));
        const uint8_t channel = static_cast<uint8_t>(WiFi.channel(i));
        const uint8_t auth = static_cast<uint8_t>(WiFi.encryptionType(i));
        uint8_t payload[1 + 32 + 2 + 1 + 1]{};
        const uint8_t len = static_cast<uint8_t>(ssid.length());
        payload[0] = len;
        memcpy(payload + 1, ssid.c_str(), len);
        const uint16_t offset = static_cast<uint16_t>(1 + len);
        payload[offset] = static_cast<uint8_t>(rssi & 0xFF);
        payload[offset + 1] = static_cast<uint8_t>(rssi >> 8);
        payload[offset + 2] = channel;
        payload[offset + 3] = auth;
        _events.publish({EventType::WifiNetworkFound, payload, static_cast<uint16_t>(offset + 4)});
    }

    _scanIndex = static_cast<uint16_t>(_scanIndex + batch);

    if (_scanIndex < count) {
        return;
    }

    uint8_t countPayload[2] = {
        static_cast<uint8_t>(count & 0xFF),
        static_cast<uint8_t>(count >> 8)
    };
    _events.publish({EventType::WifiScanDone, countPayload, sizeof(countPayload)});
    WiFi.scanDelete();
    _scanPublishing = false;
    _scanCount = -1;
    _scanIndex = 0;
}

void WiFiManager::publishConnected() {
    String ssid = WiFi.SSID();
    if (ssid.length() > 32) ssid = ssid.substring(0, 32);
    uint8_t payload[33]{};
    payload[0] = static_cast<uint8_t>(ssid.length());
    if (payload[0]) memcpy(payload + 1, ssid.c_str(), payload[0]);
    _events.publish({EventType::WifiConnected, payload, static_cast<uint16_t>(1 + payload[0])});
}
void WiFiManager::update() {
    publishScanResults();
    const wl_status_t current = WiFi.status();
    if (current == WL_CONNECTED) {
        _connecting = false;
        if (!_connectedEventSent) {
            publishConnected();
            _connectedEventSent = true;
        }
        if (!_ipEventSent && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
            const IPAddress ip = WiFi.localIP();
            uint8_t payload[4] = {
                ip[0], ip[1], ip[2], ip[3]
            };
            _events.publish({EventType::WifiGotIp, payload, sizeof(payload)});
            _ipEventSent = true;
        }
    }
    else if (_connecting && (current == WL_CONNECT_FAILED || current == WL_NO_SSID_AVAIL)) {
        _connecting = false;
        _events.publish({EventType::WifiConnectFailed, nullptr, 0});
    }
    else if (_lastStatus == WL_CONNECTED && current != WL_CONNECTED) {
        _connecting = false;
        _connectedEventSent = false;
        _ipEventSent = false;
        _events.publish({EventType::WifiDisconnected, nullptr, 0});
    }
    _lastStatus = current;
}
}
