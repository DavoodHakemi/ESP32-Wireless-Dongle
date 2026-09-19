// Mock WiFi.h — the API subset used by WiFiManager and NetworkService.
#pragma once

#include "Arduino.h"
#include "Network.h"

typedef enum {
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL,
    WL_SCAN_COMPLETED,
    WL_CONNECTED,
    WL_CONNECT_FAILED,
    WL_CONNECTION_LOST,
    WL_DISCONNECTED,
} wl_status_t;

#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED (-2)

typedef enum {
    WIFI_MODE_NULL = 0,
    WIFI_MODE_STA,
    WIFI_MODE_AP,
    WIFI_MODE_APSTA,
} wifi_mode_t;

#define WIFI_STA WIFI_MODE_STA
#define WIFI_AP WIFI_MODE_AP

typedef enum {
    ENC_TYPE_NONE = 7,
    ENC_TYPE_WEP = 5,
    ENC_TYPE_TKIP = 2,
    ENC_TYPE_CCMP = 4,
} wifi_auth_mode_t;

class WiFiClass {
public:
    wl_status_t status() { return _status; }
    void mode(wifi_mode_t mode) { _mode = mode; }
    void setSleep(bool sleep) { _sleep = sleep; }
    wl_status_t begin(const char* ssid, const char* passphrase = nullptr) {
        (void)ssid;
        (void)passphrase;
        return _status;
    }
    void disconnect(bool eraseap = false, bool turnOffWifi = false) {
        (void)eraseap;
        (void)turnOffWifi;
    }
    int scanNetworks(bool async = false, bool show_hidden = false, bool passive = false,
                     uint32_t max_ms_per_chan = 300) {
        (void)async;
        (void)show_hidden;
        (void)passive;
        (void)max_ms_per_chan;
        return 0;
    }
    int scanComplete() { return -2; }
    void scanDelete() {}
    int32_t RSSI(uint32_t i = 0) { (void)i; return -100; }
    String SSID(uint32_t i = 0) { (void)i; return String(); }
    wifi_auth_mode_t encryptionType(uint32_t i) { (void)i; return ENC_TYPE_NONE; }
    int32_t channel(uint32_t i) { (void)i; return 1; }
    IPAddress localIP() { return IPAddress(); }

private:
    wl_status_t _status = WL_DISCONNECTED;
    wifi_mode_t _mode = WIFI_MODE_NULL;
    bool _sleep = false;
};

extern WiFiClass WiFi;
