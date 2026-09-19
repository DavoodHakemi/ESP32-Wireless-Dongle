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
    _pendingConnection = PendingConnection::None;

    if (!_preferencesReady) {
        _preferencesReady = _preferences.begin("a2dp", false);
    }

    return _preferencesReady
        ? Result<void>::ok()
        : Result<void>::fail(ErrorCode::HardwareError);
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
    _audio.stop();

    portENTER_CRITICAL(&_callbackMux);
    _tone = false;
    _pendingConnectionState = 0xFF;
    _pendingAudioState = 0xFF;
    memset(_pendingDiscoveredAddress, 0, sizeof(_pendingDiscoveredAddress));
    _callbackCount = 0;
    _callbackBytes = 0;
    _lastCallbackMs = 0;
    _estimatedA2dpSampleRate = 44100;
    _rateMeasureStartMs = 0;
    _rateMeasureFrames = 0;
    _rateEstimatePending = false;
    _maxGapMs = 0;
    _stallCount = 0;
    _pcmStartedPending = false;
    _pcmStartedReported = false;
    _toneCommandMs = 0;
    _playbackResetPending = true;
    _toneReady = false;
    portEXIT_CRITICAL(&_callbackMux);

    _pendingConnection = PendingConnection::None;
    _pendingAddress = "";
    _pendingName = "";
    _connectionEventPending = false;
    _audioStartedPending = false;
    _audioStoppedPending = false;
    _classicFoundCallbackPending = false;
    _classicFoundEventPending = false;
    _disconnectCandidate = false;
    _disconnectHadAudio = false;
    _disconnectSince = 0;
    _mediaCheckPending = false;
    _mediaCheckDue = 0;
    _mediaAttempts = 0;
    _connectionState = 0xFF;
    _audioState = 0xFF;
    _lastStatsLogMs = 0;
    _shutdownReason = ShutdownReason::None;
    _shutdownFallbackName = "";
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
    if (!_preferencesReady) {
        return String();
    }
    return _preferences.getString("classic_mac", "");
}

String A2DPManager::cachedName() {
    if (!_preferencesReady) {
        return String();
    }
    return _preferences.getString("target_name", "");
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

    portENTER_CRITICAL(&self->_callbackMux);
    memcpy(self->_pendingDiscoveredAddress, address, sizeof(self->_pendingDiscoveredAddress));
    self->_classicFoundCallbackPending = true;
    portEXIT_CRITICAL(&self->_callbackMux);
    return true;
}

int32_t A2DPManager::frameCallback(Frame* frames, int32_t count) {
    auto* self = _callbackInstance;
    if (!self || !frames || count <= 0) {
        return 0;
    }

    const uint32_t now = millis();

    portENTER_CRITICAL(&self->_callbackMux);

    if (self->_rateMeasureStartMs == 0) {
        self->_rateMeasureStartMs = now;
    }
    self->_rateMeasureFrames += static_cast<uint32_t>(count);

    const uint32_t previous = self->_lastCallbackMs;

    if (previous != 0) {
        const uint32_t gap = now - previous;
        if (gap > self->_maxGapMs) {
            self->_maxGapMs = gap;
        }
        if (gap >= 100) {
            ++self->_stallCount;
        }
    }

    self->_lastCallbackMs = now;
    ++self->_callbackCount;
    self->_callbackBytes += static_cast<uint32_t>(count) * sizeof(Frame);

    if (static_cast<uint32_t>(now - self->_rateMeasureStartMs) >= 500 &&
        self->_rateMeasureFrames >= 1000) {
        const uint32_t elapsed = now - self->_rateMeasureStartMs;
        const uint32_t measured =
            (self->_rateMeasureFrames * 1000U + elapsed / 2U) / elapsed;

        uint16_t nearest = 44100;
        uint32_t bestError = 0xFFFFFFFFu;
        const uint16_t candidates[] = {16000, 32000, 44100, 48000};
        for (const uint16_t candidate : candidates) {
            const uint32_t error =
                measured > candidate
                ? measured - candidate
                : candidate - measured;
            if (error < bestError) {
                bestError = error;
                nearest = candidate;
            }
        }

        if (bestError <= nearest / 12U && nearest != self->_estimatedA2dpSampleRate) {
            self->_estimatedA2dpSampleRate = nearest;
            self->_rateEstimatePending = true;
            self->_playbackResetPending = true;
        }

        self->_rateMeasureStartMs = now;
        self->_rateMeasureFrames = 0;
    }

    portEXIT_CRITICAL(&self->_callbackMux);

    const bool filled = self->fillFrames(frames, count);
    if (filled) {
        portENTER_CRITICAL(&self->_callbackMux);
        if (!self->_pcmStartedReported) {
            self->_pcmStartedReported = true;
            self->_pcmStartedPending = true;
        }
        portEXIT_CRITICAL(&self->_callbackMux);
    }

    return count;
}

