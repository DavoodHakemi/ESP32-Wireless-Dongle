#pragma once
#include "protocol/CommandParser.h"
#include "protocol/ProtocolCodec.h"
#include "services/system/SystemService.h"
#include "services/wifi/WiFiManager.h"
#include "services/network/NetworkService.h"
#include "services/bluetooth/BluetoothManager.h"
#include "services/a2dp/A2DPManager.h"

namespace dongle::protocol {
class CommandDispatcher final : public ICommandSink {
public:
    CommandDispatcher(ProtocolCodec& codec, services::system::SystemService& system,
        services::wifi::WiFiManager& wifi, services::network::NetworkService& network,
        services::bluetooth::BluetoothManager& bluetooth, services::a2dp::A2DPManager& a2dp)
    : _codec(codec), _system(system), _wifi(wifi), _network(network), _bluetooth(bluetooth), _a2dp(a2dp) {}
    void onCommand(const ProtocolCommand& command) override;
private:
    ProtocolCodec& _codec;
    services::system::SystemService& _system;
    services::wifi::WiFiManager& _wifi;
    services::network::NetworkService& _network;
    services::bluetooth::BluetoothManager& _bluetooth;
    services::a2dp::A2DPManager& _a2dp;
    static uint16_t readU16(const uint8_t*);
    static uint32_t readU32(const uint8_t*);
    static void writeU16(uint8_t*, uint16_t);
    static void writeU32(uint8_t*, uint32_t);
    static StatusCode mapError(ErrorCode error);
    void response(const ProtocolCommand&, StatusCode, const uint8_t* p=nullptr, uint16_t n=0);
    bool requireWifi(const ProtocolCommand&);
};
}
