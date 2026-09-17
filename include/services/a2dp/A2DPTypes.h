#pragma once
#include <Arduino.h>
#include <cstdint>
#include "core/Types.h"
#include "services/a2dp/A2DPConnection.h"

namespace dongle::services::a2dp {
struct Status {
    bool active{
        false
    };
    bool connecting{
        false
    };
    bool tone{
        false
    };
    uint8_t connectionState{
        0xFF
    };
    uint8_t audioState{
        0xFF
    };
    ConnectionState state{
        ConnectionState::Disconnected
    };
    String target;
};
struct AudioStatus {
    bool streaming{
        false
    };
    bool primed{
        false
    };
    AudioProfile profile{};
    uint32_t used{
        0
    };
    uint32_t capacity{
        0
    };
    uint32_t received{
        0
    };
    uint32_t dropped{
        0
    };
    uint32_t underruns{
        0
    };
    uint32_t callbackCount{
        0
    };
    uint32_t callbackBytes{
        0
    };
    uint32_t maxGapMs{
        0
    };
    uint32_t stalls{
        0
    };
};
}
