#include "services/network/NetworkService.h"
#include "core/Event.h"
#include "config/AppConfig.h"

namespace dongle::services::network {
bool NetworkService::waitForData(WiFiClient& client, uint16_t timeoutMs) {
    const uint32_t deadline = millis() + timeoutMs;
    while (static_cast<int32_t>(millis() - deadline) < 0) {
        if (client.available()) return true;
        if (!client.connected()) return false;
        yield();
    }
    return false;
}
Result<Endpoint> NetworkService::tcpConnect(const Endpoint& endpoint) {
    if (!wifiConnected()) return Result<Endpoint>::fail(ErrorCode::NotConnected);
    _tcp.stop();
    _tcp.setTimeout(config::NETWORK_DEFAULT_TIMEOUT_MS);
    if (!_tcp.connect(endpoint.address, endpoint.port, config::NETWORK_DEFAULT_TIMEOUT_MS))
    return Result<Endpoint>::fail(ErrorCode::Timeout);
    _tcpWasConnected = true;
    return Result<Endpoint>::ok(endpoint);
}
Result<uint16_t> NetworkService::tcpSend(const uint8_t* data, uint16_t length) {
    if (!_tcp.connected()) return Result<uint16_t>::fail(ErrorCode::NotConnected);
    return Result<uint16_t>::ok(static_cast<uint16_t>(_tcp.write(data, length)));
}
Result<uint16_t> NetworkService::tcpReceive(uint16_t timeoutMs, uint16_t maxLength, uint8_t* out) {
    if (!_tcp.connected()) return Result<uint16_t>::fail(ErrorCode::NotConnected);
    _tcp.setTimeout(timeoutMs);
    if (!waitForData(_tcp, timeoutMs)) return _tcp.connected() ? Result<uint16_t>::fail(ErrorCode::Timeout)
    : Result<uint16_t>::fail(ErrorCode::NotConnected);
    const int received = _tcp.read(out, maxLength);
    return received >= 0 ? Result<uint16_t>::ok(static_cast<uint16_t>(received))
    : Result<uint16_t>::fail(ErrorCode::InternalError);
}
void NetworkService::tcpClose() {
    _tcp.stop();
    _tcpWasConnected = false;
}
Result<void> NetworkService::tcpServerStart(uint16_t port) {
    _serverClient.stop();
    _server.stop();
    _server.begin(port);
    _serverRunning = true;
    _serverWasConnected = false;
    return Result<void>::ok();
}
Result<Endpoint> NetworkService::tcpServerAccept(uint16_t timeoutMs) {
    if (!_serverRunning) return Result<Endpoint>::fail(ErrorCode::NotInitialized);
    const uint32_t deadline = millis() + timeoutMs;
    while (static_cast<int32_t>(millis() - deadline) < 0) {
        WiFiClient candidate = _server.accept();
        if (candidate && candidate.connected()) {
            _serverClient = candidate;
            _serverClient.setTimeout(config::NETWORK_DEFAULT_TIMEOUT_MS);
            _serverWasConnected = true;
            return Result<Endpoint>::ok({_serverClient.remoteIP(), _serverClient.remotePort()});
        }
        yield();
    }
    return Result<Endpoint>::fail(ErrorCode::Timeout);
}
Result<uint16_t> NetworkService::tcpServerSend(const uint8_t* data, uint16_t length) {
    if (!_serverClient.connected()) return Result<uint16_t>::fail(ErrorCode::NotConnected);
    return Result<uint16_t>::ok(static_cast<uint16_t>(_serverClient.write(data, length)));
}
Result<uint16_t> NetworkService::tcpServerReceive(uint16_t timeoutMs, uint16_t maxLength, uint8_t* out) {
    if (!_serverClient.connected()) return Result<uint16_t>::fail(ErrorCode::NotConnected);
    _serverClient.setTimeout(timeoutMs);
    if (!waitForData(_serverClient, timeoutMs)) return _serverClient.connected() ? Result<uint16_t>::fail(ErrorCode::Timeout)
    : Result<uint16_t>::fail(ErrorCode::NotConnected);
    const int received = _serverClient.read(out, maxLength);
    return received >= 0 ? Result<uint16_t>::ok(static_cast<uint16_t>(received))
    : Result<uint16_t>::fail(ErrorCode::InternalError);
}
void NetworkService::tcpServerClose() {
    _serverClient.stop();
    _server.stop();
    _serverRunning = false;
    _serverWasConnected = false;
}
Result<void> NetworkService::udpOpen(uint16_t localPort) {
    _udp.stop();
    if (!_udp.begin(localPort)) return Result<void>::fail(ErrorCode::InternalError);
    _udpRunning = true;
    return Result<void>::ok();
}
Result<uint16_t> NetworkService::udpSend(const Endpoint& endpoint, const uint8_t* data, uint16_t length) {
    if (!_udpRunning) return Result<uint16_t>::fail(ErrorCode::NotConnected);
    if (_udp.beginPacket(endpoint.address, endpoint.port) != 1) return Result<uint16_t>::fail(ErrorCode::InternalError);
    const size_t written = _udp.write(data, length);
    if (_udp.endPacket() != 1) return Result<uint16_t>::fail(ErrorCode::InternalError);
    return Result<uint16_t>::ok(static_cast<uint16_t>(written));
}
Result<uint16_t> NetworkService::udpReceive(uint16_t timeoutMs, uint16_t maxLength, uint8_t* out, Endpoint& remote) {
    if (!_udpRunning) return Result<uint16_t>::fail(ErrorCode::NotConnected);
    const uint32_t deadline = millis() + timeoutMs;
    int packetSize = 0;
    while (static_cast<int32_t>(millis() - deadline) < 0) {
        packetSize = _udp.parsePacket();
        if (packetSize > 0) break;
        yield();
    }
    if (packetSize <= 0) return Result<uint16_t>::fail(ErrorCode::Timeout);
    remote = {
        _udp.remoteIP(), _udp.remotePort()
    };
    const uint16_t length = static_cast<uint16_t>(packetSize > maxLength ? maxLength : packetSize);
    const int received = _udp.read(out, length);
    return received >= 0 ? Result<uint16_t>::ok(static_cast<uint16_t>(received)) : Result<uint16_t>::fail(ErrorCode::InternalError);
}
void NetworkService::udpClose() {
    _udp.stop();
    _udpRunning = false;
}
void NetworkService::stopAll() {
    tcpClose();
    tcpServerClose();
    udpClose();
}
void NetworkService::update() {
    if (_tcpWasConnected && !_tcp.connected()) {
        _tcp.stop();
        _tcpWasConnected = false;
        _events.publish({EventType::TcpDisconnected, nullptr, 0});
    }
    if (_serverWasConnected && !_serverClient.connected()) {
        _serverClient.stop();
        _serverWasConnected = false;
        _events.publish({EventType::TcpDisconnected, nullptr, 0});
    }
    if (!wifiConnected()) stopAll();
}
}
