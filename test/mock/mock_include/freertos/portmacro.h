// Mock portmacro.h — critical-section primitives used by A2DP/Bluetooth
// callback guards. On the host mock these are no-ops (single-threaded).
#pragma once

#include <cstdint>

typedef int BaseType_t;
typedef uint32_t portMUX_TYPE;

#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))
#define portDISABLE_INTERRUPTS() ((void)0)
#define portENABLE_INTERRUPTS() ((void)0)
#define portMUX_INITIALIZER_UNLOCKED 0u
