#pragma once
#include <cstdint>

namespace dongle::protocol {
enum class CommandId : uint8_t {
    GetInfo = 0x01, GetVersion = 0x02, Ping = 0x03, Reset = 0x04, SetBaud = 0x05,
    WifiScan = 0x10, WifiConnect = 0x11, WifiDisconnect = 0x12, WifiStatus = 0x13,
    TcpConnect = 0x20, TcpSend = 0x21, TcpReceive = 0x22, TcpClose = 0x23,
    TcpServerStart = 0x24, TcpServerAccept = 0x25, TcpServerSend = 0x26,
    TcpServerReceive = 0x27, TcpServerClose = 0x28,
    UdpOpen = 0x29, UdpSend = 0x2A, UdpReceive = 0x2B, UdpClose = 0x2C,
    BluetoothInit = 0x40, BluetoothGetInfo = 0x41, BluetoothScanStart = 0x42,
    BluetoothScanStop = 0x43, BluetoothStatus = 0x44, BluetoothConnect = 0x45,
    BluetoothDisconnect = 0x46, BluetoothBleScanStart = 0x47, BluetoothBleScanStop = 0x48,
    A2dpGetInfo = 0x60, A2dpConnect = 0x61, A2dpTestTone = 0x62,
    A2dpStopTone = 0x63, A2dpDisconnect = 0x64, A2dpStatus = 0x65,
    A2dpConnectName = 0x66, A2dpConnectAuto = 0x67, A2dpClearCache = 0x68,
    A2dpGetCache = 0x69, AudioStart = 0x6A, AudioStop = 0x6B,
    AudioStatus = 0x6C, AudioData = 0x6D
};

enum class EventId : uint8_t {
    WifiNetworkFound = 0x14, WifiScanDone = 0x15, WifiConnected = 0x16,
    WifiDisconnected = 0x17, WifiGotIp = 0x18, WifiConnectFailed = 0x19,
    TcpConnected = 0x30, TcpDisconnected = 0x31, TcpServerConnected = 0x32,
    BluetoothDeviceFound = 0x50, BluetoothScanDone = 0x51, BluetoothConnecting = 0x52,
    BluetoothConnected = 0x53, BluetoothConnectFailed = 0x54, BluetoothDisconnected = 0x55,
    BluetoothDeviceDetail = 0x56, BluetoothBleDeviceFound = 0x57, BluetoothBleScanDone = 0x58,
    A2dpConnecting = 0x70, A2dpConnected = 0x71, A2dpAudioStarted = 0x72,
    A2dpAudioStopped = 0x73, A2dpDisconnected = 0x74, A2dpConnectFailed = 0x75,
    A2dpClassicFound = 0x76, AudioUnderrun = 0x77, SerialLog = 0x78
};
}
