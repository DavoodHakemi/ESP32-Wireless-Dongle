#include "services/a2dp/A2DPManager.h"

#include "config/AppConfig.h"
#include "core/Event.h"
#include "services/bluetooth/BluetoothManager.h"

#include <Arduino.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace dongle::services::a2dp {

A2DPManager* A2DPManager::_callbackInstance = nullptr;

A2DPManager::A2DPManager(
    ILogger& logger,
    IEventSink& events,
    services::bluetooth::BluetoothManager& bluetooth)
: _logger(logger), _events(events), _bluetooth(bluetooth) {
    _preferences.begin("a2dp", false);
    _callbackInstance = this;
}

A2DPManager::~A2DPManager() {
    _preferences.end();
    if (_callbackInstance == this) {
        _callbackInstance = nullptr;
    }
}

Result<void> A2DPManager::begin() {
    _state = ConnectionState::Disconnected;
    return Result<void>::ok();
}

bool A2DPManager::parseAddress(const String& text, uint8_t out[6]) const {
    if (!out || text.length() != 17) {
        return false;
    }

    int value[6]{};
    if (sscanf(text.c_str(),"%02x:%02x:%02x:%02x:%02x:%02x",
        &value[0], &value[1], &value[2],
        &value[3], &value[4], &value[5]) != 6) {
        return false;
    }

    for (int index = 0; index < 6; ++index) {
        if (value[index] < 0 || value[index] > 255) {
            return false;
        }
        out[index] = static_cast<uint8_t>(value[index]);
    }

    return true;
}

void A2DPManager::resetRuntimeState() {
    _tone = false;
    _audio.stop();
    _connectionEventPending = false;
    _audioStartedPending = false;
    _audioStoppedPending = false;
    _classicFoundPending = false;
    _disconnectCandidate = false;
    _disconnectHadAudio = false;
    _disconnectSince = 0;
    _mediaCheckPending = false;
    _mediaCheckDue = 0;
    _mediaAttempts = 0;
    _connectionState = 0xFF;
    _audioState = 0xFF;
    _callbackCount = 0;
    _callbackBytes = 0;
    _lastCallbackMs = 0;
    _maxGapMs = 0;
    _stallCount = 0;
    _lastStatsLogMs = 0;
    _pcmStartedPending = false;
    _pcmStartedReported = false;
    _toneCommandMs = 0;
    _audioBlockLoaded = false;
    _audioSampleIndex = 0;
    _audioRepeatPhase = 0;
    _toneIndex = 0;
}

void A2DPManager::configureCallbacks() {
    _source.configure(
        config::DEVICE_NAME,
        nameSelector,
        frameCallback,
        connectionCallback,
        audioCallback,
        this);
}

void A2DPManager::cacheTarget() {
    if (_remoteAddress.length() == 17) {
        _preferences.putString("classic_mac", _remoteAddress);
    }
    if (_targetName.length()) {
        _preferences.putString("target_name", _targetName);
    }
}

String A2DPManager::cachedMac() {
    return _preferences.getString("classic_mac","");
}

String A2DPManager::cachedName() {
    return _preferences.getString("target_name","");
}

void A2DPManager::clearCache() {
    _preferences.remove("classic_mac");
    _preferences.remove("target_name");
}

void A2DPManager::publishAddressEvent(EventType type, const String& address) {
    if (address.length() != 17) {
        return;
    }

    uint8_t payload[18]{};
    payload[0] = 17;
    memcpy(payload + 1, address.c_str(), 17);
    _events.publish({type, payload, sizeof(payload)});
}

void A2DPManager::prepareTone() {
    constexpr float PI2 = 6.2831853071795864769f;
    constexpr float AMPLITUDE = 10000.0f;

    for (uint16_t index = 0; index < config::A2DP_TONE_FRAMES; ++index) {
        const float phase =
        PI2 * config::A2DP_TONE_FREQUENCY_HZ * static_cast<float>(index) /
        static_cast<float>(config::A2DP_TONE_SAMPLE_RATE);
        const int16_t sample = static_cast<int16_t>(AMPLITUDE * sinf(phase));
        _toneBuffer[index].channel1 = sample;
        _toneBuffer[index].channel2 = sample;
    }

    _toneIndex = 0;
    _toneReady = true;
}

