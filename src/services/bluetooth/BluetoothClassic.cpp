#include "services/bluetooth/BluetoothClassic.h"
#include "core/Event.h"
#include "config/AppConfig.h"
#include "config/HardwareConfig.h"
#include <esp_gap_bt_api.h>
#include <cstdio>
#include <cstring>

namespace dongle::services::bluetooth {
BluetoothClassic* BluetoothClassic::_callbackInstance = nullptr;
BluetoothClassic::BluetoothClassic(ILogger& logger, IEventSink& events) : _logger(logger), _events(events) {}
void BluetoothClassic::formatAddress(const uint8_t a[6], char out[18]) {
    snprintf(out, 18,"%02X:%02X:%02X:%02X:%02X:%02X", a[0], a[1], a[2], a[3], a[4], a[5]);
}
bool BluetoothClassic::parseMac(const String& text, uint8_t out[6]) {
    if (text.length()!=17) return false;
    int v[6]{};
    if (sscanf(text.c_str(),"%02x:%02x:%02x:%02x:%02x:%02x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
    for (int i=0;i<6;++i) {
        if (v[i]<0||v[i]>255) return false;
        out[i]=static_cast<uint8_t>(v[i]);
    }
    return true;
}
Result<void> BluetoothClassic::begin() {
    if (_initialized) return Result<void>::ok();
    if (!_serial.begin(config::DEVICE_NAME, true)) return Result<void>::fail(ErrorCode::HardwareError);
    _initialized=true;
    return Result<void>::ok();
}
bool BluetoothClassic::copyNameFromEir(uint8_t* eir, char* out, size_t outSize) {
    if (!eir||!out||outSize==0) return false;
    uint8_t len=0;
    uint8_t* data=esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);
    if (!data) data=esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &len);
    if (!data||!len) return false;
    size_t n=len<outSize?len:outSize-1;
    memcpy(out, data, n);
    out[n]='\0';
    return true;
}
int BluetoothClassic::findEntry(const esp_bd_addr_t address) const {
    for (int i=0;i<MAX_ENTRIES;++i) if (_entries[i].used&&memcmp(_entries[i].address, address, 6)==0)return i;
    return -1;
}
int BluetoothClassic::allocateEntry(const esp_bd_addr_t address) {
    int i=findEntry(address);
    if (i>=0)return i;
    for (i=0;i<MAX_ENTRIES;++i)if (!_entries[i].used) {
        _entries[i]=ClassicScanEntry{};
        _entries[i].used=true;
        memcpy(_entries[i].address, address, 6);
        if (_count<MAX_ENTRIES)++_count;
        return i;
    }
    return -1;
}
void BluetoothClassic::gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t*param) {
    if (_callbackInstance)_callbackInstance->handleGapEvent(event, param);
}
void BluetoothClassic::handleGapEvent(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t*param) {
    if (!param)return;
    if (event==ESP_BT_GAP_DISC_RES_EVT) {
        const int index=allocateEntry(param->disc_res.bda);
        if (index<0)return;
        auto&entry=_entries[index];
        for (int i=0;i<param->disc_res.num_prop;++i) {
            auto&prop=param->disc_res.prop[i];
            if (prop.type==ESP_BT_GAP_DEV_PROP_BDNAME) {
                size_t n=prop.len>64?64:prop.len;
                memcpy(entry.name, prop.val, n);
                entry.name[n]='\0';
                while (n>0&&entry.name[n-1]=='\0') {
                    entry.name[n-1]='\0';
                    --n;
                }
            }
            else if (prop.type==ESP_BT_GAP_DEV_PROP_RSSI&&prop.len>0) {
                entry.rssi=*reinterpret_cast<int8_t*>(prop.val);
                entry.hasRssi=true;
            }
            else if (prop.type==ESP_BT_GAP_DEV_PROP_COD&&prop.len>0) {
                entry.cod=0;
                memcpy(&entry.cod, prop.val, prop.len>4?4:prop.len);
                entry.hasCod=true;
            }
            else if (prop.type==ESP_BT_GAP_DEV_PROP_EIR) {
                copyNameFromEir(reinterpret_cast<uint8_t*>(prop.val), entry.name, sizeof(entry.name));
            }
        }
    }
    else if (event==ESP_BT_GAP_DISC_STATE_CHANGED_EVT&&param->disc_st_chg.state==ESP_BT_GAP_DISCOVERY_STOPPED) {
        _scanning.store(false, std::memory_order_relaxed);
        _scanCompletionPending.store(true, std::memory_order_release);
    }
}
Result<void> BluetoothClassic::scan(uint16_t seconds) {
    if (_scanning || _scanStartPending) {
        return Result<void>::fail(ErrorCode::Busy);
    }

    _scanSeconds=seconds?constrain(seconds, (uint16_t)1, (uint16_t)30):10;

    // Keep the command-response path non-blocking on first use. BluetoothSerial::begin()
    // may require stack initialization time, so defer only the cold-start begin()
    // to update(). Once initialized, the legacy scan start remains synchronous.
    if (!_initialized) {
        _scanStartPending = true;
        return Result<void>::ok();
    }

    return startScanNow();
}

