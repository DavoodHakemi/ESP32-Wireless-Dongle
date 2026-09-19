// Mock esp_heap_caps.h — capability constants match the real IDF subset.
#pragma once
#include <cstddef>
#include "mock_esp_common.h"

#define MALLOC_CAP_EXEC (1 << 0)
#define MALLOC_CAP_32BIT (1 << 1)
#define MALLOC_CAP_8BIT (1 << 2)
#define MALLOC_CAP_DMA (1 << 3)
#define MALLOC_CAP_PID2 (1 << 4)
#define MALLOC_CAP_PID3 (1 << 5)
#define MALLOC_CAP_PID4 (1 << 6)
#define MALLOC_CAP_PID5 (1 << 7)
#define MALLOC_CAP_PID6 (1 << 8)
#define MALLOC_CAP_PID7 (1 << 9)
#define MALLOC_CAP_SPISRAM (1 << 10)
#define MALLOC_CAP_INTERNAL (1 << 11)
#define MALLOC_CAP_DEFAULT (1 << 12)
#define MALLOC_CAP_IRAM_8BIT (1 << 13)
#define MALLOC_CAP_RETENTION (1 << 14)
#define MALLOC_CAP_RTCRAM (1 << 15)

size_t esp_get_free_heap_size();
size_t esp_get_minimum_free_heap_size();
size_t heap_caps_get_free_size(uint32_t caps);
size_t heap_caps_get_largest_free_block(uint32_t caps);
