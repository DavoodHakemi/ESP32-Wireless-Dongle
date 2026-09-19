#include "application/DongleApplication.h"
#include "config/HardwareConfig.h"
#include "config/Version.h"
#include "protocol/Protocol.h"
#include <Arduino.h>

namespace dongle::application {

namespace {
    protocol::EventId toWireEvent(EventType type) {
        using ET = EventType;
        using PE = protocol::EventId;
        switch (type) {
        case ET::WifiNetworkFound:
            return PE::WifiNetworkFound;
        case ET::WifiScanDone:return PE::WifiScanDone;
        case ET::WifiConnected:return PE::WifiConnected;
        case ET::WifiDisconnected:return PE::WifiDisconnected;
        case ET::WifiGotIp:return PE::WifiGotIp;
        case ET::WifiConnectFailed:return PE::WifiConnectFailed;
        case ET::TcpConnected:return PE::TcpConnected;
        case ET::TcpDisconnected:return PE::TcpDisconnected;
        case ET::TcpServerConnected:return PE::TcpServerConnected;
        case ET::BluetoothDeviceFound:return PE::BluetoothDeviceFound;
        case ET::BluetoothScanDone:return PE::BluetoothScanDone;
        case ET::BluetoothConnecting:return PE::BluetoothConnecting;
        case ET::BluetoothConnected:return PE::BluetoothConnected;
        case ET::BluetoothConnectFailed:return PE::BluetoothConnectFailed;
        case ET::BluetoothDisconnected:return PE::BluetoothDisconnected;
        case ET::BluetoothDeviceDetail:return PE::BluetoothDeviceDetail;
        case ET::BluetoothBleDeviceFound:return PE::BluetoothBleDeviceFound;
        case ET::BluetoothBleScanDone:return PE::BluetoothBleScanDone;
        case ET::A2dpConnecting:return PE::A2dpConnecting;
        case ET::A2dpConnected:return PE::A2dpConnected;
        case ET::A2dpAudioStarted:return PE::A2dpAudioStarted;
        case ET::A2dpAudioStopped:return PE::A2dpAudioStopped;
        case ET::A2dpDisconnected:return PE::A2dpDisconnected;
        case ET::A2dpConnectFailed:return PE::A2dpConnectFailed;
        case ET::A2dpClassicFound:return PE::A2dpClassicFound;
        case ET::AudioUnderrun:return PE::AudioUnderrun;
        case ET::SerialLog:return PE::SerialLog;
        }
        return PE::SerialLog;
    }
}
DongleApplication::DongleApplication()
    : _transport(config::SERIAL_BAUD),
      _eventQueue(),
      _codec(_transport),
      _parser(*this),
      _logger(*this),
      _system(_logger),
      _wifi(_logger, *this),
      _network(_logger, *this),
      _bluetooth(_logger, *this),
      _a2dp(_logger, *this, _bluetooth),
      _dispatcher(
          _codec,
          _system,
          _wifi,
          _network,
          _bluetooth,
          _a2dp) {}
bool DongleApplication::begin() {
    if (!_transport.begin()) {
        return false;
    }

    delay(250);

    if (!_wifi.begin().success) {
        return false;
    }

    if (!_a2dp.begin().success) {
        return false;
    }

    _logger.info("Firmware %s initialized", DONGLE_FIRMWARE_VERSION);
    return true;
}
void DongleApplication::update() {
    static uint8_t buffer[2048];
    if (_transport.available()) {
        const size_t n = _transport.read(buffer, sizeof(buffer));
        if (n) {
            _parser.feed(buffer, static_cast<uint16_t>(n));
        }
    }
    _transport.update();
    _wifi.update();
    drainEvents();
    _network.update();
    drainEvents();
    _bluetooth.update();
    drainEvents();
    _bluetooth.postUpdate();
    _a2dp.update();
    drainEvents();
}
void DongleApplication::drainEvents() {
    if (!_protocolReady) {
        return;
    }

    EventQueue::EventRecord event{};
    while (_eventQueue.pop(event)) {
        _codec.sendEvent(
            static_cast<uint8_t>(toWireEvent(event.type)),
            event.payload,
            event.length);
    }
}

void DongleApplication::onCommand(
    const protocol::ProtocolCommand& command) {
    _dispatcher.onCommand(command);

    if (command.id == protocol::CommandId::GetInfo) {
        _protocolReady = true;
    }
}

void DongleApplication::publish(const Event& event) {
    _eventQueue.enqueue(event);
}
}