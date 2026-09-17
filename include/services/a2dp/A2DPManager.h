#pragma once
#include <Preferences.h>
#include <cstdint>
#include <freertos/portmacro.h>
#include "config/AppConfig.h"
#include "core/Result.h"
#include "core/Interfaces.h"
#include "services/a2dp/A2DPTypes.h"
#include "services/a2dp/A2DPSource.h"
#include "services/a2dp/AudioStreamBuffer.h"

namespace dongle::services::bluetooth {
class BluetoothManager;
}

namespace dongle::services::a2dp {
class A2DPManager final {
public:
    A2DPManager(ILogger& logger, IEventSink& events, services::bluetooth::BluetoothManager& bluetooth);
    ~A2DPManager();
    Result<void> begin();
    Result<void> connectByAddress(const String& address);
    Result<void> connectByName(const String& name);
    Result<void> connectAuto();
    Result<void> startTone();
    Result<void> stopTone();
    Result<void> disconnect();
    Result<void> startAudio(const AudioProfile& profile);
    Result<void> stopAudio();
    Result<void> pushAudioData(const uint8_t* payload, uint16_t length);
    Status status() const;
    AudioStatus audioStatus() const;
    String cachedMac(); String cachedName();
    void clearCache();
    void update();
private:
    ILogger& _logger;
    IEventSink& _events;
    services::bluetooth::BluetoothManager& _bluetooth;
    A2DPSourceAdapter _source;
    Preferences _preferences;
    AudioStreamBuffer _audio;
    ConnectionState _state{ConnectionState::Disconnected};
    bool _tone{false};
    bool _autoMode{false};
    bool _fallbackAttempted{false};
    String _fallbackName;
    String _targetName;
    String _remoteAddress;
    uint8_t _targetAddress[6]{};
    uint8_t _connectionState{0xFF};
    uint8_t _audioState{0xFF};
    bool _connectionEventPending{false};
    bool _audioStartedPending{false};
    bool _audioStoppedPending{false};
    bool _classicFoundPending{false};
    bool _disconnectCandidate{false};
    bool _disconnectHadAudio{false};
    uint32_t _disconnectSince{0};
    bool _mediaCheckPending{false};
    uint32_t _mediaCheckDue{0};
    uint8_t _mediaAttempts{0};
    uint32_t _connectStart{0};
    bool _pcmStartedPending{false};
    bool _pcmStartedReported{false};
    uint32_t _toneCommandMs{0};
    Frame _toneBuffer[config::A2DP_TONE_FRAMES]{};
    uint16_t _toneIndex{0};
    bool _toneReady{false};
    uint8_t _audioBlock[config::AUDIO_HEADER_BYTES + config::AUDIO_BLOCK_SAMPLES * 4]{};
    bool _audioBlockLoaded{false};
    uint16_t _audioSampleIndex{0};
    uint8_t _audioRepeatPhase{0};
    int16_t _audioLeft{0};
    int16_t _audioRight{0};
    volatile uint32_t _callbackCount{0};
    volatile uint32_t _callbackBytes{0};
    volatile uint32_t _lastCallbackMs{0};
    volatile uint32_t _maxGapMs{0};
    volatile uint32_t _stallCount{0};
    uint32_t _lastStatsLogMs{0};

    static A2DPManager* _callbackInstance;
    static bool nameSelector(const char*, esp_bd_addr_t, int);
    static int32_t frameCallback(Frame*, int32_t);
    static void connectionCallback(esp_a2d_connection_state_t, void*);
    static void audioCallback(esp_a2d_audio_state_t, void*);
    bool parseAddress(const String&, uint8_t out[6]) const;
    void configureCallbacks();
    void prepareTone();
    bool fillFrames(Frame*, int32_t);
    void handleConnection(esp_a2d_connection_state_t);
    void handleAudio(esp_a2d_audio_state_t);
    void applyPendingCallbacks();
    void publishAddressEvent(EventType type, const String& address);
    void resetRuntimeState();

    // Callback-owned notification state. Callbacks only record state here;
    // the application task consumes it from applyPendingCallbacks().
    uint8_t _pendingConnectionState{0xFF};
    uint8_t _pendingAudioState{0xFF};
    uint8_t _pendingDiscoveredAddress[6]{};
    mutable portMUX_TYPE _callbackMux = portMUX_INITIALIZER_UNLOCKED;

    enum class PendingConnection : uint8_t {
        None,
        Address,
        Name,
        Auto
    };

    void cacheTarget();
    bool queueAddressConnection(const String& address, const uint8_t parsed[6], int retries);
    bool queueNameConnection(const String& name);
    void processPendingConnection();
    bool _preferencesReady{false};
    PendingConnection _pendingConnection{PendingConnection::None};
    String _pendingAddress;
    String _pendingName;
    uint8_t _pendingTargetAddress[6]{};
    int _pendingRetries{3};
};
}
