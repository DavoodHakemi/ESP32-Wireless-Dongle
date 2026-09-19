// Mock runtime definitions: provides the framework globals and esp_* stub
// implementations referenced by the firmware sources. Host-side only — this
// file is never compiled into the real ESP32 firmware.
#include "Arduino.h"
#include "ESP.h"
#include "WiFi.h"
#include "BLEDevice.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_heap_caps.h"

#include <chrono>
#include <thread>

HardwareSerial Serial;
WiFiClass WiFi;
EspClass ESP;
BLEScan BLEDevice::_scan;

uint32_t millis() {
    using namespace std::chrono;
    const auto now = steady_clock::now().time_since_epoch();
    return static_cast<uint32_t>(duration_cast<milliseconds>(now).count());
}

uint32_t micros() {
    using namespace std::chrono;
    const auto now = steady_clock::now().time_since_epoch();
    return static_cast<uint32_t>(duration_cast<microseconds>(now).count());
}

void delay(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void delayMicroseconds(uint32_t us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

esp_bt_controller_status_t esp_bt_controller_get_status() {
    return ESP_BT_CONTROLLER_STATUS_ENABLED;
}

esp_bluedroid_status_t esp_bluedroid_get_status() {
    return ESP_BLUEDROID_STATUS_ENABLED;
}

esp_err_t esp_bt_gap_start_discovery(esp_bt_inq_mode_t mode, uint8_t inq_len,
                                     uint8_t num_rsps) {
    (void)mode;
    (void)inq_len;
    (void)num_rsps;
    return ESP_OK;
}

esp_err_t esp_bt_gap_cancel_discovery() {
    return ESP_OK;
}

esp_err_t esp_bt_gap_set_scan_mode(int connectable, int discoverable) {
    (void)connectable;
    (void)discoverable;
    return ESP_OK;
}

esp_err_t esp_a2d_media_ctrl(esp_a2d_media_ctrl_t ctrl) {
    (void)ctrl;
    return ESP_OK;
}

size_t esp_get_free_heap_size() {
    return 200000;
}

size_t esp_get_minimum_free_heap_size() {
    return 150000;
}

size_t heap_caps_get_free_size(uint32_t caps) {
    (void)caps;
    return 180000;
}

size_t heap_caps_get_largest_free_block(uint32_t caps) {
    (void)caps;
    return 100000;
}

void yield() {}
