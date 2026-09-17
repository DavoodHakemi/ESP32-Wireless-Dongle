#pragma once

#include <Arduino.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace dongle {

struct MacAddress {
    uint8_t bytes[6]{
        0, 0, 0, 0, 0, 0
    };

    bool isValid() const {
        return bytes[0] || bytes[1] || bytes[2] || bytes[3] || bytes[4] || bytes[5];
    }

    String toString() const {
        char text[18];
        snprintf(text, sizeof(text),"%02X:%02X:%02X:%02X:%02X:%02X",
            bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
        return String(text);
    }
};

struct IPv4Address {
    uint8_t bytes[4]{
        0, 0, 0, 0
    };
    String toString() const {
        char text[16];
        snprintf(text, sizeof(text),"%u.%u.%u.%u",
            bytes[0], bytes[1], bytes[2], bytes[3]);
        return String(text);
    }
};

struct BluetoothDevice {
    MacAddress address;
    String name;
    int8_t rssi{
        -127
    };
    uint32_t cod{
        0
    };
    bool hasCod{
        false
    };
    bool hasRssi{
        false
    };
};

struct WiFiNetwork {
    String ssid;
    int16_t rssi{
        -127
    };
    uint8_t channel{
        0
    };
    uint8_t auth{
        0
    };
};

struct AudioProfile {
    uint32_t sampleRate{
        22050
    };
    uint8_t channels{
        1
    };
    uint8_t bits{
        16
    };

    bool operator==(const AudioProfile& other) const {
        return sampleRate == other.sampleRate &&
        channels == other.channels &&
        bits == other.bits;
    }
};
}
