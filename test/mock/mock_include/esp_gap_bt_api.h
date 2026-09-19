// Mock esp_gap_bt_api.h — matches the Arduino-ESP32 3.0.7 / ESP-IDF 5.1
// signatures actually used by the firmware sources (COD helpers are static
// inline uint32_t functions in the real framework).
#pragma once
#include "mock_esp_common.h"

#define ESP_BT_CONNECTABLE 0x1
#define ESP_BT_GENERAL_DISCOVERABLE 0x2

#define ESP_BT_COD_SRVC_AUDIO 0x100
#define ESP_BT_COD_MAJOR_DEV_AV 4

typedef enum {
    ESP_BT_INQ_MODE_GENERAL_INQUIRY = 0,
    ESP_BT_INQ_MODE_LIMITED_INQUIRY,
} esp_bt_inq_mode_t;

typedef enum {
    ESP_BT_GAP_DISCOVERY_STARTED = 0,
    ESP_BT_GAP_DISCOVERY_STOPPED,
} esp_bt_gap_discovery_state_t;

esp_err_t esp_bt_gap_start_discovery(esp_bt_inq_mode_t mode, uint8_t inq_len, uint8_t num_rsps);
esp_err_t esp_bt_gap_cancel_discovery();
esp_err_t esp_bt_gap_set_scan_mode(int connectable, int discoverable);

static inline uint32_t esp_bt_gap_get_cod_srvc(uint32_t cod)
{
    return (cod >> 16) & 0xFF;
}

static inline uint32_t esp_bt_gap_get_cod_major_dev(uint32_t cod)
{
    return (cod >> 8) & 0x1F;
}

static inline uint32_t esp_bt_gap_get_cod_minor_dev(uint32_t cod)
{
    return (cod >> 2) & 0x3F;
}