bool A2DPManager::fillFrames(Frame* frames, int32_t count) {
    if (!frames || count <= 0) {
        return false;
    }

    bool tone = false;
    bool toneReady = false;
    portENTER_CRITICAL(&_callbackMux);
    if (_playbackResetPending) {
        _audioBlockLoaded = false;
        _audioSampleIndex = 0;
        _audioResamplePhase = 0;
        _audioLeft = 0;
        _audioRight = 0;
        _toneIndex = 0;
        _playbackResetPending = false;
    }
    tone = _tone;
    toneReady = _toneReady;
    portEXIT_CRITICAL(&_callbackMux);

    if (tone) {
        if (!toneReady) {
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

    const AudioProfile profile = _audio.profile();
    uint16_t outputRate = 44100;
    portENTER_CRITICAL(&_callbackMux);
    outputRate = _estimatedA2dpSampleRate;
    portEXIT_CRITICAL(&_callbackMux);

    const uint32_t sourceRate = profile.sampleRate ? profile.sampleRate : 22050;
    if (outputRate == 0) {
        outputRate = 44100;
    }

    int32_t output = 0;

    while (output < count) {
        if (!_audioBlockLoaded) {
            if (!_audio.loadBlock(
                _audioBlock,
                _audio.blockBytes(),
                _audio.blockSamples())) {
                break;
            }
            _audioSampleIndex = 0;
            _audioResamplePhase = 0;
            _audioBlockLoaded = true;
        }

        if (_audioSampleIndex >= _audio.blockSamples()) {
            _audioBlockLoaded = false;
            continue;
        }

        const uint32_t offset =
            config::AUDIO_HEADER_BYTES +
            static_cast<uint32_t>(_audioSampleIndex) *
            profile.channels * 2U;

        _audioLeft = static_cast<int16_t>(
            static_cast<uint16_t>(_audioBlock[offset]) |
            (static_cast<uint16_t>(_audioBlock[offset + 1]) << 8));

        _audioRight =
            profile.channels == 2
            ? static_cast<int16_t>(
                static_cast<uint16_t>(_audioBlock[offset + 2]) |
                (static_cast<uint16_t>(_audioBlock[offset + 3]) << 8))
            : _audioLeft;

        frames[output].channel1 = _audioLeft;
        frames[output].channel2 = _audioRight;
        ++output;

        _audioResamplePhase += sourceRate;
        while (_audioResamplePhase >= outputRate) {
            _audioResamplePhase -= outputRate;
            ++_audioSampleIndex;
            if (_audioSampleIndex >= _audio.blockSamples()) {
                _audioBlockLoaded = false;
                break;
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
    portENTER_CRITICAL(&_callbackMux);
    _pendingConnectionState = static_cast<uint8_t>(state);
    portEXIT_CRITICAL(&_callbackMux);
}

void A2DPManager::handleAudio(esp_a2d_audio_state_t state) {
    portENTER_CRITICAL(&_callbackMux);
    _pendingAudioState = static_cast<uint8_t>(state);
    portEXIT_CRITICAL(&_callbackMux);
}

void A2DPManager::applyPendingCallbacks() {
    uint8_t connectionState = 0xFF;
    uint8_t audioState = 0xFF;
    bool classicFound = false;
    uint8_t discoveredAddress[6]{};

    portENTER_CRITICAL(&_callbackMux);
    connectionState = _pendingConnectionState;
    audioState = _pendingAudioState;
    _pendingConnectionState = 0xFF;
    _pendingAudioState = 0xFF;
    classicFound = _classicFoundCallbackPending;
    _classicFoundCallbackPending = false;
    memcpy(discoveredAddress, _pendingDiscoveredAddress, sizeof(discoveredAddress));
    portEXIT_CRITICAL(&_callbackMux);

    if (classicFound) {
        char text[18]{};
        snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
            discoveredAddress[0], discoveredAddress[1], discoveredAddress[2],
            discoveredAddress[3], discoveredAddress[4], discoveredAddress[5]);
        _remoteAddress = String(text);
        memcpy(_targetAddress, discoveredAddress, sizeof(_targetAddress));
        _classicFoundEventPending = true;
    }

    if (audioState != 0xFF) {
        _audioState = audioState;

        if (audioState == ESP_A2D_AUDIO_STATE_STARTED) {
            _state = ConnectionState::Streaming;
            _disconnectHadAudio = true;
            _disconnectCandidate = false;
            _mediaCheckPending = false;
            _audioStartedPending = true;
        }
        else if (audioState == ESP_A2D_AUDIO_STATE_STOPPED) {
            if (connectionState != ESP_A2D_CONNECTION_STATE_DISCONNECTED &&
                _state == ConnectionState::Streaming) {
                _state = ConnectionState::Connected;
            }
            _audioStoppedPending = true;
        }
    }

    if (connectionState != 0xFF) {
        _connectionState = connectionState;

        if (connectionState == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            _state = ConnectionState::Connected;
            _disconnectCandidate = false;
            _disconnectHadAudio = false;
            _connectionEventPending = true;
            _mediaCheckPending = true;
            _mediaCheckDue = millis() + config::A2DP_MEDIA_CHECK_DELAY_MS;
            _mediaAttempts = 0;
            _connectStart = 0;
        }
        else if (connectionState == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            _disconnectCandidate = true;
            _disconnectSince = millis();
            if (_state == ConnectionState::Streaming ||
                audioState == ESP_A2D_AUDIO_STATE_STARTED) {
                _disconnectHadAudio = true;
            }
        }
    }
}

bool A2DPManager::queueAddressConnection(
    const String& address,
    const uint8_t parsed[6],
    int retries) {
    _pendingConnection = PendingConnection::Address;
    _pendingAddress = address;
    _pendingName = "";
    memcpy(_pendingTargetAddress, parsed, sizeof(_pendingTargetAddress));
    _pendingRetries = retries > 0 ? retries : 1;
    return true;
}

bool A2DPManager::queueNameConnection(const String& name) {
    _pendingConnection = PendingConnection::Name;
    _pendingAddress = "";
    _pendingName = name;
    _pendingRetries = 0;
    return true;
}

Result<void> A2DPManager::connectByAddress(const String& address) {
    if (_state == ConnectionState::Connecting ||
        _state == ConnectionState::Connected ||
        _state == ConnectionState::Streaming ||
        _pendingConnection != PendingConnection::None ||
        _shutdownReason != ShutdownReason::None) {
        return Result<void>::fail(ErrorCode::Busy);
    }

    uint8_t parsed[6]{};
    if (!parseAddress(address, parsed)) {
        return Result<void>::fail(ErrorCode::InvalidArgument);
    }

    queueAddressConnection(address, parsed, 3);
    return Result<void>::ok();
}

Result<void> A2DPManager::connectByName(const String& name) {
    if (!name.length() || name.length() > 64) {
        return Result<void>::fail(ErrorCode::InvalidArgument);
    }

    if (_state == ConnectionState::Connecting ||
        _state == ConnectionState::Connected ||
        _state == ConnectionState::Streaming ||
        _pendingConnection != PendingConnection::None ||
        _shutdownReason != ShutdownReason::None) {
        return Result<void>::fail(ErrorCode::Busy);
    }

    queueNameConnection(name);
    return Result<void>::ok();
}

Result<void> A2DPManager::connectAuto() {
    if (_state == ConnectionState::Connecting ||
        _state == ConnectionState::Connected ||
        _state == ConnectionState::Streaming ||
        _pendingConnection != PendingConnection::None ||
        _shutdownReason != ShutdownReason::None) {
        return Result<void>::fail(ErrorCode::Busy);
    }

    String mac = cachedMac();
    String name = cachedName();
    if (!name.length()) {
        name = config::A2DP_DEFAULT_TARGET_NAME;
    }

    _logger.info(
        "A2DP auto target: cached_mac=%s, target_name=%s",
        mac.length() == 17 ? mac.c_str() : "(none)",
        name.length() ? name.c_str() : "(none)");

    if (mac.length() == 17) {
        uint8_t parsed[6]{};
        if (!parseAddress(mac, parsed)) {
            return Result<void>::fail(ErrorCode::InvalidArgument);
        }
        _pendingConnection = PendingConnection::Auto;
        _pendingAddress = mac;
        _pendingName = name;
        _autoMode = true;
        _fallbackName = name;
        _fallbackAttempted = false;
        memcpy(_pendingTargetAddress, parsed, sizeof(_pendingTargetAddress));
        // ESP32-A2DP v1.8.10 performs reconnect logic on heartbeat events.
        // Zero retries intentionally skips direct MAC retry and lets the
        // library fall back to its Classic name inquiry on the first heartbeat.
        _pendingRetries = 0;
        _logger.info("A2DP auto-connect queued for cached Classic MAC %s", mac.c_str());
        return Result<void>::ok();
    }

    _pendingConnection = PendingConnection::Auto;
    _pendingAddress = "";
    _pendingName = name;
    _pendingRetries = 0;
    _logger.info("A2DP auto-connect queued for Classic name '%s'", name.c_str());
    return Result<void>::ok();
}

void A2DPManager::processPendingConnection() {
    if (_pendingConnection == PendingConnection::None) {
        return;
    }

    const PendingConnection pending = _pendingConnection;
    const String address = _pendingAddress;
    const String name = _pendingName;
    const int retries = _pendingRetries;
    const uint8_t targetAddress[6] = {
        _pendingTargetAddress[0], _pendingTargetAddress[1],
        _pendingTargetAddress[2], _pendingTargetAddress[3],
        _pendingTargetAddress[4], _pendingTargetAddress[5]
    };

    _pendingConnection = PendingConnection::None;
    _pendingAddress = "";
    _pendingName = "";

    Result<void> result = Result<void>::ok();

    if (pending == PendingConnection::Address ||
        (pending == PendingConnection::Auto && address.length() == 17)) {
        _bluetooth.classic.suspendForA2dp();
        resetRuntimeState();

        _remoteAddress = address;
        _targetName = pending == PendingConnection::Auto ? name : "";
        _autoMode = pending == PendingConnection::Auto;
        _fallbackAttempted = false;
        _fallbackName = pending == PendingConnection::Auto ? name : "";
        memcpy(_targetAddress, targetAddress, sizeof(_targetAddress));

        configureCallbacks();
        _logger.info(
            "A2DP source start: address=%s, retries=%d",
            address.c_str(),
            retries >= 0 ? retries : 3);
        result = _source.startByAddress(targetAddress, retries >= 0 ? retries : 3);

        if (result.success) {
            _state = ConnectionState::Connecting;
            _connectStart = millis();
            publishAddressEvent(EventType::A2dpConnecting, _remoteAddress);
        }
    }
    else {
        _bluetooth.classic.suspendForA2dp();
        resetRuntimeState();

        _targetName = name;
        _remoteAddress = "";
        _autoMode = false;
        _fallbackAttempted = false;
        _fallbackName = "";

        configureCallbacks();
        _logger.info("A2DP source start: name='%s'", name.c_str());
        result = _source.startByName();

        if (result.success) {
            _state = ConnectionState::Connecting;
            _connectStart = millis();
        }
    }

    if (!result.success) {
        _logger.error(
            "A2DP connection start failed: error=%d",
            static_cast<int>(result.error));
        _events.publish({EventType::A2dpConnectFailed, nullptr, 0});
        _bluetooth.classic.restoreAfterA2dp();
    }
}

void A2DPManager::beginShutdown(
    ShutdownReason reason,
    const String& fallbackName) {
    if (reason == ShutdownReason::None ||
        _shutdownReason != ShutdownReason::None) {
        return;
    }

    _shutdownReason = reason;
    _shutdownFallbackName = fallbackName;
    _state = ConnectionState::Disconnecting;
    _connectStart = 0;
    _disconnectCandidate = false;
    _disconnectHadAudio = false;
    _mediaCheckPending = false;
    _pendingConnection = PendingConnection::None;
    _pendingAddress = "";
    _pendingName = "";

    _audio.stop();
    portENTER_CRITICAL(&_callbackMux);
    _tone = false;
    portEXIT_CRITICAL(&_callbackMux);

    _connectionState = ESP_A2D_CONNECTION_STATE_DISCONNECTED;
    _audioState = ESP_A2D_AUDIO_STATE_STOPPED;

    _source.beginStop();
}

bool A2DPManager::processShutdown() {
    if (_shutdownReason == ShutdownReason::None) {
        return false;
    }

    if (!_source.finishStop()) {
        return true;
    }

    _bluetooth.classic.restoreAfterA2dp();

    const ShutdownReason reason = _shutdownReason;
    const String fallbackName = _shutdownFallbackName;
    _shutdownReason = ShutdownReason::None;
    _shutdownFallbackName = "";

    _state = ConnectionState::Disconnected;
    _connectStart = 0;
    _disconnectCandidate = false;
    _disconnectHadAudio = false;
    _connectionState = ESP_A2D_CONNECTION_STATE_DISCONNECTED;
    _audioState = ESP_A2D_AUDIO_STATE_STOPPED;
    _audio.stop();

    if (fallbackName.length()) {
        _autoMode = false;
        _fallbackAttempted = true;
        _logger.info(
            "A2DP source stopped; starting Classic name fallback '%s'",
            fallbackName.c_str());
        queueNameConnection(fallbackName);
    }
    else if (reason == ShutdownReason::Disconnect) {
        publishAddressEvent(EventType::A2dpDisconnected, _remoteAddress);
    }
    else {
        _autoMode = false;
        _events.publish({EventType::A2dpConnectFailed, nullptr, 0});
    }

    return true;
}

Result<void> A2DPManager::startTone() {
    if (_state != ConnectionState::Connected &&
        _state != ConnectionState::Streaming) {
        return Result<void>::fail(ErrorCode::NotConnected);
    }

    prepareTone();
    portENTER_CRITICAL(&_callbackMux);
    _tone = true;
    _pcmStartedReported = false;
    _pcmStartedPending = false;
    _toneCommandMs = millis();
    portEXIT_CRITICAL(&_callbackMux);
    _logger.info("A2DP test tone enabled");
    return Result<void>::ok();
}

Result<void> A2DPManager::stopTone() {
    portENTER_CRITICAL(&_callbackMux);
    _tone = false;
    portEXIT_CRITICAL(&_callbackMux);
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

    portENTER_CRITICAL(&_callbackMux);
    _tone = false;
    _playbackResetPending = true;
    portEXIT_CRITICAL(&_callbackMux);

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
    portENTER_CRITICAL(&_callbackMux);
    _playbackResetPending = true;
    portEXIT_CRITICAL(&_callbackMux);
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
    if (_pendingConnection != PendingConnection::None) {
        _pendingConnection = PendingConnection::None;
        _pendingAddress = "";
        _pendingName = "";
        return Result<void>::ok();
    }

    if (_shutdownReason != ShutdownReason::None ||
        _state == ConnectionState::Disconnected) {
        return Result<void>::ok();
    }

    beginShutdown(ShutdownReason::Disconnect, "");
    return Result<void>::ok();
}

Status A2DPManager::status() const {
    Status result;
    portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&_callbackMux));
    result.tone = _tone;
    portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&_callbackMux));
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

    uint32_t callbackCount = 0;
    uint32_t callbackBytes = 0;
    uint32_t maxGapMs = 0;
    uint32_t stalls = 0;
    portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&_callbackMux));
    callbackCount = _callbackCount;
    callbackBytes = _callbackBytes;
    maxGapMs = _maxGapMs;
    stalls = _stallCount;
    portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&_callbackMux));

    AudioStatus result;
    result.streaming = buffer.streaming;
    result.primed = buffer.primed;
    result.profile = buffer.profile;
    result.used = buffer.used;
    result.capacity = buffer.capacity;
    result.received = buffer.received;
    result.dropped = buffer.dropped;
    result.underruns = buffer.underruns;
    result.callbackCount = callbackCount;
    result.callbackBytes = callbackBytes;
    result.maxGapMs = maxGapMs;
    result.stalls = stalls;
    return result;
}

