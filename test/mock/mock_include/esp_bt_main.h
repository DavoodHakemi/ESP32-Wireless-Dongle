// Mock esp_bt_main.h
#pragma once
#include "mock_esp_common.h"

typedef enum {
    ESP_BLUEDROID_STATUS_UNINITIALIZED = 0,
    ESP_BLUEDROID_STATUS_INITIALIZED,
    ESP_BLUEDROID_STATUS_ENABLED,
} esp_bluedroid_status_t;

esp_bluedroid_status_t esp_bluedroid_get_status();
