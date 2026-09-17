#pragma once
#include "services/bluetooth/BluetoothClassic.h"
#include "services/bluetooth/BluetoothLE.h"

namespace dongle::services::bluetooth {
class BluetoothManager final {
public:
    BluetoothManager(ILogger& logger, IEventSink& events) : classic(logger, events), ble(logger, events) {}
    Result<void> begin() {
        return classic.begin();
    }
    void update() {
        classic.update();
        ble.update();
    }
    BluetoothClassic classic;
    BluetoothLE ble;
};
}
