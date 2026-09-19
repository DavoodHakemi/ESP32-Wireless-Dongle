// Mock Network.h — IPAddress + WiFiClient/WiFiServer/WiFiUDP subset used by
// NetworkService. Method behavior is inert; the mock proves compile/link
// parity, not network semantics.
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include "Arduino.h"

class IPAddress {
public:
    IPAddress() = default;
    explicit IPAddress(uint32_t addr) : _addr(addr) {}
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
    : _addr((static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
            (static_cast<uint32_t>(c) << 8) | static_cast<uint32_t>(d)) {}

    uint32_t operator==(const IPAddress& o) const { return _addr == o._addr ? 1u : 0u; }
    uint32_t operator!=(const IPAddress& o) const { return _addr != o._addr ? 1u : 0u; }

    uint8_t operator[](int index) const {
        return static_cast<uint8_t>((_addr >> (24 - index * 8)) & 0xFF);
    }

    String toString() const {
        char text[16]{};
        snprintf(text, sizeof(text), "%u.%u.%u.%u",
                 (*this)[0], (*this)[1], (*this)[2], (*this)[3]);
        return String(text);
    }

private:
    uint32_t _addr = 0;
};

class WiFiClient {
public:
    WiFiClient() = default;
    explicit WiFiClient(int) {}
    explicit operator bool() const { return false; }
    int connect(const IPAddress& ip, uint16_t port) { (void)ip; (void)port; return 0; }
    int connect(const IPAddress& ip, uint16_t port, uint32_t timeoutMs) {
        (void)ip;
        (void)port;
        (void)timeoutMs;
        return 0;
    }
    uint8_t connected() { return 0; }
    int available() { return 0; }
    int read() { return -1; }
    int read(uint8_t* buf, size_t size) { (void)buf; (void)size; return -1; }
    size_t write(const uint8_t* data, size_t size) { (void)data; return size; }
    void setTimeout(uint32_t ms) { (void)ms; }
    IPAddress remoteIP() { return IPAddress(); }
    uint16_t remotePort() { return 0; }
    void stop() {}
};

class WiFiServer {
public:
    explicit WiFiServer(uint16_t port = 0) : _port(port) {}
    void begin(uint16_t port = 0) { if (port) _port = port; }
    WiFiClient accept() { return WiFiClient(); }
    void stop() {}
private:
    uint16_t _port = 0;
};

class WiFiUDP {
public:
    uint8_t begin(uint16_t port) { (void)port; return 1; }
    void stop() {}
    int beginPacket(const IPAddress& ip, uint16_t port) { (void)ip; (void)port; return 1; }
    int endPacket() { return 1; }
    size_t write(const uint8_t* data, size_t size) { (void)data; return size; }
    int parsePacket() { return 0; }
    int read(uint8_t* buf, size_t size) { (void)buf; (void)size; return -1; }
    IPAddress remoteIP() { return IPAddress(); }
    uint16_t remotePort() { return 0; }
};
