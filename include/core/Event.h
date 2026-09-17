#pragma once

#include <cstdint>

namespace dongle {

enum class EventType : uint8_t {
    WifiNetworkFound,
    WifiScanDone,
    WifiConnected,
    WifiDisconnected,
    WifiGotIp,
    WifiConnectFailed,

    TcpConnected,
    TcpDisconnected,
    TcpServerConnected,

    BluetoothDeviceFound,
    BluetoothScanDone,
    BluetoothConnecting,
    BluetoothConnected,
    BluetoothConnectFailed,
    BluetoothDisconnected,
    BluetoothDeviceDetail,
    BluetoothBleDeviceFound,
    BluetoothBleScanDone,

    A2dpConnecting,
    A2dpConnected,
    A2dpAudioStarted,
    A2dpAudioStopped,
    A2dpDisconnected,
    A2dpConnectFailed,
    A2dpClassicFound,
    AudioUnderrun,
    SerialLog
};

struct Event {
    EventType type;
    const uint8_t* payload{
        nullptr
    };
    uint16_t length{
        0
    };
};
}
