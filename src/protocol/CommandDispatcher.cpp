#include "protocol/CommandDispatcher.h"
#include "config/AppConfig.h"
#include "config/HardwareConfig.h"
#include "core/Error.h"
#include <cstring>

namespace dongle::protocol {
uint16_t CommandDispatcher::readU16(const uint8_t*p) {
    return static_cast<uint16_t>(p[0])|(static_cast<uint16_t>(p[1])<<8);
}
uint32_t CommandDispatcher::readU32(const uint8_t*p) {
    return static_cast<uint32_t>(p[0])|(static_cast<uint32_t>(p[1])<<8)|(static_cast<uint32_t>(p[2])<<16)|(static_cast<uint32_t>(p[3])<<24);
}
void CommandDispatcher::writeU16(uint8_t*p, uint16_t v) {
    p[0]=static_cast<uint8_t>(v&0xFF);
    p[1]=static_cast<uint8_t>(v>>8);
}
void CommandDispatcher::writeU32(uint8_t*p, uint32_t v) {
    p[0]=static_cast<uint8_t>(v&0xFF);
    p[1]=static_cast<uint8_t>((v>>8)&0xFF);
    p[2]=static_cast<uint8_t>((v>>16)&0xFF);
    p[3]=static_cast<uint8_t>((v>>24)&0xFF);
}
StatusCode CommandDispatcher::mapError(ErrorCode e) {
    switch (e) {
    case ErrorCode::None:return StatusCode::Success;
    case ErrorCode::InvalidArgument:return StatusCode::InvalidParameter;
    case ErrorCode::Busy:return StatusCode::Busy;
    case ErrorCode::NotConnected:return StatusCode::NotConnected;
    case ErrorCode::Timeout:return StatusCode::Timeout;
    case ErrorCode::Unsupported:return StatusCode::NotSupported;
    default:return StatusCode::InternalError;
    }
}
void CommandDispatcher::response(const ProtocolCommand&c, StatusCode s, const uint8_t*p, uint16_t n) {
    _codec.sendResponse(static_cast<uint8_t>(c.id), c.sequence, s, p, n);
}
bool CommandDispatcher::requireWifi(const ProtocolCommand&c) {
    if (_network.wifiConnected())return true;
    response(c, StatusCode::NotConnected);
    return false;
}
void CommandDispatcher::onCommand(const ProtocolCommand& c) {
    switch (c.id) {
    case CommandId::Ping: {
            const uint8_t pong[]={
                'P','O','N','G'
            };
            response(c, StatusCode::Success, pong, 4);
            break;
        }
    case CommandId::GetVersion: {
            const String v=_system.version();
            response(c, StatusCode::Success, reinterpret_cast<const uint8_t*>(v.c_str()), static_cast<uint16_t>(v.length()));
            break;
        }
    case CommandId::GetInfo: {
            uint8_t p[39]{};
            const uint16_t n=_system.getInfo(p, sizeof(p));
            response(c, n?StatusCode::Success:StatusCode::InternalError, p, n);
            break;
        }
    case CommandId::Reset: response(c, StatusCode::Success);
        _system.reset();
        break;
    case CommandId::SetBaud: if (c.length!=4) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        response(c, mapError(_system.setBaud(readU32(c.payload)).error));
        break;
    case CommandId::WifiScan: {
            auto r=_wifi.scan();
            response(c, mapError(r.error));
            break;
        }
    case CommandId::WifiConnect: {
            if (c.length<2) {
                response(c, StatusCode::InvalidParameter);
                break;
            }
            uint16_t o=0;
            if (o>=c.length) {
                response(c, StatusCode::InvalidParameter);
                break;
            }
            const uint8_t sl=c.payload[o++];
            if (sl>32||o+sl>=c.length+1) {
                response(c, StatusCode::InvalidParameter);
                break;
            }
            String ssid;
            for (uint8_t i=0;i<sl;++i)ssid+=(char)c.payload[o+i];
            o+=sl;
            if (o>=c.length) {
                response(c, StatusCode::InvalidParameter);
                break;
            }
            const uint8_t pl=c.payload[o++];
            if (pl>64||o+pl!=c.length) {
                response(c, StatusCode::InvalidParameter);
                break;
            }
            String pass;
            for (uint8_t i=0;i<pl;++i)pass+=(char)c.payload[o+i];
            response(c, mapError(_wifi.connect(ssid, pass).error));
            break;
        }
    case CommandId::WifiDisconnect: response(c, mapError(_wifi.disconnect().error));
        break;
    case CommandId::WifiStatus: {
            auto s=_wifi.status();
            uint8_t p[40]{};
            p[0]=s.state;
            p[1]=static_cast<uint8_t>(s.rssi&0xFF);
            p[2]=static_cast<uint8_t>(s.rssi>>8);
            p[3]=s.ip[0];
            p[4]=s.ip[1];
            p[5]=s.ip[2];
            p[6]=s.ip[3];
            String ss=s.ssid;
            if (ss.length()>32)ss=ss.substring(0, 32);
            p[7]=static_cast<uint8_t>(ss.length());
            memcpy(p+8, ss.c_str(), ss.length());
            response(c, StatusCode::Success, p, static_cast<uint16_t>(8+ss.length()));
            break;
        }
    case CommandId::TcpConnect: {
            if (!requireWifi(c)||c.length!=6) {
                if (c.length!=6&&_network.wifiConnected())response(c, StatusCode::InvalidParameter);
                break;
            }
            services::network::Endpoint e{
                IPAddress(c.payload[0], c.payload[1], c.payload[2], c.payload[3]), readU16(c.payload+4)
            };
            auto r=_network.tcpConnect(e);
            if (!r.success) {
                response(c, mapError(r.error));
                break;
            }
            uint8_t p[6]={
                e.address[0], e.address[1], e.address[2], e.address[3]
            };
            writeU16(p+4, e.port);
            response(c, StatusCode::Success, p, 6);
            break;
        }
    case CommandId::TcpSend: {
            if (!requireWifi(c))break;
            if (c.length>config::MAX_NETWORK_DATA) {
                response(c, StatusCode::InvalidParameter);
                break;
            }
            auto r=_network.tcpSend(c.payload, c.length);
            uint8_t p[2]{};
            writeU16(p, r.success?r.value:0);
            response(c, mapError(r.error), p, 2);
            break;
        }
    case CommandId::TcpReceive: {
            if (!requireWifi(c)||c.length!=4) {
                if (c.length!=4&&_network.wifiConnected())response(c, StatusCode::InvalidParameter);
                break;
            }
            const uint16_t timeout=readU16(c.payload), maxLen=readU16(c.payload+2);
            if (!maxLen||maxLen>config::MAX_NETWORK_DATA) {
                response(c, StatusCode::InvalidParameter);
                break;
            }
            uint8_t b[config::MAX_NETWORK_DATA];
            auto r=_network.tcpReceive(timeout?timeout:1, maxLen, b);
            response(c, mapError(r.error), b, r.success?r.value:0);
            break;
        }
    case CommandId::TcpClose:_network.tcpClose();
        response(c, StatusCode::Success);
        break;
    case CommandId::TcpServerStart:if (!requireWifi(c))break;
        if (c.length!=2) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        response(c, mapError(_network.tcpServerStart(readU16(c.payload)).error));
        break;
    case CommandId::TcpServerAccept:if (!requireWifi(c))break;
        if (c.length!=2) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        {
            auto r=_network.tcpServerAccept(readU16(c.payload));
            if (!r.success) {
                response(c, mapError(r.error));
                break;
            }
            uint8_t p[6]={
                r.value.address[0], r.value.address[1], r.value.address[2], r.value.address[3]
            };
            writeU16(p+4, r.value.port);
            response(c, StatusCode::Success, p, 6);
            break;
        }
    case CommandId::TcpServerSend:if (!requireWifi(c))break;
        if (c.length>config::MAX_NETWORK_DATA) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        {
            auto r=_network.tcpServerSend(c.payload, c.length);
            uint8_t p[2]{};
            writeU16(p, r.success?r.value:0);
            response(c, mapError(r.error), p, 2);
            break;
        }
    case CommandId::TcpServerReceive:if (!requireWifi(c))break;
        if (c.length!=4) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        {
            const uint16_t t=readU16(c.payload), m=readU16(c.payload+2);
            if (!m||m>config::MAX_NETWORK_DATA) {
                response(c, StatusCode::InvalidParameter);
                break;
            }
            uint8_t b[config::MAX_NETWORK_DATA];
            auto r=_network.tcpServerReceive(t?t:1, m, b);
            response(c, mapError(r.error), b, r.success?r.value:0);
            break;
        }
    case CommandId::TcpServerClose:_network.tcpServerClose();
        response(c, StatusCode::Success);
        break;
    case CommandId::UdpOpen:if (!requireWifi(c))break;
        if (c.length!=2) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        response(c, mapError(_network.udpOpen(readU16(c.payload)).error));
        break;
    case CommandId::UdpSend:if (!requireWifi(c))break;
        if (c.length<6||c.length-6>config::MAX_NETWORK_DATA) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        {
            services::network::Endpoint e{
                IPAddress(c.payload[0], c.payload[1], c.payload[2], c.payload[3]), readU16(c.payload+4)
            };
            auto r=_network.udpSend(e, c.payload+6, c.length-6);
            uint8_t p[2]{};
            writeU16(p, r.success?r.value:0);
            response(c, mapError(r.error), p, 2);
            break;
        }
    case CommandId::UdpReceive:if (!requireWifi(c))break;
        if (c.length!=4) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        {
            const uint16_t t=readU16(c.payload), m=readU16(c.payload+2);
            if (!m||m>config::MAX_NETWORK_DATA) {
                response(c, StatusCode::InvalidParameter);
                break;
            }
            services::network::Endpoint remote;
            uint8_t b[6+config::MAX_NETWORK_DATA];
            auto r=_network.udpReceive(t?t:1, m, b+6, remote);
            if (!r.success) {
                response(c, mapError(r.error));
                break;
            }
            b[0]=remote.address[0];
            b[1]=remote.address[1];
            b[2]=remote.address[2];
            b[3]=remote.address[3];
            writeU16(b+4, remote.port);
            response(c, StatusCode::Success, b, static_cast<uint16_t>(6+r.value));
            break;
        }
    case CommandId::UdpClose:_network.udpClose();
        response(c, StatusCode::Success);
        break;
    case CommandId::BluetoothInit:response(c, mapError(_bluetooth.begin().error));
        break;
    case CommandId::BluetoothGetInfo:{
            if (_a2dp.status().active||_a2dp.status().connecting) {
                response(c, StatusCode::Busy);
                break;
            }
            if (!_bluetooth.classic.initialized()&&!_bluetooth.classic.begin().success) {
                response(c, StatusCode::InternalError);
                break;
            }
            const String name=config::DEVICE_NAME;
            const String mac=_bluetooth.classic.localAddress();
            uint8_t p[1+32+1+17+3]{};
            uint16_t o=0;
            p[o++]=static_cast<uint8_t>(name.length());
            memcpy(p+o, name.c_str(), name.length());
            o+=name.length();
            p[o++]=static_cast<uint8_t>(mac.length());
            if (mac.length())memcpy(p+o, mac.c_str(), mac.length());
            o+=mac.length();
            p[o++]=1;
            p[o++]=_bluetooth.classic.scanning();
            p[o++]=_bluetooth.classic.connected();
            response(c, StatusCode::Success, p, o);
            break;
        }
    case CommandId::BluetoothStatus:{
            uint8_t p[3]={
                static_cast<uint8_t>(_bluetooth.classic.initialized()), static_cast<uint8_t>(_bluetooth.classic.scanning()), static_cast<uint8_t>(_bluetooth.classic.connected())
            };
            response(c, StatusCode::Success, p, 3);
            break;
        }
    case CommandId::BluetoothScanStart:if (_a2dp.status().active||_a2dp.status().connecting||_bluetooth.ble.scanning()) {
            response(c, StatusCode::Busy);
            break;
        }
        if (c.length!=0&&c.length!=2) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        response(c, mapError(_bluetooth.classic.scan(c.length?readU16(c.payload):10).error));
        break;
    case CommandId::BluetoothScanStop:response(c, mapError(_bluetooth.classic.stopScan().error));
        break;
    case CommandId::BluetoothConnect:if (_a2dp.status().active||_a2dp.status().connecting||c.length!=17) {
            response(c, _a2dp.status().active||_a2dp.status().connecting?StatusCode::Busy:StatusCode::InvalidParameter);
            break;
        }
        {
            String a;
            for (uint8_t i=0;i<17;++i)a+=(char)c.payload[i];
            response(c, mapError(_bluetooth.classic.connect(a).error));
            break;
        }
    case CommandId::BluetoothDisconnect:response(c, mapError(_bluetooth.classic.disconnect().error));
        break;
    case CommandId::BluetoothBleScanStart:if (_a2dp.status().active||_a2dp.status().connecting||_bluetooth.classic.scanning()) {
            response(c, StatusCode::Busy);
            break;
        }
        if (c.length!=0&&c.length!=2) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        response(c, mapError(_bluetooth.ble.scan(c.length?readU16(c.payload):10).error));
        break;
    case CommandId::BluetoothBleScanStop:response(c, mapError(_bluetooth.ble.stop().error));
        break;
    case CommandId::A2dpGetInfo:{
            auto s=_a2dp.status();
            uint8_t p[23]{};
            p[0]=s.active;
            p[1]=s.connecting;
            p[2]=s.tone;
            p[3]=s.connectionState;
            p[4]=s.audioState;
            p[5]=s.target.length()==17?17:0;
            if (p[5])memcpy(p+6, s.target.c_str(), 17);
            response(c, StatusCode::Success, p, sizeof(p));
            break;
        }
    case CommandId::A2dpConnect:if (c.length!=17) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        {
            String a;
            for (uint8_t i=0;i<17;++i)a+=(char)c.payload[i];
            response(c, mapError(_a2dp.connectByAddress(a).error));
            break;
        }
    case CommandId::A2dpConnectName:if (c.length==0||c.length>64) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        {
            String n;
            for (uint16_t i=0;i<c.length;++i)n+=(char)c.payload[i];
            response(c, mapError(_a2dp.connectByName(n).error));
            break;
        }
    case CommandId::A2dpConnectAuto:if (c.length) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        response(c, mapError(_a2dp.connectAuto().error));
        break;
    case CommandId::A2dpClearCache:if (c.length) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        _a2dp.clearCache();
        response(c, StatusCode::Success);
        break;
    case CommandId::A2dpGetCache:{
            String m=_a2dp.cachedMac(), n=_a2dp.cachedName();
            if (m.length()!=17)m="";
            if (n.length()>16)n=n.substring(0, 16);
            uint8_t p[35]{};
            p[0]=m.length();
            if (m.length())memcpy(p+1, m.c_str(), 17);
            p[18]=n.length();
            if (n.length())memcpy(p+19, n.c_str(), n.length());
            response(c, StatusCode::Success, p, sizeof(p));
            break;
        }
    case CommandId::A2dpTestTone:response(c, mapError(_a2dp.startTone().error));
        break;
    case CommandId::A2dpStopTone:response(c, mapError(_a2dp.stopTone().error));
        break;
    case CommandId::A2dpDisconnect:response(c, mapError(_a2dp.disconnect().error));
        break;
    case CommandId::A2dpStatus:{
            auto s=_a2dp.status();
            uint8_t p[5]={
                static_cast<uint8_t>(s.active), static_cast<uint8_t>(s.connecting), static_cast<uint8_t>(s.tone), s.connectionState, s.audioState
            };
            response(c, StatusCode::Success, p, 5);
            break;
        }
    case CommandId::AudioStart:if (c.length!=6) {
            response(c, StatusCode::InvalidParameter);
            break;
        }
        {
            AudioProfile p{
                readU32(c.payload), c.payload[4], c.payload[5]
            };
            response(c, mapError(_a2dp.startAudio(p).error));
            break;
        }
    case CommandId::AudioStop:response(c, mapError(_a2dp.stopAudio().error));
        break;
    case CommandId::AudioData:if (c.length<5||c.length>config::MAX_PROTOCOL_PAYLOAD) {
            break;
        }
        (void)_a2dp.pushAudioData(c.payload, c.length);
        break;
    case CommandId::AudioStatus:{
            auto s=_a2dp.audioStatus();
            uint8_t p[36]{};
            p[0]=s.streaming;
            p[1]=s.primed;
            writeU32(p+2, s.profile.sampleRate);
            p[6]=s.profile.channels;
            p[7]=s.profile.bits;
            writeU32(p+8, s.used);
            writeU32(p+12, s.capacity);
            writeU32(p+16, s.received);
            writeU32(p+20, s.dropped);
            writeU32(p+24, s.underruns);
            writeU32(p+28, s.callbackCount);
            writeU32(p+32, s.callbackBytes);
            response(c, StatusCode::Success, p, sizeof(p));
            break;
        }
    default:response(c, StatusCode::InvalidCommand);
        break;
    }
}
}