bool A2DPManager::nameSelector(
    const char* name,
    esp_bd_addr_t address,
    int/*rssi*/) {
    auto* self = _callbackInstance;
    if (!self || !name || !self->_targetName.length()) {
        return false;
    }

    if (strncmp(name, self->_targetName.c_str(), self->_targetName.length()) != 0) {
        return false;
    }

    char text[18]{};
    snprintf(text, sizeof(text),"%02X:%02X:%02X:%02X:%02X:%02X",
        address[0], address[1], address[2],
        address[3], address[4], address[5]);

    self->_remoteAddress = String(text);
    memcpy(self->_targetAddress, address, sizeof(self->_targetAddress));
    self->_classicFoundPending = true;
    return true;
}

int32_t A2DPManager::frameCallback(Frame* frames, int32_t count) {
    auto* self = _callbackInstance;
    if (!self || !frames || count <= 0) {
        return 0;
    }

    const uint32_t now = millis();
    const uint32_t previous = self->_lastCallbackMs;

    if (previous != 0) {
        const uint32_t gap = now - previous;
        if (gap > self->_maxGapMs) {
            self->_maxGapMs = gap;
        }
        if (gap >= 100) {
            __atomic_fetch_add(&self->_stallCount, 1U, __ATOMIC_RELAXED);
        }
    }

    self->_lastCallbackMs = now;
    __atomic_fetch_add(&self->_callbackCount, 1U, __ATOMIC_RELAXED);
    __atomic_fetch_add(&self->_callbackBytes, static_cast<uint32_t>(count) * sizeof(Frame), __ATOMIC_RELAXED);

    const bool filled = self->fillFrames(frames, count);
    if (filled && !self->_pcmStartedReported) {
        self->_pcmStartedReported = true;
        self->_pcmStartedPending = true;
    }

    return count;
}

bool A2DPManager::fillFrames(Frame* frames, int32_t count) {
    if (!frames || count <= 0) {
        return false;
    }

    if (_tone) {
        if (!_toneReady) {
            memset(frames, 0, static_cast<size_t>(count) * sizeof(Frame));
            return true;
        }

        int32_t remaining = count;
        Frame* destination = frames;

        while (remaining > 0) {
            const uint16_t available = static_cast<uint16_t>(
                config::A2DP_TONE_FRAMES - _toneIndex);
            const uint16_t chunk = static_cast<uint16_t>(
                remaining < available ? remaining : available);

            memcpy(
                destination,
                _toneBuffer + _toneIndex,
                static_cast<size_t>(chunk) * sizeof(Frame));

            destination += chunk;
            remaining -= chunk;
            _toneIndex = static_cast<uint16_t>(
                (_toneIndex + chunk) % config::A2DP_TONE_FRAMES);
        }

        return true;
    }

    if (!_audio.streaming()) {
        memset(frames, 0, static_cast<size_t>(count) * sizeof(Frame));
        return true;
    }

    int32_t output = 0;
    const uint8_t repeatCount =
    _audio.profile().sampleRate
    ? static_cast<uint8_t>(config::A2DP_TONE_SAMPLE_RATE /
        _audio.profile().sampleRate)
    : 1;

    while (output < count) {
        if (!_audioBlockLoaded) {
            if (!_audio.loadBlock(
                _audioBlock,
                _audio.blockBytes(),
                _audio.blockSamples())) {
                break;
            }
            _audioSampleIndex = 0;
            _audioRepeatPhase = 0;
            _audioBlockLoaded = true;
        }

        if (_audioSampleIndex >= _audio.blockSamples()) {
            _audioBlockLoaded = false;
            continue;
        }

        if (_audioRepeatPhase == 0) {
            const uint32_t offset =
            config::AUDIO_HEADER_BYTES +
            static_cast<uint32_t>(_audioSampleIndex) *
            _audio.profile().channels * 2U;

            _audioLeft = static_cast<int16_t>(
                static_cast<uint16_t>(_audioBlock[offset]) |
                (static_cast<uint16_t>(_audioBlock[offset + 1]) << 8));

            _audioRight =
            _audio.profile().channels == 2
            ? static_cast<int16_t>(
                static_cast<uint16_t>(_audioBlock[offset + 2]) |
                (static_cast<uint16_t>(_audioBlock[offset + 3]) << 8))
            : _audioLeft;
        }

        frames[output].channel1 = _audioLeft;
        frames[output].channel2 = _audioRight;
        ++output;

        if (_audio.profile().sampleRate == 44100) {
            _audioRepeatPhase = 0;
            ++_audioSampleIndex;
        }
        else {
            const uint8_t repetitions = repeatCount < 1 ? 1 : repeatCount;
            ++_audioRepeatPhase;
            if (_audioRepeatPhase >= repetitions) {
                _audioRepeatPhase = 0;
                ++_audioSampleIndex;
            }
        }
    }

    if (output < count) {
        memset(
            frames + output,
            0,
            static_cast<size_t>(count - output) * sizeof(Frame));
        _audio.onUnderrun();
    }

    return true;
}

