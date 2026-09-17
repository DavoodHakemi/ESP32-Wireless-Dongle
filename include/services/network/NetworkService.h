#pragma once
#include <WiFi.h>
#include "core/Result.h"
#include "core/Interfaces.h"

namespace dongle::services::network {
struct Endpoint {
    IPAddress address;
    uint16_t port{
        0
    };
};
class NetworkService final {
public:
    NetworkService(ILogger& logger, IEventSink& events) : _logger(logger), _events(events), _server(1) {}
    Result<Endpoint> tcpConnect(const Endpoint& endpoint);
    Result<uint16_t> tcpSend(const uint8_t* data, uint16_t length);
    Result<uint16_t> tcpReceive(uint16_t timeoutMs, uint16_t maxLength, uint8_t* out);
    void tcpClose();
    Result<void> tcpServerStart(uint16_t port);
    Result<Endpoint> tcpServerAccept(uint16_t timeoutMs);
    Result<uint16_t> tcpServerSend(const uint8_t* data, uint16_t length);
    Result<uint16_t> tcpServerReceive(uint16_t timeoutMs, uint16_t maxLength, uint8_t* out);
    void tcpServerClose();
    Result<void> udpOpen(uint16_t localPort);
    Result<uint16_t> udpSend(const Endpoint& endpoint, const uint8_t* data, uint16_t length);
    Result<uint16_t> udpReceive(uint16_t timeoutMs, uint16_t maxLength, uint8_t* out, Endpoint& remote);
    void udpClose();
    void update();
    void stopAll();
    bool wifiConnected() const {
        return WiFi.status() == WL_CONNECTED;
    }
private:
    ILogger& _logger;
    IEventSink& _events;
    WiFiClient _tcp;
    WiFiServer _server;
    WiFiClient _serverClient;
    WiFiUDP _udp;
    bool _serverRunning{
        false
    };
    bool _udpRunning{
        false
    };
    bool _tcpWasConnected{
        false
    };
    bool _serverWasConnected{
        false
    };
    static bool waitForData(WiFiClient& client, uint16_t timeoutMs);
};
}
