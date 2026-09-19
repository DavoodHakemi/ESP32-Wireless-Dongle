// Mock BLEDevice.h / BLEScan.h / BLEAdvertisedDevice.h — combined stub of
// the Arduino-ESP32 3.0.7 BLE API subset used by BluetoothLE.
#pragma once

#include <string>
#include <vector>
#include "Arduino.h"

class BLEAddress {
public:
    BLEAddress() = default;
    explicit BLEAddress(const char* addr) : _addr(addr ? addr : "") {}
    std::string toString() const { return _addr; }
private:
    std::string _addr = "00:00:00:00:00:00";
};

class BLEAdvertisedDevice {
public:
    BLEAddress getAddress() const { return _address; }
    int getRSSI() const { return _rssi; }
    bool haveName() const { return !_name.empty(); }
    std::string getName() const { return _name; }
    std::string toString() const { return _toString; }
private:
    friend class BLEScanResults;
    BLEAddress _address;
    int _rssi = -128;
    std::string _name;
    std::string _toString;
};

class BLEScanResults {
public:
    int getCount() const { return static_cast<int>(_devices.size()); }
    BLEAdvertisedDevice getDevice(uint32_t idx) const {
        return idx < _devices.size() ? _devices[idx] : BLEAdvertisedDevice();
    }
private:
    friend class BLEScan;
    std::vector<BLEAdvertisedDevice> _devices;
};

class BLEScan {
public:
    void setActiveScan(bool active) { (void)active; }
    void setInterval(uint16_t intervalMs) { (void)intervalMs; }
    void setWindow(uint16_t windowMs) { (void)windowMs; }
    bool start(uint32_t duration, void (*completeCallback)(BLEScanResults),
               bool isContinue = false) {
        (void)duration;
        (void)completeCallback;
        (void)isContinue;
        return true;
    }
    void stop() {}
    BLEScanResults* getResults() { return &_results; }
    void clearResults() { _results._devices.clear(); }
private:
    BLEScanResults _results;
};

class BLEDevice {
public:
    static void init(const std::string& deviceName) { (void)deviceName; }
    static bool getInitialized() { return true; }
    static BLEScan* getScan() { return &_scan; }
private:
    static BLEScan _scan;
};