void A2DPManager::connectionCallback(
    esp_a2d_connection_state_t state,
    void* context) {
    auto* self = static_cast<A2DPManager*>(context);
    if (self) {
        self->handleConnection(state);
    }
}

void A2DPManager::audioCallback(
    esp_a2d_audio_state_t state,
    void* context) {
    auto* self = static_cast<A2DPManager*>(context);
    if (self) {
        self->handleAudio(state);
    }
}

void A2DPManager::handleConnection(esp_a2d_connection_state_t state) {
    _connectionState = static_cast<uint8_t>(state);

    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        _state = ConnectionState::Connected;
        _disconnectCandidate = false;
        _disconnectHadAudio = false;
        _connectionEventPending = true;
        _mediaCheckPending = true;
        _mediaCheckDue = millis() + config::A2DP_MEDIA_CHECK_DELAY_MS;
        _mediaAttempts = 0;
        _connectStart = 0;
        return;
    }

    if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
        _disconnectCandidate = true;
        _disconnectSince = millis();
        _disconnectHadAudio = (_state == ConnectionState::Streaming);
    }
}

void A2DPManager::handleAudio(esp_a2d_audio_state_t state) {
    _audioState = static_cast<uint8_t>(state);

    if (state == ESP_A2D_AUDIO_STATE_STARTED) {
        _state = ConnectionState::Streaming;
        _disconnectHadAudio = true;
        _disconnectCandidate = false;
        _mediaCheckPending = false;
        _audioStartedPending = true;
    }
    else if (state == ESP_A2D_AUDIO_STATE_STOPPED) {
        if (_state == ConnectionState::Streaming) {
            _state = ConnectionState::Connected;
        }
        _audioStoppedPending = true;
    }
}

Result<void> A2DPManager::connectByAddress(const String& address) {
    if (_state == ConnectionState::Connecting ||
        _state == ConnectionState::Connected ||
        _state == ConnectionState::Streaming) {
        return Result<void>::fail(ErrorCode::Busy);
    }

    uint8_t parsed[6]{};
    if (!parseAddress(address, parsed)) {
        return Result<void>::fail(ErrorCode::InvalidArgument);
    }

    _bluetooth.classic.suspendForA2dp();
    resetRuntimeState();

    _remoteAddress = address;
    _targetName ="";
    _autoMode = false;
    _fallbackAttempted = false;
    _fallbackName ="";
    memcpy(_targetAddress, parsed, sizeof(_targetAddress));

    configureCallbacks();
    _source.startByAddress(parsed, 3);

    _state = ConnectionState::Connecting;
    _connectStart = millis();
    publishAddressEvent(EventType::A2dpConnecting, address);
    return Result<void>::ok();
}

Result<void> A2DPManager::connectByName(const String& name) {
    if (!name.length() || name.length() > 64) {
        return Result<void>::fail(ErrorCode::InvalidArgument);
    }

    if (_state == ConnectionState::Connecting ||
        _state == ConnectionState::Connected ||
        _state == ConnectionState::Streaming) {
        return Result<void>::fail(ErrorCode::Busy);
    }

    _bluetooth.classic.suspendForA2dp();
    resetRuntimeState();

    _targetName = name;
    _remoteAddress ="";
    _autoMode = false;
    _fallbackAttempted = false;
    _fallbackName ="";

    configureCallbacks();
    _source.startByName();

    _state = ConnectionState::Connecting;
    _connectStart = millis();
    return Result<void>::ok();
}

