// Mock FreeRTOS.h — only the fixed-width types and queue api used by the
// firmware sources.
#pragma once

#include <cstdint>
#include <cstddef>

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

#define portMAX_DELAY 0xFFFFFFFFu
#define portTICK_PERIOD_MS 1