void A2DPManager::update() {
    if (processShutdown()) {
        return;
    }

    processPendingConnection();
    applyPendingCallbacks();

    if (_classicFoundEventPending) {
        _classicFoundEventPending = false;
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
            _remoteAddress.length() ? _remoteAddress.c_str() : "UNKNOWN");
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

    bool pcmStartedPending = false;
    uint32_t toneCommandMs = 0;
    uint32_t callbackCount = 0;
    uint32_t callbackBytes = 0;
    uint32_t maxGapMs = 0;
    uint32_t stalls = 0;
    uint16_t estimatedRate = 44100;
    bool rateEstimatePending = false;

    portENTER_CRITICAL(&_callbackMux);
    pcmStartedPending = _pcmStartedPending;
    _pcmStartedPending = false;
    toneCommandMs = _toneCommandMs;
    callbackCount = _callbackCount;
    callbackBytes = _callbackBytes;
    maxGapMs = _maxGapMs;
    stalls = _stallCount;
    estimatedRate = _estimatedA2dpSampleRate;
    rateEstimatePending = _rateEstimatePending;
    _rateEstimatePending = false;
    portEXIT_CRITICAL(&_callbackMux);

    if (pcmStartedPending) {
        _logger.info(
            "A2DP PCM callback active; streaming started after %lu ms",
            static_cast<unsigned long>(toneCommandMs ? millis() - toneCommandMs : 0));
    }

    if (rateEstimatePending) {
        _logger.info(
            "A2DP measured output rate: %u Hz",
            static_cast<unsigned>(estimatedRate));
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
            String fallbackName;
            const bool fallback =
                _autoMode && !_fallbackAttempted && _fallbackName.length();

            if (fallback) {
                _fallbackAttempted = true;
                fallbackName = _fallbackName;
                _autoMode = false;
                beginShutdown(ShutdownReason::ConnectFailed, fallbackName);
            }
            else {
                _autoMode = false;
                beginShutdown(ShutdownReason::Disconnect, "");
            }

            return;
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

    bool toneEnabled = false;
    portENTER_CRITICAL(&_callbackMux);
    toneEnabled = _tone;
    portEXIT_CRITICAL(&_callbackMux);

    if (toneEnabled &&
        static_cast<uint32_t>(millis() - _lastStatsLogMs) >= 2000) {
        _lastStatsLogMs = millis();
        _logger.debug(
            "A2DP tone callback stats: callbacks=%lu bytes=%lu max_gap=%lums stalls=%lu",
            static_cast<unsigned long>(callbackCount),
            static_cast<unsigned long>(callbackBytes),
            static_cast<unsigned long>(maxGapMs),
            static_cast<unsigned long>(stalls));
    }

    if (_state == ConnectionState::Connecting &&
        _connectStart != 0 &&
        static_cast<uint32_t>(millis() - _connectStart) >=
        config::A2DP_CONNECT_TIMEOUT_MS) {
        const bool fallback =
            _autoMode && !_fallbackAttempted && _fallbackName.length();
        const String fallbackName = fallback ? _fallbackName : String();

        _logger.error(
            "A2DP connection timeout after %lu ms; stopping A2DP asynchronously",
            static_cast<unsigned long>(millis() - _connectStart));

        if (fallback) {
            _fallbackAttempted = true;
            _autoMode = false;
        }

        beginShutdown(ShutdownReason::ConnectFailed, fallbackName);
    }
}

}
// namespace dongle::services::a2dp
