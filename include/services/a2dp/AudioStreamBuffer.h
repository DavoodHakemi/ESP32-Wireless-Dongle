#pragma once
#include <cstdint>
#include "core/Types.h"
#include "config/AppConfig.h"
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace dongle::services::a2dp {
struct AudioBufferStatus {
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
};
class AudioStreamBuffer final {
public:
    static constexpr uint32_t CAPACITY=config::AUDIO_RING_CAPACITY;
    bool start(const AudioProfile& profile, uint32_t prebufferBytes);
    void stop();
    bool pushPacket(const uint8_t* payload, uint16_t length);
    uint32_t used() const;
    uint32_t freeBytes() const;
    bool primed() const {
        return _primed;
    }
    bool streaming() const {
        return _running;
    }
    bool loadBlock(uint8_t* out, uint16_t bytes, uint16_t expectedSamples);
    void onUnderrun();
    bool takeUnderrunFlag();
    uint32_t received() const;
    uint32_t dropped() const;
    uint32_t underruns() const;
    const AudioProfile& profile()const{
        return _profile;
    }
    uint16_t blockSamples()const{
        return config::AUDIO_BLOCK_SAMPLES;
    }
    uint16_t blockBytes()const{
        return static_cast<uint16_t>(config::AUDIO_HEADER_BYTES+config::AUDIO_BLOCK_SAMPLES*_profile.channels*2);
    }
    AudioBufferStatus status() const;
private:
    uint8_t _buffer[CAPACITY]{};
    uint32_t _read{
        0
    }
    , _write{
        0
    }
    , _received{
        0
    }
    , _dropped{
        0
    }
    , _underruns{
        0
    };
    bool _underrunPending{
        false
    };
    AudioProfile _profile{};
    uint32_t _prebuffer{
        0
    };
    bool _running{
        false
    };
    bool _primed{
        false
    };
    mutable portMUX_TYPE _mux=portMUX_INITIALIZER_UNLOCKED;
    uint32_t usedUnsafe() const;
};
}