Result<void> BluetoothClassic::startScanNow() {
    _callbackInstance=this;
    memset(_entries, 0, sizeof(_entries));
    _count=0;
    _scanCompletionPending=false;

    if (esp_bt_gap_register_callback(gapCallback)!=ESP_OK) {
        _callbackInstance=nullptr;
        return Result<void>::fail(ErrorCode::HardwareError);
    }

    const uint8_t inq=static_cast<uint8_t>(
        constrain((static_cast<uint32_t>(_scanSeconds)*25U+31U)/32U, 1U, 48U));

    if (esp_bt_gap_start_discovery(
            ESP_BT_INQ_MODE_GENERAL_INQUIRY, inq, 0)!=ESP_OK) {
        _callbackInstance=nullptr;
        return Result<void>::fail(ErrorCode::ScanFailed);
    }

    _scanning=true;
    return Result<void>::ok();
}
Result<void> BluetoothClassic::stopScan() {
    if (_scanStartPending) {
        _scanStartPending=false;
        return Result<void>::ok();
    }
    if (!_scanning)return Result<void>::ok();
    esp_bt_gap_cancel_discovery();
    return Result<void>::ok();
}
void BluetoothClassic::finishScan() {
    for (uint8_t i=0;i<MAX_ENTRIES;++i) {
        const auto&entry=_entries[i];
        if (!entry.used)continue;
        char addr[18];
        formatAddress(entry.address, addr);
        String name=entry.name[0]?String(entry.name):String("(unknown)");
        if (name.length()>64)name=name.substring(0, 64);
        uint8_t p[1+17+1+64+2]{};
        uint16_t o=0;
        p[o++]=17;
        memcpy(p+o, addr, 17);
        o+=17;
        p[o++]=static_cast<uint8_t>(name.length());
        memcpy(p+o, name.c_str(), name.length());
        o+=name.length();
        const int16_t rssi=entry.hasRssi?entry.rssi:-127;
        p[o++]=static_cast<uint8_t>(rssi&0xFF);
        p[o++]=static_cast<uint8_t>(rssi>>8);
        _events.publish({EventType::BluetoothDeviceFound, p, o});
        uint8_t d[1+17+1+64+1+1+4+1+1+2+1]{};
        o=0;
        d[o++]=17;
        memcpy(d+o, addr, 17);
        o+=17;
        d[o++]=static_cast<uint8_t>(name.length());
        memcpy(d+o, name.c_str(), name.length());
        o+=name.length();
        d[o++]=static_cast<uint8_t>(rssi);
        d[o++]=entry.hasCod?1:0;
        uint32_t cod=entry.hasCod?entry.cod:0;
        memcpy(d+o, &cod, 4);
        o+=4;
        const uint8_t major=entry.hasCod?static_cast<uint8_t>(esp_bt_gap_get_cod_major_dev(cod)):0xFF;
        const uint8_t minor=entry.hasCod?static_cast<uint8_t>(esp_bt_gap_get_cod_minor_dev(cod)):0xFF;
        const uint16_t service=entry.hasCod?static_cast<uint16_t>(esp_bt_gap_get_cod_srvc(cod)):0;
        const uint8_t audio=(entry.hasCod&&(esp_bt_gap_get_cod_major_dev(cod)==ESP_BT_COD_MAJOR_DEV_AV||(service&ESP_BT_COD_SRVC_AUDIO)!=0))?1:0;
        d[o++]=major;
        d[o++]=minor;
        memcpy(d+o, &service, 2);
        o+=2;
        d[o++]=audio;
        _events.publish({EventType::BluetoothDeviceDetail, d, o});
    }
    uint8_t c[2]={
        static_cast<uint8_t>(_count&0xFF), static_cast<uint8_t>(_count>>8)
    };
    _events.publish({EventType::BluetoothScanDone, c, 2});
    _callbackInstance=nullptr;
    if (_initialized) {
        _serial.end();
        _initialized=false;
        begin();
    }
}
void BluetoothClassic::update() {
    if (_scanStartPending) {
        _scanStartPending=false;

        if (!_initialized) {
            const auto initialized=begin();
            if (!initialized.success) {
                uint8_t countPayload[2]{};
                _events.publish({EventType::BluetoothScanDone, countPayload, sizeof(countPayload)});
                _logger.error("Bluetooth Classic scan initialization failed");
                return;
            }
        }

        const auto started=startScanNow();
        if (!started.success) {
            uint8_t countPayload[2]{};
            _events.publish({EventType::BluetoothScanDone, countPayload, sizeof(countPayload)});
            _logger.error(
                "Bluetooth Classic scan start failed: error=%d",
                static_cast<int>(started.error));
        }
    }

    if (_scanCompletionPending.exchange(false, std::memory_order_acquire)) {
        finishScan();
        _count=0;
    }
}
Result<void> BluetoothClassic::connect(const String& address) {
    if (!_initialized) {
        auto r=begin();
        if (!r.success)return r;
    }
    uint8_t mac[6];
    if (!parseMac(address, mac))return Result<void>::fail(ErrorCode::InvalidArgument);
    if (_serial.connected())_serial.disconnect();
    _remoteAddress=address;
    _connecting=true;
    uint8_t p[18]={
        17
    };
    memcpy(p+1, address.c_str(), 17);
    _events.publish({EventType::BluetoothConnecting, p, sizeof(p)});
    const bool ok=_serial.connect(mac);
    _connecting=false;
    if (ok||_serial.connected(1000)) {
        _events.publish({EventType::BluetoothConnected, p, sizeof(p)});
        return Result<void>::ok();
    }
    _events.publish({EventType::BluetoothConnectFailed, p, sizeof(p)});
    return Result<void>::fail(ErrorCode::ConnectionFailed);
}
Result<void> BluetoothClassic::disconnect() {
    const bool was=connected();
    const bool ok=!was||_serial.disconnect();
    _connecting=false;
    if (was&&!_serial.connected()) {
        uint8_t p[18]={
            17
        };
        if (_remoteAddress.length()==17) {
            memcpy(p+1, _remoteAddress.c_str(), 17);
            _events.publish({EventType::BluetoothDisconnected, p, sizeof(p)});
        }
    }
    return ok?Result<void>::ok():Result<void>::fail(ErrorCode::InternalError);
}
bool BluetoothClassic::connected() {
    return _initialized&&_serial.connected();
}
String BluetoothClassic::localAddress() {
    return _initialized?_serial.getBtAddressString():String();
}
void BluetoothClassic::suspendForA2dp() {
    if (_scanStartPending) {
        _scanStartPending=false;
    }
    if (_scanning)stopScan();
    if (_serial.connected())_serial.disconnect();
    if (_initialized) {
        _serial.end();
        _initialized=false;
    }
    _callbackInstance=nullptr;
}
void BluetoothClassic::restoreAfterA2dp() {
    if (!_initialized)begin();
}
}
