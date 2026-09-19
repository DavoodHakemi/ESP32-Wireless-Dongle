// Mock ESP32-A2DP v1.8.10 BluetoothA2DPSource.h — API subset used by
// A2DPSourceAdapter. Frame layout matches the real library.
#pragma once

#include <cstdint>
#include <cstddef>
#include "Arduino.h"
#include "mock_esp_common.h"
#include "esp_a2dp_api.h"
#include "esp_gap_bt_api.h"

typedef struct {
    int16_t channel1;
    int16_t channel2;
} Frame;

typedef void (*app_cb_t)(uint16_t event, void* param);

class BluetoothA2DPSource {
public:
    virtual ~BluetoothA2DPSource() = default;

    void set_local_name(const char* name) { (void)name; }
    void set_ssid_callback(bool (*callback)(const char* ssid, esp_bd_addr_t address, int rssi)) {
        (void)callback;
    }
    void set_data_callback(void (*callback)(const uint8_t* data, uint32_t len)) {
        (void)callback;
    }
    void set_data_callback_in_frames(int32_t (*callback)(Frame* frames, int32_t count)) {
        (void)callback;
    }
    void set_on_connection_state_changed(
        void (*callback)(esp_a2d_connection_state_t state, void* param), void* param) {
        (void)callback;
        (void)param;
    }
    void set_on_audio_state_changed(
        void (*callback)(esp_a2d_audio_state_t state, void* param), void* param) {
        (void)callback;
        (void)param;
    }
    void set_volume(uint8_t volume) { (void)volume; }
    void set_auto_reconnect(bool reconnect) { (void)reconnect; }
    void set_auto_reconnect(esp_bd_addr_t address, int maxRetries) {
        (void)address;
        (void)maxRetries;
    }
    void start() {}
    void end() {}
    bool is_discovery_active() { return false; }
    void cancel_discovery() {}
};