Result<void> A2DPManager::connectAuto() {
    String mac = cachedMac();
    String name = cachedName();
    if (!name.length()) {
        name = config::A2DP_DEFAULT_TARGET_NAME;
    }

    if (mac.length() == 17) {
        auto result = connectByAddress(mac);
        if (result.success) {
            _autoMode = true;
            _fallbackName = name;
            _fallbackAttempted = false;
        }
        return result;
    }

    return connectByName(name);
}

Result<void> A2DPManager::startTone() {
    if (_state != ConnectionState::Connected &&
        _state != ConnectionState::Streaming) {
        return Result<void>::fail(ErrorCode::NotConnected);
    }

    prepareTone();
    _tone = true;
    _pcmStartedReported = false;
    _toneCommandMs = millis();
    _logger.info("A2DP test tone enabled");
    return Result<void>::ok();
}

Result<void> A2DPManager::stopTone() {
    _tone = false;
    _logger.info("A2DP test tone stopped");
    return Result<void>::ok();
}

Result<void> A2DPManager::startAudio(const AudioProfile& profile) {
    if (_state != ConnectionState::Connected &&
        _state != ConnectionState::Streaming) {
        return Result<void>::fail(ErrorCode::NotConnected);
    }

    const bool supportedRate =
    profile.sampleRate == 11025 ||
    profile.sampleRate == 22050 ||
    profile.sampleRate == 44100;

    if (profile.bits != 16 ||
        profile.channels < 1 ||
        profile.channels > 2 ||
        !supportedRate) {
        return Result<void>::fail(ErrorCode::Unsupported);
    }

    _tone = false;

    const uint32_t blockBytes =
    static_cast<uint32_t>(config::AUDIO_HEADER_BYTES) +
    static_cast<uint32_t>(config::AUDIO_BLOCK_SAMPLES) *
    profile.channels * 2U;
    const uint32_t prebuffer =
    static_cast<uint32_t>(config::AUDIO_PREBUFFER_MULTIPLIER) * blockBytes;

    return _audio.start(profile, prebuffer)
    ? Result<void>::ok()
    : Result<void>::fail(ErrorCode::InternalError);
}

Result<void> A2DPManager::stopAudio() {
    _audio.stop();
    return Result<void>::ok();
}

Result<void> A2DPManager::pushAudioData(
    const uint8_t* payload,
    uint16_t length) {
    return _audio.pushPacket(payload, length)
    ? Result<void>::ok()
    : Result<void>::fail(ErrorCode::Busy);
}

Result<void> A2DPManager::disconnect() {
    if (_state == ConnectionState::Disconnected) {
        return Result<void>::ok();
    }

    _state = ConnectionState::Disconnecting;
    _tone = false;
    _audio.stop();
    _source.stop();
    _bluetooth.classic.restoreAfterA2dp();

    _state = ConnectionState::Disconnected;
    _connectionState = ESP_A2D_CONNECTION_STATE_DISCONNECTED;
    _audioState = ESP_A2D_AUDIO_STATE_STOPPED;
    publishAddressEvent(EventType::A2dpDisconnected, _remoteAddress);
    return Result<void>::ok();
}

Status A2DPManager::status() const {
    Status result;
    result.tone = _tone;
    result.active =
    _state == ConnectionState::Connected ||
    _state == ConnectionState::Streaming;
    result.connecting = _state == ConnectionState::Connecting;
    result.connectionState = _connectionState;
    result.audioState = _audioState;
    result.state = _state;
    result.target = _remoteAddress;
    return result;
}

AudioStatus A2DPManager::audioStatus() const {
    const auto buffer = _audio.status();

    AudioStatus result;
    result.streaming = buffer.streaming;
    result.primed = buffer.primed;
    result.profile = buffer.profile;
    result.used = buffer.used;
    result.capacity = buffer.capacity;
    result.received = buffer.received;
    result.dropped = buffer.dropped;
    result.underruns = buffer.underruns;
    result.callbackCount = _callbackCount;
    result.callbackBytes = _callbackBytes;
    result.maxGapMs = _maxGapMs;
    result.stalls = _stallCount;
    return result;
}

