#include "services/bluetooth/BluetoothLE.h"
#include "core/Event.h"
#include "config/AppConfig.h"
#include "config/HardwareConfig.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

namespace dongle::services::bluetooth {
BluetoothLE* BluetoothLE::_instance=nullptr;
void BluetoothLE::clearEntries() {
    for (auto& e:_entries)e=BleScanEntry{};
    _count=0;
}
Result<void> BluetoothLE::begin() {
    if (_initialized)return Result<void>::ok();
    BLEDevice::init("");
    _scanner=BLEDevice::getScan();
    if (!_scanner)return Result<void>::fail(ErrorCode::HardwareError);
    _scanner->setActiveScan(true);
    _scanner->setInterval(100);
    _scanner->setWindow(80);
    _initialized=true;
    return Result<void>::ok();
}
Result<void> BluetoothLE::scan(uint16_t seconds) {
    if (_scanning)return Result<void>::fail(ErrorCode::Busy);
    auto b=begin();
    if (!b.success)return b;
    _seconds=seconds?constrain(seconds, (uint16_t)1, (uint16_t)30):10;
    clearEntries();
    _done=false;
    _instance=this;
    _scanning=true;
    if (xTaskCreatePinnedToCore(worker,"bt_ble_scan", 6144, nullptr, 1, nullptr, config::A2DP_TASK_CORE)!=pdPASS) {
        _scanning=false;
        return Result<void>::fail(ErrorCode::HardwareError);
    }
    return Result<void>::ok();
}
Result<void> BluetoothLE::stop() {
    if (_scanning&&_scanner)_scanner->stop();
    return Result<void>::ok();
}
void BluetoothLE::worker(void*) {
    if (_instance)_instance->runWorker();
    vTaskDelete(nullptr);
}
void BluetoothLE::runWorker() {
    BLEScanResults* results=_scanner?_scanner->start(_seconds, false):nullptr;
    clearEntries();
    if (results) {
        const int found=results->getCount()>MAX_ENTRIES?MAX_ENTRIES:results->getCount();
        for (int i=0;i<found;++i) {
            BLEAdvertisedDevice device=results->getDevice(static_cast<uint32_t>(i));
            auto& e=_entries[_count];
            e.used=true;
            e.address=String(device.getAddress().toString().c_str());
            e.rssi=static_cast<int8_t>(constrain(device.getRSSI(), -127, 0));
            e.name=device.haveName()?String(device.getName().c_str()):String();
            e.details=String(device.toString().c_str());
            if (e.details.length()>config::BLE_DETAIL_MAX)e.details=e.details.substring(0, config::BLE_DETAIL_MAX);
            ++_count;
        }
    }
    if (_scanner)_scanner->clearResults();
    _scanning=false;
    _done=true;
    _instance=nullptr;
}
void BluetoothLE::update() {
    if (!_done)return;
    _done=false;
    for (uint8_t i=0;i<MAX_ENTRIES;++i) {
        const auto&e=_entries[i];
        if (!e.used)continue;
        String address=e.address.length()>17?e.address.substring(0, 17):e.address;
        String name=e.name.length()?e.name:String("(unknown)");
        if (name.length()>64)name=name.substring(0, 64);
        String details=e.details;
        if (details.length()>config::BLE_DETAIL_MAX)details=details.substring(0, config::BLE_DETAIL_MAX);
        uint8_t p[1+17+1+64+1+2+240+1]{};
        uint16_t o=0;
        p[o++]=static_cast<uint8_t>(address.length());
        memcpy(p+o, address.c_str(), address.length());
        o+=address.length();
        p[o++]=static_cast<uint8_t>(name.length());
        memcpy(p+o, name.c_str(), name.length());
        o+=name.length();
        p[o++]=static_cast<uint8_t>(e.rssi);
        const uint16_t dl=details.length();
        memcpy(p+o, &dl, 2);
        o+=2;
        memcpy(p+o, details.c_str(), dl);
        o+=dl;
        String u=name;
        u.toUpperCase();
        p[o++]=(u.indexOf("QCY")>=0||u.indexOf("T13")>=0)?1:0;
        _events.publish({EventType::BluetoothBleDeviceFound, p, o});
    }
    uint8_t c[2]={
        static_cast<uint8_t>(_count&0xFF), static_cast<uint8_t>(_count>>8)
    };
    _events.publish({EventType::BluetoothBleScanDone, c, 2});
}
}
