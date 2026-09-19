#pragma once

#include <cstdint>

namespace dongle::config {
constexpr char DEVICE_NAME[] ="ESP32-Wireless-Dongle";
constexpr char A2DP_DEFAULT_TARGET_NAME[] ="QCY T13 ANC2";
constexpr uint16_t MAX_PROTOCOL_PAYLOAD = 4096;
constexpr uint16_t MAX_NETWORK_DATA = 2048;
constexpr uint32_t NETWORK_DEFAULT_TIMEOUT_MS = 5000;
constexpr uint32_t A2DP_CONNECT_TIMEOUT_MS = 60000;
constexpr uint32_t A2DP_MEDIA_CHECK_DELAY_MS = 300;
constexpr uint8_t A2DP_MEDIA_CHECK_RETRIES = 2;
constexpr uint32_t A2DP_DISCONNECT_DEBOUNCE_MS = 1000;
constexpr uint32_t A2DP_TONE_SAMPLE_RATE = 44100;
constexpr uint16_t A2DP_TONE_FREQUENCY_HZ = 1000;
constexpr uint16_t A2DP_TONE_FRAMES = 441;
// 10 ms @ 44.1 kHz
constexpr uint8_t AUDIO_BITS_PER_SAMPLE = 16;
constexpr uint16_t AUDIO_BLOCK_SAMPLES = 256;
constexpr uint16_t AUDIO_HEADER_BYTES = 3;
constexpr uint32_t AUDIO_RING_CAPACITY = 28672;
constexpr uint32_t AUDIO_PREBUFFER_MULTIPLIER = 4;
constexpr uint8_t BT_SCAN_TABLE_SIZE = 32;
constexpr uint16_t BLE_DETAIL_MAX = 240;
}
