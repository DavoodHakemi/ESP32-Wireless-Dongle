#include "services/a2dp/AudioStreamBuffer.h"
#include <Arduino.h>
#include <cstring>

namespace dongle::services::a2dp {
uint32_t AudioStreamBuffer::usedUnsafe()const{
    return _write>=_read?_write-_read:CAPACITY-_read+_write;
}
uint32_t AudioStreamBuffer::used()const{
    portENTER_CRITICAL(&_mux);
    const uint32_t v=usedUnsafe();
    portEXIT_CRITICAL(&_mux);
    return v;
}
uint32_t AudioStreamBuffer::freeBytes()const{
    return (CAPACITY-1)-used();
}
bool AudioStreamBuffer::start(const AudioProfile&profile, uint32_t prebufferBytes) {
    _profile=profile;
    _prebuffer=prebufferBytes>CAPACITY-1?CAPACITY-1:prebufferBytes;
    portENTER_CRITICAL(&_mux);
    _read=_write=0;
    portEXIT_CRITICAL(&_mux);
    portENTER_CRITICAL(&_mux);
    _received=_dropped=_underruns=0;
    _underrunPending=false;
    portEXIT_CRITICAL(&_mux);
    _running=true;
    _primed=false;
    return true;
}
void AudioStreamBuffer::stop() {
    _running=false;
    _primed=false;
    portENTER_CRITICAL(&_mux);
    _read=_write=0;
    portEXIT_CRITICAL(&_mux);
}
void AudioStreamBuffer::onUnderrun() {
    portENTER_CRITICAL(&_mux);
    ++_underruns;
    _underrunPending=true;
    portEXIT_CRITICAL(&_mux);
}
bool AudioStreamBuffer::takeUnderrunFlag() {
    portENTER_CRITICAL(&_mux);
    const bool pending=_underrunPending;
    _underrunPending=false;
    portEXIT_CRITICAL(&_mux);
    return pending;
}
uint32_t AudioStreamBuffer::received() const{
    portENTER_CRITICAL(&_mux);
    const uint32_t v=_received;
    portEXIT_CRITICAL(&_mux);
    return v;
}
uint32_t AudioStreamBuffer::dropped() const{
    portENTER_CRITICAL(&_mux);
    const uint32_t v=_dropped;
    portEXIT_CRITICAL(&_mux);
    return v;
}
uint32_t AudioStreamBuffer::underruns() const{
    portENTER_CRITICAL(&_mux);
    const uint32_t v=_underruns;
    portEXIT_CRITICAL(&_mux);
    return v;
}
bool AudioStreamBuffer::pushPacket(const uint8_t*p, uint16_t n) {
    if (!_running||!p||n!=blockBytes()||n>=CAPACITY)return false;
    bool ok=false;
    portENTER_CRITICAL(&_mux);
    const uint32_t used=usedUnsafe();
    const uint32_t free=(CAPACITY-1)-used;
    if (free>=n) {
        const uint32_t first=CAPACITY-_write;
        const uint32_t a=first<n?first:n;
        memcpy(_buffer+_write, p, a);
        if (n>a)memcpy(_buffer, p+a, n-a);
        _write=(_write+n)%CAPACITY;
        ok=true;
    }
    _primed=_primed||(usedUnsafe()>=_prebuffer);
    portEXIT_CRITICAL(&_mux);
    portENTER_CRITICAL(&_mux);
    if (ok)++_received;
    else++_dropped;
    portEXIT_CRITICAL(&_mux);
    return ok;
}
bool AudioStreamBuffer::loadBlock(uint8_t*out, uint16_t n, uint16_t expectedSamples) {
    if (!_primed||!out||n>CAPACITY)return false;
    bool ok=false;
    portENTER_CRITICAL(&_mux);
    if (usedUnsafe()>=n) {
        const uint32_t first=CAPACITY-_read;
        const uint32_t a=first<n?first:n;
        memcpy(out, _buffer+_read, a);
        if (n>a)memcpy(out+a, _buffer, n-a);
        _read=(_read+n)%CAPACITY;
        ok=true;
    }
    portEXIT_CRITICAL(&_mux);
    if (!ok)return false;
    const uint16_t samples=static_cast<uint16_t>(out[1])|(static_cast<uint16_t>(out[2])<<8);
    return out[0]==0x50&&samples==expectedSamples;
}
AudioBufferStatus AudioStreamBuffer::status()const{
    AudioBufferStatus s;
    s.streaming=_running;
    s.primed=_primed;
    s.profile=_profile;
    s.used=used();
    s.capacity=CAPACITY-1;
    s.received=_received;
    s.dropped=_dropped;
    s.underruns=_underruns;
    return s;
}
}
