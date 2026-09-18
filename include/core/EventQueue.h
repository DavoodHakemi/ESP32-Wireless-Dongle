#pragma once

#include <cstdint>
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#include "core/Event.h"

namespace dongle {

class EventQueue final {
public:
    // Scan publishers emit bounded batches. Classic/Wi-Fi publish at most
    // four entries per update plus SCAN_DONE, so nine records are the largest
    // scan burst. Keep substantial headroom while avoiding a large permanent
    // DRAM allocation on ESP32-WROOM-32.
    static constexpr uint8_t CAPACITY = 16;
    static constexpr uint8_t MIN_SCAN_BURST_CAPACITY = 9;
    static_assert(CAPACITY >= MIN_SCAN_BURST_CAPACITY,
        "EventQueue capacity must absorb a bounded scan publication burst");

    // Largest current event payload is the BLE device event:
    // 1+17 address + 1+64 name + 1 RSSI + 2 detail length + 240 details + 1 flag = 327.
    // Keep one byte of headroom for protocol evolution without retaining the
    // previous 336-byte payload allocation in every queue record.
    static constexpr uint16_t MAX_PAYLOAD = 328;

    struct EventRecord {
        EventType type{};
        uint16_t length{0};
        uint8_t payload[MAX_PAYLOAD]{};
    };

    bool enqueue(const Event& event) {
        if (event.length > MAX_PAYLOAD) {
            portENTER_CRITICAL(&_lock);
            ++_dropped;
            portEXIT_CRITICAL(&_lock);
            return false;
        }

        portENTER_CRITICAL(&_lock);

        if (_count >= CAPACITY) {
            ++_dropped;
            portEXIT_CRITICAL(&_lock);
            return false;
        }

        EventRecord& record = _records[_tail];
        record.type = event.type;
        record.length = event.length;

        if (event.payload && event.length) {
            std::memcpy(record.payload, event.payload, event.length);
        }

        _tail = static_cast<uint8_t>((_tail + 1U) % CAPACITY);
        ++_count;

        portEXIT_CRITICAL(&_lock);
        return true;
    }

    bool pop(EventRecord& record) {
        portENTER_CRITICAL(&_lock);

        if (_count == 0) {
            portEXIT_CRITICAL(&_lock);
            return false;
        }

        record = _records[_head];
        _head = static_cast<uint8_t>((_head + 1U) % CAPACITY);
        --_count;

        portEXIT_CRITICAL(&_lock);
        return true;
    }

    uint8_t size() const {
        portENTER_CRITICAL(&_lock);
        const uint8_t value = _count;
        portEXIT_CRITICAL(&_lock);
        return value;
    }

    uint32_t dropped() const {
        portENTER_CRITICAL(&_lock);
        const uint32_t value = _dropped;
        portEXIT_CRITICAL(&_lock);
        return value;
    }

private:
    EventRecord _records[CAPACITY]{};
    uint8_t _head{0};
    uint8_t _tail{0};
    uint8_t _count{0};
    uint32_t _dropped{0};
    mutable portMUX_TYPE _lock = portMUX_INITIALIZER_UNLOCKED;
};

}  // namespace dongle