void A2DPManager::update() {
    if (_classicFoundPending) {
        _classicFoundPending = false;
        publishAddressEvent(EventType::A2dpClassicFound, _remoteAddress);
        publishAddressEvent(EventType::A2dpConnecting, _remoteAddress);
    }

    if (_connectionEventPending) {
        _connectionEventPending = false;
        _state = ConnectionState::Connected;

        if (_remoteAddress.length() == 17) {
            cacheTarget();
            publishAddressEvent(EventType::A2dpConnected, _remoteAddress);
        }

        _logger.info(
            "A2DP link connected: %s",
            _remoteAddress.length() ? _remoteAddress.c_str() :"UNKNOWN");
    }

    if (_audioStartedPending) {
        _audioStartedPending = false;
        _state = ConnectionState::Streaming;
        _disconnectCandidate = false;
        _events.publish({EventType::A2dpAudioStarted, nullptr, 0});
        _logger.info("A2DP media audio STARTED");
    }

    if (_audioStoppedPending) {
        _audioStoppedPending = false;
        _events.publish({EventType::A2dpAudioStopped, nullptr, 0});
    }

    if (_pcmStartedPending) {
        _pcmStartedPending = false;
        _logger.info(
            "A2DP PCM callback active; streaming started after %lu ms",
            static_cast<unsigned long>(
            _toneCommandMs ? millis() - _toneCommandMs : 0));
    }

    if (_audio.takeUnderrunFlag()) {
        uint8_t payload[8]{};
        const uint32_t underruns = _audio.underruns();
        const uint32_t used = _audio.used();
        memcpy(payload, &underruns, sizeof(underruns));
        memcpy(payload + sizeof(underruns), &used, sizeof(used));
        _events.publish({EventType::AudioUnderrun, payload, sizeof(payload)});
    }

    if (_disconnectCandidate &&
        static_cast<uint32_t>(millis() - _disconnectSince) >=
        config::A2DP_DISCONNECT_DEBOUNCE_MS) {
        _disconnectCandidate = false;

        if (!_disconnectHadAudio && _state != ConnectionState::Streaming) {
            _state = ConnectionState::Disconnected;
            _tone = false;
            _audio.stop();
            _connectionState = ESP_A2D_CONNECTION_STATE_DISCONNECTED;
            _audioState = ESP_A2D_AUDIO_STATE_STOPPED;
            _events.publish({EventType::A2dpDisconnected, nullptr, 0});
            _logger.warn("A2DP link disconnected: %s", _remoteAddress.c_str());

            if (_autoMode && !_fallbackAttempted && _fallbackName.length()) {
                _fallbackAttempted = true;
                const String fallbackName = _fallbackName;
                _autoMode = false;
                connectByName(fallbackName);
            }
        }
    }

    if (_mediaCheckPending &&
        _state == ConnectionState::Connected &&
        static_cast<int32_t>(millis() - _mediaCheckDue) >= 0) {
        const esp_err_t error = _source.checkMedia();

        _logger.info(
            "A2DP media check: err=%d, connection=%u, audio=%u",
            static_cast<int>(error),
            static_cast<unsigned>(_connectionState),
            static_cast<unsigned>(_audioState));

        if (error == ESP_OK ||
            _mediaAttempts >= config::A2DP_MEDIA_CHECK_RETRIES) {
            _mediaCheckPending = false;
        }
        else {
            ++_mediaAttempts;
            _mediaCheckDue = millis() + 1000;
        }
    }

    if (_tone &&
        static_cast<uint32_t>(millis() - _lastStatsLogMs) >= 2000) {
        _lastStatsLogMs = millis();
        _logger.debug(
            "A2DP tone callback stats: callbacks=%lu bytes=%lu max_gap=%lums stalls=%lu",
            static_cast<unsigned long>(_callbackCount),
            static_cast<unsigned long>(_callbackBytes),
            static_cast<unsigned long>(_maxGapMs),
            static_cast<unsigned long>(_stallCount));
    }

    if (_state == ConnectionState::Connecting &&
        _connectStart != 0 &&
        static_cast<uint32_t>(millis() - _connectStart) >=
        config::A2DP_CONNECT_TIMEOUT_MS) {
        _state = ConnectionState::Error;
        _logger.error("A2DP connection timeout");
        _events.publish({EventType::A2dpConnectFailed, nullptr, 0});
        _source.stop();
        _bluetooth.classic.restoreAfterA2dp();

        if (_autoMode && !_fallbackAttempted && _fallbackName.length()) {
            _fallbackAttempted = true;
            const String fallbackName = _fallbackName;
            _autoMode = false;
            connectByName(fallbackName);
        }
        else {
            _autoMode = false;
        }
    }
}

}
// namespace dongle::services::a2dp
