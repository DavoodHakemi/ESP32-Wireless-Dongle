#pragma once
#include <BluetoothA2DPSource.h>
#include "core/Result.h"

namespace dongle::services::a2dp {

class A2DPSourceAdapter final {
public:
    using FrameCallback = int32_t (*)(Frame*, int32_t);
    using NameSelector = bool (*)(const char*, esp_bd_addr_t, int);
    using ConnectionCallback = void (*)(esp_a2d_connection_state_t, void*);
    using AudioCallback = void (*)(esp_a2d_audio_state_t, void*);

    void configure(
        const char* localName,
        NameSelector selector,
        FrameCallback frameCallback,
        ConnectionCallback connectionCallback,
        AudioCallback audioCallback,
        void* context);

    Result<void> startByAddress(const uint8_t address[6], int retries = 3);
    Result<void> startByName();

    // Request shutdown without blocking the application task while GAP
    // discovery is still active. finishStop() completes the shutdown once
    // the library reports that discovery has stopped.
    void beginStop();
    bool finishStop();
    bool stopping() const { return _stopPending; }

    // Legacy blocking stop retained for callers that explicitly need a
    // synchronous shutdown. A2DPManager uses beginStop()/finishStop().
    void stop();

    esp_err_t checkMedia();

    bool discoveryActive() {
        return _source.is_discovery_active();
    }

private:
    BluetoothA2DPSource _source;
    bool _stopPending{false};
};

}  // namespace dongle::services::a2dp
