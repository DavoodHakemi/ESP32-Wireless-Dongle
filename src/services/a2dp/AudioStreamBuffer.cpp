#include "services/a2dp/AudioStreamBuffer.h"
#include <Arduino.h>
#include <cstring>

namespace dongle::services::a2dp {

uint32_t AudioStreamBuffer::usedUnsafe() const {
    return _write >= _read ? _write - _read : CAPACITY - _read + _write;
}

uint32_t AudioStreamBuffer::used() const {
    portENTER_CRITICAL(&_mux);
    const uint32_t value = usedUnsafe();
    portEXIT_CRITICAL(&_mux);
    return value;
}

uint32_t AudioStreamBuffer::freeBytes() const {
    portENTER_CRITICAL(&_mux);
    const uint32_t value = (CAPACITY - 1) - usedUnsafe();
    portEXIT_CRITICAL(&_mux);
    return value;
}

bool AudioStreamBuffer::primed() const {
    portENTER_CRITICAL(&_mux);
    const bool value = _primed;
    portEXIT_CRITICAL(&_mux);
    return value;
}

bool AudioStreamBuffer::streaming() const {
    portENTER_CRITICAL(&_mux);
    const bool value = _running;
    portEXIT_CRITICAL(&_mux);
    return value;
}

AudioProfile AudioStreamBuffer::profile() const {
    portENTER_CRITICAL(&_mux);
    const AudioProfile value = _profile;
    portEXIT_CRITICAL(&_mux);
    return value;
}

uint16_t AudioStreamBuffer::blockBytes() const {
    portENTER_CRITICAL(&_mux);
    const uint16_t value = static_cast<uint16_t>(
        config::AUDIO_HEADER_BYTES +
        config::AUDIO_BLOCK_SAMPLES * _profile.channels * 2U);
    portEXIT_CRITICAL(&_mux);
    return value;
}

bool AudioStreamBuffer::start(
    const AudioProfile& profile,
    uint32_t prebufferBytes) {
    portENTER_CRITICAL(&_mux);
    _profile = profile;
    _prebuffer = prebufferBytes > CAPACITY - 1
        ? CAPACITY - 1
        : prebufferBytes;
    _read = 0;
    _write = 0;
    _received = 0;
    _dropped = 0;
    _underruns = 0;
    _underrunPending = false;
    _running = true;
    _primed = false;
    portEXIT_CRITICAL(&_mux);
    return true;
}

void AudioStreamBuffer::stop() {
    portENTER_CRITICAL(&_mux);
    _running = false;
    _primed = false;
    _read = 0;
    _write = 0;
    _underrunPending = false;
    portEXIT_CRITICAL(&_mux);
}

void AudioStreamBuffer::onUnderrun() {
    portENTER_CRITICAL(&_mux);
    ++_underruns;
    _underrunPending = true;
    portEXIT_CRITICAL(&_mux);
}

bool AudioStreamBuffer::takeUnderrunFlag() {
    portENTER_CRITICAL(&_mux);
    const bool pending = _underrunPending;
    _underrunPending = false;
    portEXIT_CRITICAL(&_mux);
    return pending;
}

uint32_t AudioStreamBuffer::received() const {
    portENTER_CRITICAL(&_mux);
    const uint32_t value = _received;
    portEXIT_CRITICAL(&_mux);
    return value;
}

uint32_t AudioStreamBuffer::dropped() const {
    portENTER_CRITICAL(&_mux);
    const uint32_t value = _dropped;
    portEXIT_CRITICAL(&_mux);
    return value;
}

uint32_t AudioStreamBuffer::underruns() const {
    portENTER_CRITICAL(&_mux);
    const uint32_t value = _underruns;
    portEXIT_CRITICAL(&_mux);
    return value;
}

bool AudioStreamBuffer::pushPacket(const uint8_t* payload, uint16_t length) {
    if (!payload) {
        return false;
    }

    bool ok = false;
    portENTER_CRITICAL(&_mux);

    const uint16_t expectedLength = static_cast<uint16_t>(
        config::AUDIO_HEADER_BYTES +
        config::AUDIO_BLOCK_SAMPLES * _profile.channels * 2U);

    if (_running &&
        length == expectedLength &&
        length < CAPACITY) {
        const uint32_t used = usedUnsafe();
        const uint32_t free = (CAPACITY - 1) - used;

        if (free >= length) {
            const uint32_t first = CAPACITY - _write;
            const uint32_t firstLength = first < length ? first : length;
            memcpy(_buffer + _write, payload, firstLength);
            if (length > firstLength) {
                memcpy(_buffer, payload + firstLength, length - firstLength);
            }
            _write = (_write + length) % CAPACITY;
            ok = true;
        }

        if (usedUnsafe() >= _prebuffer) {
            _primed = true;
        }
    }

    if (ok) {
        ++_received;
    }
    else {
        ++_dropped;
    }

    portEXIT_CRITICAL(&_mux);
    return ok;
}

bool AudioStreamBuffer::loadBlock(
    uint8_t* out,
    uint16_t length,
    uint16_t expectedSamples) {
    if (!out || length > CAPACITY) {
        return false;
    }

    bool ok = false;
    portENTER_CRITICAL(&_mux);

    if (_running && _primed && usedUnsafe() >= length) {
        const uint32_t first = CAPACITY - _read;
        const uint32_t firstLength = first < length ? first : length;
        memcpy(out, _buffer + _read, firstLength);
        if (length > firstLength) {
            memcpy(out + firstLength, _buffer, length - firstLength);
        }
        _read = (_read + length) % CAPACITY;
        ok = true;
    }

    portEXIT_CRITICAL(&_mux);

    if (!ok) {
        return false;
    }

    const uint16_t samples =
        static_cast<uint16_t>(out[1]) |
        (static_cast<uint16_t>(out[2]) << 8);

    return out[0] == 0x50 && samples == expectedSamples;
}

AudioBufferStatus AudioStreamBuffer::status() const {
    AudioBufferStatus result;

    portENTER_CRITICAL(&_mux);
    result.streaming = _running;
    result.primed = _primed;
    result.profile = _profile;
    result.used = usedUnsafe();
    result.capacity = CAPACITY - 1;
    result.received = _received;
    result.dropped = _dropped;
    result.underruns = _underruns;
    portEXIT_CRITICAL(&_mux);

    return result;
}

}
