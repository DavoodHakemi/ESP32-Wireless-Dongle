// Mock BluetoothSerial.h — the API subset used by BluetoothClassic.
// Signatures follow Arduino-ESP32 3.0.7 BluetoothSerial.
#pragma once

#include <string>
#include <vector>
#include "Arduino.h"
#include "mock_esp_common.h"

class BTAddress {
public:
    BTAddress() = default;
    explicit BTAddress(const esp_bd_addr_t addr) {
        memcpy(_addr, addr, 6);
    }
    const esp_bd_addr_t* getNative() const { return &_addr; }
    String toString() const {
        char text[18]{};
        snprintf(text, sizeof(text), "%02x:%02x:%02x:%02x:%02x:%02x",
                 _addr[0], _addr[1], _addr[2], _addr[3], _addr[4], _addr[5]);
        return String(text);
    }
private:
    esp_bd_addr_t _addr{};
};

class BTAdvertisedDevice {
public:
    BTAddress getAddress() const { return _address; }
    bool haveRSSI() const { return _haveRssi; }
    int getRSSI() const { return _rssi; }
    bool haveCOD() const { return _haveCod; }
    uint32_t getCOD() const { return _cod; }
    bool haveName() const { return !_name.empty(); }
    std::string getName() const { return _name; }

private:
    friend class MockScanResultBuilder;
    BTAddress _address;
    bool _haveRssi = false;
    int _rssi = 0;
    bool _haveCod = false;
    uint32_t _cod = 0;
    std::string _name;
};

class BTScanResults {
public:
    int getCount() const { return static_cast<int>(_devices.size()); }
    BTAdvertisedDevice* getDevice(uint32_t idx) {
        if (idx >= _devices.size()) return nullptr;
        return &_devices[idx];
    }
private:
    std::vector<BTAdvertisedDevice> _devices;
};

class BluetoothSerial {
public:
    bool begin(const char* localName = nullptr, bool isMaster = false) {
        (void)localName;
        (void)isMaster;
        return true;
    }
    void end() {}
    bool connect(const String& remoteName) { (void)remoteName; return false; }
    bool connect(const BTAddress& remoteAddress) { (void)remoteAddress; return false; }
    bool connect(const uint8_t (&address)[6]) { (void)address; return false; }
    bool connected(uint32_t timeoutMs = 0) { (void)timeoutMs; return false; }
    bool disconnect() { return true; }
    BTScanResults* getScanResults() { return &_results; }
    void discoverClear() {}
    String getBtAddressString() { return String("00:00:00:00:00:00"); }

private:
    BTScanResults _results;
};
