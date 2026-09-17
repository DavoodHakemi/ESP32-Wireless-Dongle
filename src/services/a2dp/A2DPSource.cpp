#include "services/a2dp/A2DPSource.h"

#include <esp_a2dp_api.h>
#include <cstring>

namespace dongle::services::a2dp {

void A2DPSourceAdapter::configure(
    const char* localName,
    NameSelector selector,
    FrameCallback frameCallback,
    ConnectionCallback connectionCallback,
    AudioCallback audioCallback,
    void* context) {
    _source.set_local_name(localName);
    _source.set_ssid_callback(selector);
    _source.set_data_callback(nullptr);
    _source.set_data_callback_in_frames(frameCallback);
    _source.set_on_connection_state_changed(connectionCallback, context);
    _source.set_on_audio_state_changed(audioCallback, context);
    _source.set_volume(127);
}

Result<void> A2DPSourceAdapter::startByAddress(
    const uint8_t address[6],
    int retries) {
    if (!address) {
        return Result<void>::fail(ErrorCode::InvalidArgument);
    }

    // ESP32-A2DP creates and owns its own application task internally.
    // Keep start() on the dongle application loop, matching the previously
    // working Arduino implementation and keeping A2DP lifecycle operations
    // serialized with the rest of the Bluetooth subsystem.
    _source.set_auto_reconnect(address, retries > 0 ? retries : 3);
    _source.start();
    return Result<void>::ok();
}

Result<void> A2DPSourceAdapter::startByName() {
    // The library performs the Classic inquiry asynchronously after start().
    // No extra wrapper task is required here; start() already dispatches into
    // the library-owned Bluetooth application task.
    _source.set_auto_reconnect(false);
    _source.start();
    return Result<void>::ok();
}

void A2DPSourceAdapter::stop() {
    // cancel_discovery() sets the library's is_end flag before end() waits for
    // the GAP discovery-stopped callback. This is important for recovery to
    // BluetoothSerial and later Classic/BLE scans.
    if (_source.is_discovery_active()) {
        _source.cancel_discovery();
    }

    _source.end();
}

esp_err_t A2DPSourceAdapter::checkMedia() {
    return esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
}

}  // namespace dongle::services::a2dp
