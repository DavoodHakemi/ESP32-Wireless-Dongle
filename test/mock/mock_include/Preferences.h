// Mock Preferences.h — NVS subset used by A2DPManager.
#pragma once

#include "Arduino.h"

class Preferences {
public:
    bool begin(const char* name, bool readOnly = false) {
        (void)name;
        (void)readOnly;
        return true;
    }
    void end() {}
    bool clear() { return true; }
    bool remove(const char* key) { (void)key; return true; }
    size_t putString(const char* key, const String& value) {
        (void)key;
        (void)value;
        return value.length();
    }
    String getString(const char* key, const String& fallback = String()) {
        (void)key;
        return fallback;
    }
    bool getBool(const char* key, bool fallback = false) {
        (void)key;
        return fallback;
    }
    size_t putBool(const char* key, bool value) {
        (void)key;
        (void)value;
        return 1;
    }
};
