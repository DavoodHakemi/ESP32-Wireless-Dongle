// Mock ESP-IDF types shared by all esp_* stub headers.
#pragma once

#include <cstdint>
#include <cstddef>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1

typedef uint8_t esp_bd_addr_t[6];
