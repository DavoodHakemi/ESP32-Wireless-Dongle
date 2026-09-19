// Mock Arduino.h for host-side compile/link verification of the dongle
// firmware sources (verification level 2 fallback when PlatformIO is not
// available). Provides only the API subset used by src/ and include/.
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <chrono>

// The real Arduino-ESP32 core includes Esp.h from Arduino.h; mirror that so
// sources relying on ESP.getEfuseMac() etc. compile identically in both.
#include "ESP.h"

using byte = uint8_t;
using boolean = bool;

#define PI 3.1415926535897932384626433832795
#define SERIAL_8N1 0x800001c
#define constrain(amt, low, high) \
    ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void yield();

class String {
public:
    String() = default;
    String(const char* s) : _s(s ? s : "") {}
    String(char c) : _s(1, c) {}
    String(int value) : _s(std::to_string(value)) {}
    String(unsigned value) : _s(std::to_string(value)) {}
    String(long value) : _s(std::to_string(value)) {}
    String(unsigned long value) : _s(std::to_string(value)) {}
    String(float value) : _s(std::to_string(value)) {}
    String(double value) : _s(std::to_string(value)) {}

    const char* c_str() const { return _s.c_str(); }
    unsigned int length() const { return static_cast<unsigned int>(_s.size()); }
    char charAt(unsigned int i) const { return _s[i]; }
    char operator[](unsigned int i) const { return _s[i]; }
    char& operator[](unsigned int i) { return _s[i]; }

    bool startsWith(const String& p) const {
        return _s.rfind(p._s, 0) == 0;
    }
    bool endsWith(const String& p) const {
        return _s.size() >= p._s.size() &&
               _s.compare(_s.size() - p._s.size(), p._s.size(), p._s) == 0;
    }
    int indexOf(const String& needle) const {
        const std::size_t pos = _s.find(needle._s);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }
    int indexOf(char needle) const {
        const std::size_t pos = _s.find(needle);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }
    String substring(unsigned int from) const {
        return from < _s.size() ? String(_s.substr(from).c_str()) : String();
    }
    String substring(unsigned int from, unsigned int to) const {
        if (from >= _s.size() || to <= from) return String();
        return String(_s.substr(from, to - from).c_str());
    }
    void trim() {
        const auto first = _s.find_first_not_of(" \t\r\n");
        const auto last = _s.find_last_not_of(" \t\r\n");
        _s = first == std::string::npos ? "" : _s.substr(first, last - first + 1);
    }
    void toUpperCase() {
        for (char& c : _s) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }
    void toLowerCase() {
        for (char& c : _s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    long toInt() const { return std::strtol(_s.c_str(), nullptr, 10); }
    float toFloat() const { return std::strtof(_s.c_str(), nullptr); }

    String& operator+=(const String& o) { _s += o._s; return *this; }
    String& operator+=(const char* o) { _s += o; return *this; }
    String& operator+=(char c) { _s += c; return *this; }
    friend String operator+(const String& a, const String& b) {
        return String((a._s + b._s).c_str());
    }
    friend String operator+(const String& a, const char* b) {
        return String((a._s + b).c_str());
    }
    friend String operator+(const char* a, const String& b) {
        return String((std::string(a) + b._s).c_str());
    }
    friend bool operator==(const String& a, const String& b) { return a._s == b._s; }
    friend bool operator==(const String& a, const char* b) { return a._s == b; }
    friend bool operator==(const char* a, const String& b) { return a == b._s; }
    friend bool operator!=(const String& a, const String& b) { return a._s != b._s; }
    friend bool operator!=(const String& a, const char* b) { return a._s != b; }
    friend bool operator<(const String& a, const String& b) { return a._s < b._s; }

private:
    std::string _s;
};

class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(const uint8_t* data, size_t size) = 0;
    virtual size_t write(uint8_t b) { return write(&b, 1); }
    size_t print(const char* s) { return write(reinterpret_cast<const uint8_t*>(s), strlen(s)); }
    size_t print(const String& s) { return write(reinterpret_cast<const uint8_t*>(s.c_str()), s.length()); }
    size_t print(char c) { return write(static_cast<uint8_t>(c)); }
    size_t print(int v) { return print(String(v)); }
    size_t print(unsigned v) { return print(String(v)); }
    size_t print(long v) { return print(String(v)); }
    size_t println() { return print("\r\n"); }
    template <typename T> size_t println(const T& v) {
        const size_t n = print(v);
        return n + println();
    }
};

class Stream : public Print {
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual size_t read(uint8_t* /*buffer*/, size_t /*size*/) { return 0; }
    virtual int peek() { return -1; }
    virtual void flush() {}
};

class HardwareSerial : public Stream {
public:
    HardwareSerial() = default;
    void begin(unsigned long baud, uint32_t config = 0) {
        (void)config;
        _baud = baud;
    }
    void end() {}
    void setRxBufferSize(size_t size) { _rxSize = size; }
    void setTxBufferSize(size_t size) { _txSize = size; }
    int available() override { return 0; }
    int read() override { return -1; }
    size_t read(uint8_t* /*buffer*/, size_t /*size*/) override { return 0; }
    size_t write(uint8_t) override { return 1; }
    size_t write(const uint8_t* /*data*/, size_t size) override { return size; }
    void flush() {}

private:
    unsigned long _baud = 0;
    size_t _rxSize = 0;
    size_t _txSize = 0;
};

extern HardwareSerial Serial;
