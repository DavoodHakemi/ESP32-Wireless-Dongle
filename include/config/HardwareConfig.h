#pragma once

#include <Arduino.h>

namespace dongle::config {
constexpr uint32_t SERIAL_BAUD = 921600UL;
constexpr uint32_t SERIAL_RX_BUFFER = 4096;
constexpr uint32_t SERIAL_TX_BUFFER = 4096;
constexpr uint32_t CPU_FREQUENCY_MHZ = 240;
constexpr uint32_t FLASH_FREQUENCY_HZ = 80000000UL;
// Core 1 is reserved for the Arduino/application loop. A2DP library
// callbacks/tasks are expected to remain compatible with that lifecycle.
constexpr uint8_t A2DP_TASK_CORE = 1;
// Bluetooth Classic/BLE scans use framework-owned asynchronous APIs and do
// not allocate an application scan task.
}