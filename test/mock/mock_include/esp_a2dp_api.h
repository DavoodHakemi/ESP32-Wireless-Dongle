// Mock esp_a2dp_api.h
#pragma once
#include "mock_esp_common.h"

typedef enum {
    ESP_A2D_CONNECTION_STATE_DISCONNECTED = 0,
    ESP_A2D_CONNECTION_STATE_CONNECTING,
    ESP_A2D_CONNECTION_STATE_CONNECTED,
} esp_a2d_connection_state_t;

typedef enum {
    ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND = 0,
    ESP_A2D_AUDIO_STATE_STOPPED,
    ESP_A2D_AUDIO_STATE_STARTED,
} esp_a2d_audio_state_t;

typedef enum {
    ESP_A2D_MEDIA_CTRL_NONE = 0,
    ESP_A2D_MEDIA_CTRL_STOP,
    ESP_A2D_MEDIA_CTRL_PAUSE,
    ESP_A2D_MEDIA_CTRL_START,
    ESP_A2D_MEDIA_CTRL_SUSPEND,
    ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY,
} esp_a2d_media_ctrl_t;

esp_err_t esp_a2d_media_ctrl(esp_a2d_media_ctrl_t ctrl);
