// Mock esp_bt.h
#pragma once
#include "mock_esp_common.h"

typedef enum {
    ESP_BT_MODE_IDLE = 0,
    ESP_BT_MODE_BLE,
    ESP_BT_MODE_CLASSIC_BT,
    ESP_BT_MODE_BTDM,
} esp_bt_mode_t;

typedef enum {
    ESP_BT_CONTROLLER_STATUS_IDLE = 0,
    ESP_BT_CONTROLLER_STATUS_INITED,
    ESP_BT_CONTROLLER_STATUS_DEINITED,
    ESP_BT_CONTROLLER_STATUS_ENABLED,
    ESP_BT_CONTROLLER_STATUS_DISABLED,
} esp_bt_controller_status_t;

esp_bt_controller_status_t esp_bt_controller_get_status();
