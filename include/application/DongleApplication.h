#pragma once
#include "transport/SerialTransport.h"
#include "core/EventQueue.h"
#include "protocol/ProtocolCodec.h"
#include "protocol/CommandParser.h"
#include "protocol/CommandDispatcher.h"
#include "utils/Logger.h"
#include "services/system/SystemService.h"
#include "services/wifi/WiFiManager.h"
#include "services/network/NetworkService.h"
#include "services/bluetooth/BluetoothManager.h"
#include "services/a2dp/A2DPManager.h"

namespace dongle::application {

class DongleApplication final : public protocol::ICommandSink, public IEventSink {
public:
    DongleApplication();
    bool begin();
    void update();
    void onCommand(const protocol::ProtocolCommand&) override;
    void publish(const Event&) override;

private:
    transport::SerialTransport _transport;
    EventQueue _eventQueue;
    protocol::ProtocolCodec _codec;
    protocol::CommandParser _parser;
    utils::Logger _logger;
    services::system::SystemService _system;
    services::wifi::WiFiManager _wifi;
    services::network::NetworkService _network;
    services::bluetooth::BluetoothManager _bluetooth;
    services::a2dp::A2DPManager _a2dp;
    protocol::CommandDispatcher _dispatcher;
    bool _protocolReady{false};
};

}
